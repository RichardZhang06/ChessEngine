#include "eval.h"

namespace {

// Piece-square tables (Tomasz Michniewski's "simplified evaluation function").
// Written from white's point of view with rank 8 first, so white indexes with sq ^ 56 and black with sq.
const int PST[6][64] = {
    // pawn
    {0,  0,  0,  0,   0,   0,  0,  0,  50, 50, 50,  50,  50,  50, 50, 50, 10, 10, 20, 30,  30,  20, 10, 10, 5,  5,  10, 25, 25,  10, 5,  5,
     0,  0,  0,  20,  20,  0,  0,  0,  5,  -5, -10, 0,   0,   -10, -5, 5,  5,  10, 10, -20, -20, 10, 10, 5,  0,  0,  0,  0,  0,   0,  0,  0},
    // knight
    {-50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,   0,   -20, -40, -30, 0,   10,  15,  15,  10,
     0,   -30, -30, 5,   15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,   -30, -30, 5,   10,  15,
     15,  10,  5,   -30, -40, -20, 0,   5,   5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50},
    // bishop
    {-20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,   0,   0,   -10, -10, 0,   5,   10,  10,  5,
     0,   -10, -10, 5,   5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,   -10, -10, 10,  10,  10,
     10,  10,  10,  -10, -10, 5,   0,   0,   0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20},
    // rook
    {0,  0,  0, 0, 0, 0, 0, 0, 5,  10, 10, 10, 10, 10, 10, 5,  -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5,
     -5, 0,  0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 0,  0, 0, 5, 5, 0, 0, 0},
    // queen
    {-20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10, -10, 0,  5,  5,  5,  5,  0,   -10, -5, 0,
     5,   5,   5,   5,  0,  -5,  0,   0,   5,   5,   5,   5,  0,  -5,  -10, 5,   5,   5,  5,  5,  0,  -10, -10, 0,   5,  0,
     0,   0,   0,   -10, -20, -10, -10, -5,  -5,  -10, -10, -20},
    // king (middlegame)
    {-30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40,
     -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20,
     -20, -20, -20, -10, 20,  20,  0,   0,   0,   0,   20,  20,  20,  30,  10,  0,   0,   10,  30,  20}};

const int KING_ENDGAME[64] = {-50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0,   0,   -10, -20, -30,
                              -30, -10, 20,  30,  30,  20,  -10, -30, -30, -10, 30,  40,  40,  30,  -10, -30,
                              -30, -10, 30,  40,  40,  30,  -10, -30, -30, -10, 20,  30,  30,  20,  -10, -30,
                              -30, -30, 0,   0,   0,   0,   -30, -30, -50, -30, -30, -30, -30, -30, -30, -50};

constexpr int BISHOP_PAIR = 30;
constexpr int PASSED_PAWN[8] = {0, 10, 20, 35, 60, 100, 150, 0};  // by rank from the pawn's own side
constexpr int TEMPO = 10;

inline int pst_index(Color c, int sq) { return c == WHITE ? sq ^ 56 : sq; }

}  // namespace

int evaluate(const Position& pos) {
  int score[2] = {0, 0};
  int phase = 0;  // 24 with all pieces on the board, 0 with only kings and pawns

  for (int c = 0; c < 2; c++) {
    const Color color = Color(c);
    for (int t = PAWN; t <= QUEEN; t++) {
      Bitboard pcs = pos.pieces(color, PieceType(t));
      int n = popcount(pcs);
      score[c] += n * PIECE_VALUE[t];
      phase += n * (t == KNIGHT || t == BISHOP ? 1 : t == ROOK ? 2 : t == QUEEN ? 4 : 0);
      while (pcs) score[c] += PST[t][pst_index(color, pop_lsb(pcs))];
    }
    if (popcount(pos.pieces(color, BISHOP)) >= 2) score[c] += BISHOP_PAIR;

    Bitboard pawns = pos.pieces(color, PAWN);
    const Bitboard enemy_pawns = pos.pieces(~color, PAWN);
    while (pawns) {
      int sq = pop_lsb(pawns);
      if (!(passed_pawn_mask[color][sq] & enemy_pawns))
        score[c] += PASSED_PAWN[color == WHITE ? rank_of(sq) : 7 - rank_of(sq)];
    }
  }

  // King: blend the middlegame and endgame tables by how much material is left.
  phase = phase > 24 ? 24 : phase;
  for (int c = 0; c < 2; c++) {
    int idx = pst_index(Color(c), pos.king_square(Color(c)));
    score[c] += (PST[KING][idx] * phase + KING_ENDGAME[idx] * (24 - phase)) / 24;
  }

  const Color us = pos.side_to_move();
  return score[us] - score[~us] + TEMPO;
}
