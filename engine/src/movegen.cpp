#include "movegen.h"

namespace {

void add_pawn_move(MoveList& list, int from, int to, bool capture) {
  int rank = rank_of(to);
  if (rank == 7 || rank == 0) {
    int base = capture ? PROMO_CAPTURE_N : PROMO_N;
    for (int i = 3; i >= 0; i--) list.add(make_move(from, to, base + i));  // queen first
  } else {
    list.add(make_move(from, to, capture ? CAPTURE : QUIET));
  }
}

}  // namespace

void generate_pseudo_legal(const Position& pos, MoveList& list, bool captures_only) {
  const Color us = pos.side_to_move(), them = ~us;
  const Bitboard own = pos.occupied(us), enemy = pos.occupied(them), occ = own | enemy;
  const int push = us == WHITE ? 8 : -8;
  const int start_rank = us == WHITE ? 1 : 6;
  const int promo_rank = us == WHITE ? 6 : 1;  // rank a pawn is on just before promoting

  // Pawns
  Bitboard pawns = pos.pieces(us, PAWN);
  while (pawns) {
    int from = pop_lsb(pawns);
    int to = from + push;
    bool promoting = rank_of(from) == promo_rank;
    if (!(occ & bit(to)) && (!captures_only || promoting)) {
      add_pawn_move(list, from, to, false);
      if (rank_of(from) == start_rank && !captures_only && !(occ & bit(to + push)))
        list.add(make_move(from, to + push, DOUBLE_PUSH));
    }
    Bitboard caps = pawn_attacks_table[us][from] & enemy;
    while (caps) add_pawn_move(list, from, pop_lsb(caps), true);
    if (pos.ep_square() != NO_SQUARE && (pawn_attacks_table[us][from] & bit(pos.ep_square())))
      list.add(make_move(from, pos.ep_square(), EN_PASSANT));
  }

  // Knights, bishops, rooks, queens, king
  const Bitboard targets = captures_only ? enemy : ~own;
  for (int type = KNIGHT; type <= KING; type++) {
    Bitboard pcs = pos.pieces(us, PieceType(type));
    while (pcs) {
      int from = pop_lsb(pcs);
      Bitboard att;
      switch (type) {
        case KNIGHT: att = knight_attacks_table[from]; break;
        case BISHOP: att = bishop_attacks(from, occ); break;
        case ROOK: att = rook_attacks(from, occ); break;
        case QUEEN: att = queen_attacks(from, occ); break;
        default: att = king_attacks_table[from]; break;
      }
      att &= targets;
      while (att) {
        int to = pop_lsb(att);
        list.add(make_move(from, to, (enemy & bit(to)) ? CAPTURE : QUIET));
      }
    }
  }

  // Castling: squares between king and rook empty, and the king may not start in, pass through or land in check.
  if (!captures_only) {
    const int rights = pos.castling_rights();
    if (us == WHITE) {
      if ((rights & WHITE_OO) && !(occ & (bit(F1) | bit(G1))) && !pos.attacked(E1, them) && !pos.attacked(F1, them) &&
          !pos.attacked(G1, them))
        list.add(make_move(E1, G1, KING_CASTLE));
      if ((rights & WHITE_OOO) && !(occ & (bit(D1) | bit(C1) | bit(B1))) && !pos.attacked(E1, them) &&
          !pos.attacked(D1, them) && !pos.attacked(C1, them))
        list.add(make_move(E1, C1, QUEEN_CASTLE));
    } else {
      if ((rights & BLACK_OO) && !(occ & (bit(F8) | bit(G8))) && !pos.attacked(E8, them) && !pos.attacked(F8, them) &&
          !pos.attacked(G8, them))
        list.add(make_move(E8, G8, KING_CASTLE));
      if ((rights & BLACK_OOO) && !(occ & (bit(D8) | bit(C8) | bit(B8))) && !pos.attacked(E8, them) &&
          !pos.attacked(D8, them) && !pos.attacked(C8, them))
        list.add(make_move(E8, C8, QUEEN_CASTLE));
    }
  }
}

void generate_legal(Position& pos, MoveList& list) {
  MoveList pseudo;
  generate_pseudo_legal(pos, pseudo);
  for (Move m : pseudo) {
    pos.make(m);
    if (!pos.left_king_in_check()) list.add(m);
    pos.unmake();
  }
}

uint64_t perft(Position& pos, int depth) {
  if (depth == 0) return 1;
  MoveList pseudo;
  generate_pseudo_legal(pos, pseudo);
  uint64_t nodes = 0;
  for (Move m : pseudo) {
    pos.make(m);
    if (!pos.left_king_in_check()) nodes += depth == 1 ? 1 : perft(pos, depth - 1);
    pos.unmake();
  }
  return nodes;
}

Move parse_uci_move(Position& pos, const std::string& uci) {
  MoveList legal;
  generate_legal(pos, legal);
  for (Move m : legal)
    if (move_to_uci(m) == uci) return m;
  return NO_MOVE;
}

GameResult game_result(Position& pos) {
  MoveList legal;
  generate_legal(pos, legal);
  if (legal.count == 0) return pos.in_check() ? GameResult::CHECKMATE : GameResult::STALEMATE;
  if (pos.is_draw()) return GameResult::DRAW;
  return GameResult::ONGOING;
}
