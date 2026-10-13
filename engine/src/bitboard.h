#pragma once
#include "types.h"

constexpr Bitboard bit(int sq) { return 1ULL << sq; }
inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int lsb(Bitboard b) { return __builtin_ctzll(b); }
inline int msb(Bitboard b) { return 63 - __builtin_clzll(b); }
inline int pop_lsb(Bitboard& b) {
  int s = lsb(b);
  b &= b - 1;
  return s;
}

// Call once at startup.
void init_bitboards();

extern Bitboard knight_attacks_table[64];
extern Bitboard king_attacks_table[64];
extern Bitboard pawn_attacks_table[2][64];  // squares attacked by a pawn of the given color
extern Bitboard ray_table[8][64];
extern Bitboard passed_pawn_mask[2][64];  // squares that must be free of enemy pawns for a pawn to be passed
extern Bitboard file_mask[8];

// Ray directions: N, NE, E, SE, S, SW, W, NW
inline Bitboard ray_attacks(int dir, int sq, Bitboard occ) {
  Bitboard attacks = ray_table[dir][sq];
  Bitboard blockers = attacks & occ;
  if (blockers) {
    bool positive = dir <= 2 || dir == 7;
    int blocker = positive ? lsb(blockers) : msb(blockers);
    attacks ^= ray_table[dir][blocker];
  }
  return attacks;
}
inline Bitboard rook_attacks(int sq, Bitboard occ) {
  return ray_attacks(0, sq, occ) | ray_attacks(2, sq, occ) | ray_attacks(4, sq, occ) | ray_attacks(6, sq, occ);
}
inline Bitboard bishop_attacks(int sq, Bitboard occ) {
  return ray_attacks(1, sq, occ) | ray_attacks(3, sq, occ) | ray_attacks(5, sq, occ) | ray_attacks(7, sq, occ);
}
inline Bitboard queen_attacks(int sq, Bitboard occ) { return rook_attacks(sq, occ) | bishop_attacks(sq, occ); }
