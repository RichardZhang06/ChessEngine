#include "movegen.h"
#include "test_util.h"

static GameResult result_of(const char* fen) {
  Position pos;
  CHECK(pos.set_fen(fen));
  return game_result(pos);
}

int main() {
  CHECK(result_of("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") == GameResult::ONGOING);
  // Fool's mate: white is checkmated
  CHECK(result_of("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3") == GameResult::CHECKMATE);
  // Stalemate: black to move, no legal moves, not in check
  CHECK(result_of("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1") == GameResult::STALEMATE);
  // Insufficient material
  CHECK(result_of("8/8/4k3/8/8/4K3/8/8 w - - 0 1") == GameResult::DRAW);
  CHECK(result_of("8/8/4k3/8/8/4KB2/8/8 w - - 0 1") == GameResult::DRAW);
  CHECK(result_of("8/8/4k3/8/8/4KR2/8/8 w - - 0 1") == GameResult::ONGOING);
  // 50-move rule
  CHECK(result_of("4k3/8/8/8/8/8/8/R3K3 w - - 100 80") == GameResult::DRAW);
  CHECK(result_of("4k3/8/8/8/8/8/8/R3K3 w - - 99 80") == GameResult::ONGOING);

  // Threefold-style repetition: knights out and back
  {
    Position pos;
    const char* shuffle[] = {"g1f3", "g8f6", "f3g1", "f6g8"};
    CHECK(game_result(pos) == GameResult::ONGOING);
    for (const char* m : shuffle) pos.make(parse_uci_move(pos, m));
    CHECK(pos.is_repetition());
    CHECK(game_result(pos) == GameResult::DRAW);
  }

  // UCI move parsing
  {
    Position pos;
    CHECK(parse_uci_move(pos, "e2e4") != NO_MOVE);
    CHECK(parse_uci_move(pos, "e2e5") == NO_MOVE);
    CHECK(parse_uci_move(pos, "zzzz") == NO_MOVE);
    Position promo;
    promo.set_fen("8/P6k/8/8/8/8/8/K7 w - - 0 1");
    CHECK(parse_uci_move(promo, "a7a8q") != NO_MOVE);
    CHECK(parse_uci_move(promo, "a7a8n") != NO_MOVE);
    CHECK(parse_uci_move(promo, "a7a8") == NO_MOVE);
    Position castle;
    castle.set_fen("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
    CHECK(parse_uci_move(castle, "e1g1") != NO_MOVE);
    CHECK(parse_uci_move(castle, "e1c1") != NO_MOVE);
    CHECK_EQ(move_to_uci(parse_uci_move(castle, "e1g1")), std::string("e1g1"));
  }
  TEST_MAIN_END();
}
