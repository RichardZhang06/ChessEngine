#include <set>

#include "book.h"
#include "test_util.h"
#include "uci.h"

int main() {
  OpeningBook book;
  CHECK(book.load_embedded());
  CHECK(book.size() > 1000);

  // Every book move is legal in the position it is stored for (the key's missing fields default sensibly).
  int checked = 0, bad = 0;
  book.for_each([&](const std::string& key, const std::string& uci, int weight) {
    Position pos;
    CHECK(pos.set_fen(key));
    CHECK(weight > 0);
    // En passant captures need the ep square, which the key leaves out; they are checked against the real position
    // when probing, so accept a pawn capture onto an empty square here.
    int from = int(uci[0] - 'a') + 8 * int(uci[1] - '1'), to = int(uci[2] - 'a') + 8 * int(uci[3] - '1');
    bool maybe_ep = type_of(pos.piece_on(from)) == PAWN && file_of(from) != file_of(to) && pos.piece_on(to) == NO_PIECE;
    if (!maybe_ep && parse_uci_move(pos, uci) == NO_MOVE) {
      if (bad++ < 5) std::printf("illegal book move %s in %s\\n", uci.c_str(), key.c_str());
    }
    CHECK_EQ(OpeningBook::key(pos), key);  // the key survives a FEN round trip
    checked++;
  });
  CHECK(checked > 5000);
  CHECK_EQ(bad, 0);

  // The start position offers the main first moves only (rare ones are pruned)
  Position pos;
  std::set<std::string> first;
  for (auto& [m, w] : book.moves(pos)) first.insert(move_to_uci(m));
  CHECK(first.count("e2e4") && first.count("d2d4") && first.count("g1f3") && first.count("c2c4"));
  CHECK(!first.count("a2a3"));

  // Picks are legal, follow the weights, and are reproducible for a given seed
  std::mt19937_64 rng(42);
  int e4 = 0, d4 = 0, other = 0;
  for (int i = 0; i < 2000; i++) {
    Move m = book.pick(pos, rng);
    CHECK(m != NO_MOVE);
    std::string u = move_to_uci(m);
    (u == "e2e4" ? e4 : u == "d2d4" ? d4 : other)++;
  }
  CHECK(e4 > d4 && d4 > 0 && other > 0);  // e4 (2032 lines) beats d4 (1333); the minor moves show up too
  std::mt19937_64 a(7), b(7);
  CHECK(book.pick(pos, a) == book.pick(pos, b));

  // A line followed from the book stays in book for several moves; unknown positions return NO_MOVE
  for (const char* m : {"e2e4", "e7e5", "g1f3", "b8c6", "f1b5"}) pos.make(parse_uci_move(pos, m));
  CHECK(book.pick(pos, rng) != NO_MOVE);  // Ruy Lopez: 3...a6 and friends
  Position odd;
  odd.set_fen("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  CHECK(book.pick(odd, rng) == NO_MOVE);

  // Parsing
  OpeningBook small;
  CHECK(small.load_text("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e2e4:3 d2d4:1\n"));
  CHECK_EQ(small.size(), size_t(1));
  CHECK_EQ(small.moves(pos = Position()).size(), size_t(2));
  CHECK(!small.load_text("garbage\n"));
  CHECK(small.empty());
  CHECK(!small.load_text("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e2e4-3\n"));
  CHECK(!small.load_file("/definitely/not/a/file"));

  // A book move that is illegal in the position is ignored rather than played
  OpeningBook wrong;
  wrong.load_text("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq e2e5:1\n");
  Position start;
  CHECK(wrong.pick(start, rng) == NO_MOVE);

  // Through UCI: off by default, plays book moves when OwnBook is on, and falls back to searching out of book
  {
    std::vector<std::string> lines;
    Uci uci([&](const std::string& l) { lines.push_back(l); });
    auto has = [&](const std::string& prefix) {
      for (auto& l : lines)
        if (l.rfind(prefix, 0) == 0) return true;
      return false;
    };
    uci.handle("uci");
    CHECK(has("option name OwnBook type check default true"));
    uci.handle("setoption name BookSeed value 5");
    uci.handle("position startpos");
    uci.handle("go depth 2");
    uci.wait_for_search();
    CHECK(has("info string book move"));  // the book is on by default
    CHECK(!has("info depth"));

    lines.clear();
    uci.handle("setoption name OwnBook value false");
    uci.handle("position startpos");
    uci.handle("go depth 2");
    uci.wait_for_search();
    CHECK(has("info depth"));  // book off: the engine searches
    CHECK(!has("info string book move"));

    lines.clear();
    uci.handle("setoption name OwnBook value true");
    uci.handle("position startpos");
    uci.handle("go movetime 100");
    uci.wait_for_search();
    CHECK(has("info string book move"));
    CHECK(has("bestmove "));
    CHECK(!has("info depth"));

    lines.clear();
    uci.handle("position fen 4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
    uci.handle("go depth 2");
    uci.wait_for_search();
    CHECK(!has("info string book move"));
    CHECK(has("info depth"));  // out of book: normal search

    lines.clear();
    uci.handle("position startpos");
    uci.handle("go infinite");
    uci.handle("stop");
    CHECK(!has("info string book move"));  // infinite analysis ignores the book

    lines.clear();
    uci.handle("setoption name BookFile value /no/such/file");
    CHECK(has("info string could not load book"));
    uci.handle("setoption name OwnBook value false");
    uci.handle("position startpos");
    uci.handle("go depth 1");
    uci.wait_for_search();
    CHECK(!has("info string book move"));
  }
  TEST_MAIN_END();
}
