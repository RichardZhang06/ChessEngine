#pragma once
#include "position.h"

struct MoveList {
  Move moves[256];
  int count = 0;
  void add(Move m) { moves[count++] = m; }
  Move* begin() { return moves; }
  Move* end() { return moves + count; }
  const Move* begin() const { return moves; }
  const Move* end() const { return moves + count; }
};

// Pseudo-legal moves: may leave the mover's own king in check (callers test after make()).
// With captures_only: captures, en passant and promotions (for quiescence search).
void generate_pseudo_legal(const Position& pos, MoveList& list, bool captures_only = false);

// Fully legal moves.
void generate_legal(Position& pos, MoveList& list);

// Number of leaf nodes reachable in exactly `depth` plies. Used to verify move generation.
uint64_t perft(Position& pos, int depth);

// Find the legal move matching a UCI string like "e2e4" or "e7e8q"; NO_MOVE if there is none.
Move parse_uci_move(Position& pos, const std::string& uci);

enum class GameResult { ONGOING, CHECKMATE, STALEMATE, DRAW };
// Checkmate, stalemate, or a draw by repetition / 50-move rule / insufficient material.
GameResult game_result(Position& pos);
