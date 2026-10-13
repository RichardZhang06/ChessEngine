#pragma once
#include <string>
#include <vector>

#include "bitboard.h"
#include "types.h"

// State saved by make() so that unmake() can restore the previous position exactly.
struct StateInfo {
  Key key;
  int castling;
  int ep;
  int halfmove;
  int captured;
  Move move;
};

class Position {
 public:
  Position();
  explicit Position(bool /*empty*/);  // blank board, used internally while parsing a FEN

  // FEN (https://www.chessprogramming.org/Forsyth-Edwards_Notation). Returns false (position unchanged) if malformed.
  bool set_fen(const std::string& fen);
  void set_startpos();
  std::string fen() const;

  void make(Move m);
  void unmake();
  void make_null();
  void unmake_null();

  Color side_to_move() const { return side_; }
  int castling_rights() const { return castling_; }
  int ep_square() const { return ep_; }
  int halfmove_clock() const { return halfmove_; }
  int fullmove_number() const { return fullmove_; }
  Key key() const { return key_; }
  int ply_count() const { return int(history_.size()); }

  int piece_on(int sq) const { return board_[sq]; }
  Bitboard pieces(Color c, PieceType t) const { return pieces_[make_piece(c, t)]; }
  Bitboard pieces(PieceType t) const { return pieces_[t] | pieces_[6 + t]; }
  Bitboard occupied(Color c) const { return occ_[c]; }
  Bitboard occupied() const { return occ_[WHITE] | occ_[BLACK]; }
  int king_square(Color c) const { return lsb(pieces_[make_piece(c, KING)]); }

  Bitboard attackers_to(int sq, Bitboard occ) const;
  bool attacked(int sq, Color by) const;
  bool in_check() const { return attacked(king_square(side_), ~side_); }
  // After make(): true if the side that just moved left its own king attacked (the move was illegal).
  bool left_king_in_check() const { return attacked(king_square(~side_), side_); }

  bool has_non_pawn_material(Color c) const;
  bool is_repetition() const;
  bool is_fifty_move_draw() const { return halfmove_ >= 100; }
  bool is_insufficient_material() const;
  // Draw by repetition, 50-move rule or insufficient material (checkmate/stalemate are decided by move generation).
  bool is_draw() const { return is_repetition() || is_fifty_move_draw() || is_insufficient_material(); }

  Key compute_key() const;
  std::string to_string() const;

 private:
  void put_piece(int piece, int sq);
  void remove_piece(int sq);
  void move_piece(int from, int to);

  int board_[64];
  Bitboard pieces_[12];
  Bitboard occ_[2];
  Color side_;
  int castling_;
  int ep_;
  int halfmove_;
  int fullmove_;
  Key key_;
  std::vector<StateInfo> history_;
};

void init_zobrist();
