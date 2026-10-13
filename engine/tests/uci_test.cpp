#include <mutex>
#include <sstream>
#include <vector>

#include "test_util.h"
#include "uci.h"

struct Harness {
  std::vector<std::string> lines;
  Uci uci{[this](const std::string& l) { lines.push_back(l); }};
  void send(const std::string& cmd) { uci.handle(cmd); }
  bool has(const std::string& prefix) const {
    for (auto& l : lines)
      if (l.rfind(prefix, 0) == 0) return true;
    return false;
  }
  bool contains(const std::string& text) const {
    for (auto& l : lines)
      if (l.find(text) != std::string::npos) return true;
    return false;
  }
  std::string find(const std::string& prefix) const {
    for (auto& l : lines)
      if (l.rfind(prefix, 0) == 0) return l;
    return "";
  }
};

int main() {
  {
    Harness h;
    h.send("uci");
    CHECK(h.has("id name"));
    CHECK(h.has("id author"));
    CHECK(h.has("option name Hash"));
    CHECK_EQ(h.lines.back(), std::string("uciok"));
    h.send("isready");
    CHECK_EQ(h.lines.back(), std::string("readyok"));
    h.send("");
    h.send("nonsense here");  // unknown commands are ignored
    CHECK_EQ(h.lines.back(), std::string("readyok"));
  }
  {
    Harness h;
    h.send("ucinewgame");
    h.send("position startpos moves e2e4 e7e5 g1f3");
    h.send("go depth 4");
    h.uci.wait_for_search();
    CHECK(h.has("info depth 4"));
    std::string best = h.find("bestmove ");
    CHECK(!best.empty());
    // bestmove is legal in the position after the three moves (black to move)
    Position pos;
    for (const char* m : {"e2e4", "e7e5", "g1f3"}) pos.make(parse_uci_move(pos, m));
    CHECK(parse_uci_move(pos, best.substr(9, 4)) != NO_MOVE);
  }
  {
    // position fen + mate in one
    Harness h;
    h.send("position fen 6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1");
    h.send("go depth 3");
    h.uci.wait_for_search();
    CHECK(h.has("bestmove a1a8"));
    CHECK(h.contains("score mate 1"));  // reported once the mate is found (the search stops when it is proven)
  }
  {
    // movetime
    Harness h;
    h.send("position startpos");
    h.send("go movetime 100");
    h.uci.wait_for_search();
    CHECK(h.has("bestmove "));
  }
  {
    // clock-based search
    Harness h;
    h.send("go wtime 2000 btime 2000 winc 0 binc 0");
    h.uci.wait_for_search();
    CHECK(h.has("bestmove "));
  }
  {
    // infinite search only reports a move after stop
    Harness h;
    h.send("position startpos");
    h.send("go infinite");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    h.send("isready");  // answered while searching
    CHECK(h.has("readyok"));
    CHECK(!h.has("bestmove"));
    h.send("stop");
    CHECK(h.has("bestmove "));
  }
  {
    // no legal moves: bestmove 0000
    Harness h;
    h.send("position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
    h.send("go depth 3");
    h.uci.wait_for_search();
    CHECK(h.has("bestmove 0000"));
  }
  {
    // illegal move in the move list stops applying moves, engine stays usable
    Harness h;
    h.send("position startpos moves e2e4 e2e4 d2d4");
    CHECK(h.has("info string illegal move: e2e4"));
    h.send("d");
    CHECK(h.contains("fen: rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b"));
    // invalid fen is rejected
    h.send("position fen not a fen");
    CHECK(h.has("info string invalid fen"));
  }
  {
    // go perft
    Harness h;
    h.send("position startpos");
    h.send("go perft 3");
    CHECK(h.has("Nodes searched: 8902"));
  }
  {
    // setoption Hash and quit
    Harness h;
    h.send("setoption name Hash value 32");
    h.send("setoption name Hash value abc");
    CHECK(h.has("info string invalid Hash value"));
    CHECK(!h.uci.handle("quit"));
  }
  {
    // loop() reads until quit
    Harness h;
    std::istringstream in("uci\nisready\nquit\nisready\n");
    h.uci.loop(in);
    CHECK_EQ(h.lines.back(), std::string("readyok"));
    int readyok = 0;
    for (auto& l : h.lines) readyok += l == "readyok";
    CHECK_EQ(readyok, 1);  // the command after quit is not processed
  }
  TEST_MAIN_END();
}
