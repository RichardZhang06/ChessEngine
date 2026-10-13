#pragma once
#include <cstdint>
#include <string>

using Bitboard = uint64_t;
using Key = uint64_t;

enum Color : int { WHITE = 0, BLACK = 1 };
constexpr Color operator~(Color c) { return Color(c ^ 1); }

// Piece types. A piece is color * 6 + type (0..11); NO_PIECE marks an empty square.
enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE_TYPE };
constexpr int NO_PIECE = 12;
constexpr int make_piece(Color c, PieceType t) { return c * 6 + t; }
constexpr Color color_of(int p) { return Color(p / 6); }
constexpr PieceType type_of(int p) { return PieceType(p % 6); }

// Squares: a1 = 0, b1 = 1, ... h8 = 63.
constexpr int NO_SQUARE = 64;
constexpr int file_of(int sq) { return sq & 7; }
constexpr int rank_of(int sq) { return sq >> 3; }
constexpr int make_square(int file, int rank) { return rank * 8 + file; }

enum Square : int { A1 = 0, B1, C1, D1, E1, F1, G1, H1, A8 = 56, B8, C8, D8, E8, F8, G8, H8 = 63 };

// Castling rights bits.
enum : int { WHITE_OO = 1, WHITE_OOO = 2, BLACK_OO = 4, BLACK_OOO = 8 };

// A move packs from (6 bits), to (6 bits) and a 4-bit flag.
using Move = uint16_t;
constexpr Move NO_MOVE = 0;

enum MoveFlag : int {
  QUIET = 0,
  DOUBLE_PUSH = 1,
  KING_CASTLE = 2,
  QUEEN_CASTLE = 3,
  CAPTURE = 4,
  EN_PASSANT = 5,
  PROMO_N = 8,
  PROMO_B = 9,
  PROMO_R = 10,
  PROMO_Q = 11,
  PROMO_CAPTURE_N = 12,
  PROMO_CAPTURE_B = 13,
  PROMO_CAPTURE_R = 14,
  PROMO_CAPTURE_Q = 15
};

constexpr Move make_move(int from, int to, int flag = QUIET) {
  return Move(from | (to << 6) | (flag << 12));
}
constexpr int move_from(Move m) { return m & 63; }
constexpr int move_to(Move m) { return (m >> 6) & 63; }
constexpr int move_flag(Move m) { return m >> 12; }
constexpr bool is_promotion(Move m) { return move_flag(m) >= PROMO_N; }
constexpr bool is_capture(Move m) { return (move_flag(m) & 4) != 0; }  // includes en passant and promo captures
constexpr PieceType promotion_type(Move m) { return PieceType(KNIGHT + (move_flag(m) & 3)); }

std::string square_name(int sq);
std::string move_to_uci(Move m);
