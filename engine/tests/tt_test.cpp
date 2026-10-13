#include "test_util.h"
#include "tt.h"

int main() {
  TranspositionTable tt(1);
  CHECK(tt.probe(12345) == nullptr);
  Move m = make_move(12, 28, DOUBLE_PUSH);
  tt.store(12345, m, 42, 5, BOUND_EXACT);
  const TTEntry* e = tt.probe(12345);
  CHECK(e != nullptr);
  if (e) {
    CHECK_EQ(e->move, m);
    CHECK_EQ(int(e->score), 42);
    CHECK_EQ(int(e->depth), 5);
    CHECK(e->bound == BOUND_EXACT);
  }
  // A shallower non-exact result does not overwrite a deeper one, but keeps/updates sensibly
  tt.store(12345, NO_MOVE, 7, 2, BOUND_LOWER);
  CHECK_EQ(int(tt.probe(12345)->depth), 5);
  // Deeper result replaces, and a missing move keeps the old best move
  tt.store(12345, NO_MOVE, 99, 8, BOUND_UPPER);
  CHECK_EQ(int(tt.probe(12345)->score), 99);
  CHECK_EQ(tt.probe(12345)->move, m);
  tt.clear();
  CHECK(tt.probe(12345) == nullptr);
  tt.resize(2);
  CHECK(tt.probe(12345) == nullptr);
  TEST_MAIN_END();
}
