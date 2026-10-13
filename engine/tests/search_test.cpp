#include <chrono>
#include <thread>

#include "mate_solver.h"
#include "search.h"
#include "test_util.h"

static SearchResult run(const char* fen, int depth, std::string* last_info = nullptr) {
  Position pos;
  CHECK(pos.set_fen(fen));
  TranspositionTable tt(16);
  Search search(tt);
  SearchLimits lim;
  lim.depth = depth;
  return search.go(pos, lim, [&](const std::string& s) {
    if (last_info) *last_info = s;
  });
}

static bool is_mate_score(int score, int in_moves) { return score == MATE - (2 * in_moves - 1); }

int main() {
  // Mate in 1
  for (const char* fen : {"6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1", "7k/8/6K1/8/8/8/8/R7 w - - 0 1"}) {
    std::string info;
    SearchResult r = run(fen, 4, &info);
    CHECK_EQ(move_to_uci(r.best), std::string("a1a8"));
    CHECK(is_mate_score(r.score, 1));
    CHECK(info.find("score mate 1") != std::string::npos);
  }

  // Mate in 2, verified independently by brute force, must be found with the right distance
  for (const char* fen : {"7k/8/5K2/8/8/8/8/R7 w - - 0 1", "k7/8/1K6/8/8/8/8/7R w - - 0 1",
                          "r5k1/5ppp/8/8/8/8/5PPP/3RR1K1 w - - 0 1"}) {
    Position pos;
    pos.set_fen(fen);
    bool m1 = forced_mate(pos, 1), m2 = forced_mate(pos, 2);
    SearchResult r = run(fen, 6);
    if (m1) {
      CHECK(is_mate_score(r.score, 1));
    } else if (m2) {
      CHECK(is_mate_score(r.score, 2));
    } else {
      CHECK(std::abs(r.score) < MATE_BOUND);
    }
    // whatever it found, the engine's move must be legal
    CHECK(parse_uci_move(pos, move_to_uci(r.best)) == r.best);
  }

  // Forced mate in 2 where the first move is a quiet move (Kb6 style): back-rank, rook lift
  {
    const char* fen = "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1";  // Rd8# is mate in 1
    SearchResult r = run(fen, 4);
    CHECK_EQ(move_to_uci(r.best), std::string("d1d8"));
    CHECK(is_mate_score(r.score, 1));
  }

  // Wins a hanging queen
  {
    SearchResult r = run("4k3/8/8/3q4/8/8/8/3RK3 w - - 0 1", 4);
    CHECK_EQ(move_to_uci(r.best), std::string("d1d5"));
    CHECK(r.score > 400);
  }

  // Being mated / stalemated at the root: no move
  {
    CHECK(run("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3", 3).best == NO_MOVE);
    CHECK(run("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", 3).best == NO_MOVE);
  }

  // A drawn endgame scores 0
  CHECK_EQ(run("8/8/4k3/8/8/4K3/8/8 w - - 0 1", 3).score, 0);

  // Prefers to avoid stalemate when ahead: K+Q vs K, mate in a few; engine must not stalemate
  {
    Position pos;
    CHECK(pos.set_fen("7k/8/6K1/8/8/8/5Q2/8 w - - 0 1"));  // Qf7?? is stalemate; Qf8# is mate
    TranspositionTable tt(16);
    Search search(tt);
    SearchLimits lim;
    lim.depth = 4;
    SearchResult r = search.go(pos, lim);
    pos.make(r.best);
    MoveList replies;
    generate_legal(pos, replies);
    CHECK(replies.count == 0 && pos.in_check());  // checkmate, not stalemate
  }

  // Time-limited search returns in time
  {
    Position pos;
    TranspositionTable tt(16);
    Search search(tt);
    SearchLimits lim;
    lim.movetime = 300;
    auto t0 = std::chrono::steady_clock::now();
    SearchResult r = search.go(pos, lim);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    CHECK(r.best != NO_MOVE);
    CHECK(ms < 450);
    CHECK(r.depth >= 4);
  }

  // stop() from another thread ends an infinite search promptly
  {
    Position pos;
    TranspositionTable tt(16);
    Search search(tt);
    SearchLimits lim;
    lim.infinite = true;
    std::thread stopper([&] {
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      search.stop();
    });
    auto t0 = std::chrono::steady_clock::now();
    SearchResult r = search.go(pos, lim);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    stopper.join();
    CHECK(r.best != NO_MOVE);
    CHECK(ms < 600);
  }

  // The search leaves the position untouched
  {
    Position pos;
    pos.set_fen("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
    std::string before = pos.fen();
    Key key = pos.key();
    TranspositionTable tt(16);
    Search search(tt);
    SearchLimits lim;
    lim.depth = 5;
    SearchResult r = search.go(pos, lim);
    CHECK_EQ(pos.fen(), before);
    CHECK_EQ(pos.key(), key);
    CHECK(parse_uci_move(pos, move_to_uci(r.best)) == r.best);
  }
  TEST_MAIN_END();
}
