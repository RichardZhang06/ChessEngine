#pragma once
#include <cstddef>
#include <vector>

#include "types.h"

enum Bound : uint8_t { BOUND_NONE, BOUND_EXACT, BOUND_LOWER, BOUND_UPPER };

struct TTEntry {
  Key key = 0;
  Move move = NO_MOVE;
  int16_t score = 0;
  int8_t depth = 0;
  Bound bound = BOUND_NONE;
};

// Transposition table keyed by the position's Zobrist hash. Single-threaded.
class TranspositionTable {
 public:
  explicit TranspositionTable(size_t megabytes = 16) { resize(megabytes); }

  void resize(size_t megabytes);
  void clear();

  // Returns the entry for `key` if one is stored, else nullptr.
  const TTEntry* probe(Key key) const;
  void store(Key key, Move move, int score, int depth, Bound bound);

 private:
  std::vector<TTEntry> table_;
  size_t mask_ = 0;
};
