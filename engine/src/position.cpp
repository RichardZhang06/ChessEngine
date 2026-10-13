#include "position.h"

#include <cctype>
#include <cstring>
#include <sstream>

namespace {

Key zobrist_piece[12][64];
Key zobrist_castling[16];
Key zobrist_ep_file[8];
Key zobrist_side;

// Squares whose movement (from or to) removes castling rights.
int castling_mask[64];

uint64_t rng_state = 0x9E3779B97F4A7C15ULL;
uint64_t next_random() {  // xorshift64*, fixed seed so hashes are reproducible
  rng_state ^= rng_state >> 12;
  rng_state ^= rng_state << 25;
  rng_state ^= rng_state >> 27;
  return rng_state * 0x2545F4914F6CDD1DULL;
}

const char* PIECE_CHARS = "PNBRQKpnbrqk";

}  // namespace

void init_zobrist() {
  static bool done = false;
  if (done) return;
  done = true;
  for (auto& row : zobrist_piece)
    for (auto& k : row) k = next_random();
  for (auto& k : zobrist_castling) k = next_random();
  for (auto& k : zobrist_ep_file) k = next_random();
  zobrist_side = next_random();

  for (int& m : castling_mask) m = 15;
  castling_mask[E1] &= ~(WHITE_OO | WHITE_OOO);
  castling_mask[H1] &= ~WHITE_OO;
  castling_mask[A1] &= ~WHITE_OOO;
  castling_mask[E8] &= ~(BLACK_OO | BLACK_OOO);
  castling_mask[H8] &= ~BLACK_OO;
  castling_mask[A8] &= ~BLACK_OOO;
}

Position::Position() {
  init_bitboards();
  init_zobrist();
  set_startpos();
}

Position::Position(bool) : side_(WHITE), castling_(0), ep_(NO_SQUARE), halfmove_(0), fullmove_(1), key_(0) {
  for (int& b : board_) b = NO_PIECE;
  for (Bitboard& b : pieces_) b = 0;
  occ_[WHITE] = occ_[BLACK] = 0;
}

void Position::set_startpos() {
  set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

void Position::put_piece(int piece, int sq) {
  board_[sq] = piece;
  pieces_[piece] |= bit(sq);
  occ_[color_of(piece)] |= bit(sq);
}

void Position::remove_piece(int sq) {
  int piece = board_[sq];
  pieces_[piece] &= ~bit(sq);
  occ_[color_of(piece)] &= ~bit(sq);
  board_[sq] = NO_PIECE;
}

void Position::move_piece(int from, int to) {
  int piece = board_[from];
  remove_piece(from);
  put_piece(piece, to);
}

Key Position::compute_key() const {
  Key k = 0;
  for (int sq = 0; sq < 64; sq++)
    if (board_[sq] != NO_PIECE) k ^= zobrist_piece[board_[sq]][sq];
  k ^= zobrist_castling[castling_];
  if (ep_ != NO_SQUARE) k ^= zobrist_ep_file[file_of(ep_)];
  if (side_ == BLACK) k ^= zobrist_side;
  return k;
}

bool Position::set_fen(const std::string& fen) {
  std::istringstream in(fen);
  std::string placement, side, castling, ep;
  int halfmove = 0, fullmove = 1;
  if (!(in >> placement >> side)) return false;
  if (!(in >> castling)) castling = "-";
  if (!(in >> ep)) ep = "-";
  if (!(in >> halfmove)) halfmove = 0;
  if (!(in >> fullmove)) fullmove = 1;

  Position next(false);  // build into a scratch copy so a bad FEN leaves *this untouched

  int rank = 7, file = 0;
  for (char c : placement) {
    if (c == '/') {
      if (file != 8 || rank == 0) return false;
      rank--;
      file = 0;
    } else if (std::isdigit(static_cast<unsigned char>(c))) {
      file += c - '0';
      if (file > 8) return false;
    } else {
      const char* p = std::strchr(PIECE_CHARS, c);
      if (!p || file > 7) return false;
      next.put_piece(int(p - PIECE_CHARS), make_square(file, rank));
      file++;
    }
  }
  if (rank != 0 || file != 8) return false;
  if (popcount(next.pieces_[make_piece(WHITE, KING)]) != 1 || popcount(next.pieces_[make_piece(BLACK, KING)]) != 1)
    return false;

  if (side == "w") next.side_ = WHITE;
  else if (side == "b") next.side_ = BLACK;
  else return false;

  next.castling_ = 0;
  if (castling != "-") {
    for (char c : castling) {
      switch (c) {
        case 'K': next.castling_ |= WHITE_OO; break;
        case 'Q': next.castling_ |= WHITE_OOO; break;
        case 'k': next.castling_ |= BLACK_OO; break;
        case 'q': next.castling_ |= BLACK_OOO; break;
        default: return false;
      }
    }
  }
  // Drop rights that don't match the pieces actually on the board.
  if (next.board_[E1] != make_piece(WHITE, KING)) next.castling_ &= ~(WHITE_OO | WHITE_OOO);
  if (next.board_[H1] != make_piece(WHITE, ROOK)) next.castling_ &= ~WHITE_OO;
  if (next.board_[A1] != make_piece(WHITE, ROOK)) next.castling_ &= ~WHITE_OOO;
  if (next.board_[E8] != make_piece(BLACK, KING)) next.castling_ &= ~(BLACK_OO | BLACK_OOO);
  if (next.board_[H8] != make_piece(BLACK, ROOK)) next.castling_ &= ~BLACK_OO;
  if (next.board_[A8] != make_piece(BLACK, ROOK)) next.castling_ &= ~BLACK_OOO;

  next.ep_ = NO_SQUARE;
  if (ep != "-") {
    if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' || (ep[1] != '3' && ep[1] != '6')) return false;
    int sq = make_square(ep[0] - 'a', ep[1] - '1');
    // Only keep the square if a pawn could actually capture there (keeps hashes of equal positions equal).
    if (pawn_attacks_table[~next.side_][sq] & next.pieces_[make_piece(next.side_, PAWN)]) next.ep_ = sq;
  }
  next.halfmove_ = halfmove;
  next.fullmove_ = fullmove;
  next.key_ = next.compute_key();
  next.history_.clear();

  // Reject positions where the side NOT to move is in check (impossible to reach legally).
  if (next.attacked(next.king_square(~next.side_), next.side_)) return false;

  *this = next;
  return true;
}

std::string Position::fen() const {
  std::string s;
  for (int rank = 7; rank >= 0; rank--) {
    int empty = 0;
    for (int file = 0; file < 8; file++) {
      int p = board_[make_square(file, rank)];
      if (p == NO_PIECE) {
        empty++;
        continue;
      }
      if (empty) s += char('0' + empty);
      empty = 0;
      s += PIECE_CHARS[p];
    }
    if (empty) s += char('0' + empty);
    if (rank) s += '/';
  }
  s += side_ == WHITE ? " w " : " b ";
  if (castling_ == 0) s += '-';
  if (castling_ & WHITE_OO) s += 'K';
  if (castling_ & WHITE_OOO) s += 'Q';
  if (castling_ & BLACK_OO) s += 'k';
  if (castling_ & BLACK_OOO) s += 'q';
  s += ' ' + (ep_ == NO_SQUARE ? std::string("-") : square_name(ep_));
  s += ' ' + std::to_string(halfmove_) + ' ' + std::to_string(fullmove_);
  return s;
}

std::string Position::to_string() const {
  std::string s;
  for (int rank = 7; rank >= 0; rank--) {
    for (int file = 0; file < 8; file++) {
      int p = board_[make_square(file, rank)];
      s += p == NO_PIECE ? '.' : PIECE_CHARS[p];
      s += ' ';
    }
    s += '\n';
  }
  return s + "fen: " + fen() + "\n";
}

Bitboard Position::attackers_to(int sq, Bitboard occ) const {
  return (pawn_attacks_table[BLACK][sq] & pieces_[make_piece(WHITE, PAWN)]) |
         (pawn_attacks_table[WHITE][sq] & pieces_[make_piece(BLACK, PAWN)]) |
         (knight_attacks_table[sq] & pieces(KNIGHT)) | (king_attacks_table[sq] & pieces(KING)) |
         (bishop_attacks(sq, occ) & (pieces(BISHOP) | pieces(QUEEN))) |
         (rook_attacks(sq, occ) & (pieces(ROOK) | pieces(QUEEN)));
}

bool Position::attacked(int sq, Color by) const {
  Bitboard occ = occupied();
  if (pawn_attacks_table[~by][sq] & pieces_[make_piece(by, PAWN)]) return true;
  if (knight_attacks_table[sq] & pieces_[make_piece(by, KNIGHT)]) return true;
  if (king_attacks_table[sq] & pieces_[make_piece(by, KING)]) return true;
  if (bishop_attacks(sq, occ) & (pieces_[make_piece(by, BISHOP)] | pieces_[make_piece(by, QUEEN)])) return true;
  if (rook_attacks(sq, occ) & (pieces_[make_piece(by, ROOK)] | pieces_[make_piece(by, QUEEN)])) return true;
  return false;
}

bool Position::has_non_pawn_material(Color c) const {
  return (pieces_[make_piece(c, KNIGHT)] | pieces_[make_piece(c, BISHOP)] | pieces_[make_piece(c, ROOK)] |
          pieces_[make_piece(c, QUEEN)]) != 0;
}

bool Position::is_repetition() const {
  // Only positions since the last irreversible move can repeat, and only with the same side to move.
  int n = int(history_.size());
  for (int i = n - 2; i >= 0 && i >= n - halfmove_; i -= 2)
    if (history_[i].key == key_) return true;
  return false;
}

bool Position::is_insufficient_material() const {
  if (pieces(PAWN) | pieces(ROOK) | pieces(QUEEN)) return false;
  return popcount(pieces(KNIGHT) | pieces(BISHOP)) <= 1;  // K vs K, K+minor vs K
}

void Position::make(Move m) {
  const int from = move_from(m), to = move_to(m), flag = move_flag(m);
  const Color us = side_, them = ~us;
  const int piece = board_[from];

  StateInfo st{key_, castling_, ep_, halfmove_, NO_PIECE, m};

  halfmove_++;
  if (type_of(piece) == PAWN) halfmove_ = 0;

  if (ep_ != NO_SQUARE) {
    key_ ^= zobrist_ep_file[file_of(ep_)];
    ep_ = NO_SQUARE;
  }

  if (flag == EN_PASSANT) {
    int cap_sq = us == WHITE ? to - 8 : to + 8;
    st.captured = board_[cap_sq];
    key_ ^= zobrist_piece[st.captured][cap_sq];
    remove_piece(cap_sq);
    halfmove_ = 0;
  } else if (is_capture(m)) {
    st.captured = board_[to];
    key_ ^= zobrist_piece[st.captured][to];
    remove_piece(to);
    halfmove_ = 0;
  }

  key_ ^= zobrist_piece[piece][from];
  remove_piece(from);
  int placed = is_promotion(m) ? make_piece(us, promotion_type(m)) : piece;
  put_piece(placed, to);
  key_ ^= zobrist_piece[placed][to];

  if (flag == KING_CASTLE || flag == QUEEN_CASTLE) {
    int rook_from = flag == KING_CASTLE ? to + 1 : to - 2;
    int rook_to = flag == KING_CASTLE ? to - 1 : to + 1;
    int rook = board_[rook_from];
    key_ ^= zobrist_piece[rook][rook_from] ^ zobrist_piece[rook][rook_to];
    move_piece(rook_from, rook_to);
  }

  if (flag == DOUBLE_PUSH) {
    int ep = (from + to) / 2;
    if (pawn_attacks_table[us][ep] & pieces_[make_piece(them, PAWN)]) {
      ep_ = ep;
      key_ ^= zobrist_ep_file[file_of(ep_)];
    }
  }

  key_ ^= zobrist_castling[castling_];
  castling_ &= castling_mask[from] & castling_mask[to];
  key_ ^= zobrist_castling[castling_];

  if (us == BLACK) fullmove_++;
  side_ = them;
  key_ ^= zobrist_side;
  history_.push_back(st);
}

void Position::unmake() {
  const StateInfo st = history_.back();
  history_.pop_back();

  side_ = ~side_;
  const Color us = side_;
  const Move m = st.move;
  const int from = move_from(m), to = move_to(m), flag = move_flag(m);

  if (flag == KING_CASTLE || flag == QUEEN_CASTLE) {
    int rook_from = flag == KING_CASTLE ? to + 1 : to - 2;
    int rook_to = flag == KING_CASTLE ? to - 1 : to + 1;
    move_piece(rook_to, rook_from);
  }

  int piece = is_promotion(m) ? make_piece(us, PAWN) : board_[to];
  remove_piece(to);
  put_piece(piece, from);

  if (flag == EN_PASSANT) {
    int cap_sq = us == WHITE ? to - 8 : to + 8;
    put_piece(st.captured, cap_sq);
  } else if (is_capture(m)) {
    put_piece(st.captured, to);
  }

  key_ = st.key;
  castling_ = st.castling;
  ep_ = st.ep;
  halfmove_ = st.halfmove;
  if (us == BLACK) fullmove_--;
}

// A null move passes the turn (used by null-move pruning in search).
void Position::make_null() {
  history_.push_back(StateInfo{key_, castling_, ep_, halfmove_, NO_PIECE, NO_MOVE});
  if (ep_ != NO_SQUARE) {
    key_ ^= zobrist_ep_file[file_of(ep_)];
    ep_ = NO_SQUARE;
  }
  halfmove_++;
  if (side_ == BLACK) fullmove_++;
  side_ = ~side_;
  key_ ^= zobrist_side;
}

void Position::unmake_null() {
  const StateInfo st = history_.back();
  history_.pop_back();
  side_ = ~side_;
  if (side_ == BLACK) fullmove_--;
  key_ = st.key;
  castling_ = st.castling;
  ep_ = st.ep;
  halfmove_ = st.halfmove;
}
