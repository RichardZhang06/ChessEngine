#include "eval.h"
#include "test_util.h"

static int eval_fen(const char* fen) {
  Position pos;
  CHECK(pos.set_fen(fen));
  return evaluate(pos);
}

int main() {
  // Symmetric start position: only the tempo bonus separates the sides
  CHECK_EQ(eval_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), 10);
  CHECK_EQ(eval_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1"), 10);

  // Material: being a queen up is large and positive for the side to move, negative for the other
  int up = eval_fen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  int down = eval_fen("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
  CHECK(up > 800);
  CHECK(down < -800);

  // Mirroring the colors gives the same score for the side to move
  CHECK_EQ(eval_fen("r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3"),
           eval_fen("rnbqkb1r/pppp1ppp/5n2/4p3/4P3/2N5/PPPP1PPP/R1BQKBNR b KQkq - 2 3"));

  // A passed pawn on the 7th is worth more than a blocked one on the 2nd
  CHECK(eval_fen("4k3/P7/8/8/8/8/8/4K3 w - - 0 1") > eval_fen("4k3/8/8/8/8/8/P7/4K3 w - - 0 1"));
  // Bishop pair bonus
  CHECK(eval_fen("4k3/8/8/8/8/8/8/2B1KB2 w - - 0 1") > eval_fen("4k3/8/8/8/8/8/8/2B1KN2 w - - 0 1") + 0);
  TEST_MAIN_END();
}
