#include "tt.h"

void TranspositionTable::resize(size_t megabytes) {
  size_t entries = megabytes * 1024 * 1024 / sizeof(TTEntry);
  size_t n = 1;
  while (n * 2 <= entries) n *= 2;  // power of two so indexing is a mask
  table_.assign(n, TTEntry{});
  mask_ = n - 1;
}

void TranspositionTable::clear() { table_.assign(table_.size(), TTEntry{}); }

const TTEntry* TranspositionTable::probe(Key key) const {
  const TTEntry& e = table_[key & mask_];
  return e.bound != BOUND_NONE && e.key == key ? &e : nullptr;
}

void TranspositionTable::store(Key key, Move move, int score, int depth, Bound bound) {
  TTEntry& e = table_[key & mask_];
  if (e.key == key && e.bound != BOUND_NONE) {
    // Same position: keep the old entry if it searched deeper, unless the new result is exact.
    if (depth < e.depth && bound != BOUND_EXACT) return;
    if (move == NO_MOVE) move = e.move;
  }
  e.key = key;
  e.move = move;
  e.score = int16_t(score);
  e.depth = int8_t(depth);
  e.bound = bound;
}
