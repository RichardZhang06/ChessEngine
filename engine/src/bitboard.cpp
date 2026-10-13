#include "bitboard.h"

Bitboard knight_attacks_table[64];
Bitboard king_attacks_table[64];
Bitboard pawn_attacks_table[2][64];
Bitboard ray_table[8][64];
Bitboard passed_pawn_mask[2][64];
Bitboard file_mask[8];

std::string square_name(int sq) {
  if (sq < 0 || sq > 63) return "-";
  return std::string{char('a' + file_of(sq)), char('1' + rank_of(sq))};
}

std::string move_to_uci(Move m) {
  if (m == NO_MOVE) return "0000";
  std::string s = square_name(move_from(m)) + square_name(move_to(m));
  if (is_promotion(m)) s += "nbrq"[promotion_type(m) - KNIGHT];
  return s;
}

void init_bitboards() {
  static bool done = false;
  if (done) return;
  done = true;

  const int df[8] = {0, 1, 1, 1, 0, -1, -1, -1};
  const int dr[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  const int kn_f[8] = {1, 2, 2, 1, -1, -2, -2, -1};
  const int kn_r[8] = {2, 1, -1, -2, -2, -1, 1, 2};

  for (int f = 0; f < 8; f++) {
    file_mask[f] = 0;
    for (int r = 0; r < 8; r++) file_mask[f] |= bit(make_square(f, r));
  }

  for (int sq = 0; sq < 64; sq++) {
    int f = file_of(sq), r = rank_of(sq);
    knight_attacks_table[sq] = king_attacks_table[sq] = 0;
    pawn_attacks_table[WHITE][sq] = pawn_attacks_table[BLACK][sq] = 0;

    for (int i = 0; i < 8; i++) {
      int nf = f + kn_f[i], nr = r + kn_r[i];
      if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) knight_attacks_table[sq] |= bit(make_square(nf, nr));
      nf = f + df[i];
      nr = r + dr[i];
      if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) king_attacks_table[sq] |= bit(make_square(nf, nr));

      ray_table[i][sq] = 0;
      nf = f + df[i];
      nr = r + dr[i];
      while (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) {
        ray_table[i][sq] |= bit(make_square(nf, nr));
        nf += df[i];
        nr += dr[i];
      }
    }

    for (int side = 0; side < 2; side++) {
      int step = side == WHITE ? 1 : -1;
      for (int side_f : {-1, 1}) {
        int nf = f + side_f, nr = r + step;
        if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) pawn_attacks_table[side][sq] |= bit(make_square(nf, nr));
      }
      Bitboard mask = 0;
      for (int nf = f - 1; nf <= f + 1; nf++) {
        if (nf < 0 || nf > 7) continue;
        for (int nr = r + step; nr >= 0 && nr < 8; nr += step) mask |= bit(make_square(nf, nr));
      }
      passed_pawn_mask[side][sq] = mask;
    }
  }
}
