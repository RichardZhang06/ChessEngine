// Regression suite: if a search or evaluation change makes the engine miss any of these, this test fails.
// Everything runs at a fixed depth with a single thread, so results are deterministic across machines.
#include "mate_solver.h"
#include "search.h"
#include "test_util.h"

struct Tactic {
  const char* id;
  const char* fen;
  const char* best;  // expected best move (UCI)
};

static SearchResult search_fen(const std::string& fen, int depth) {
  Position pos;
  if (!pos.set_fen(fen)) {
    std::printf("bad fen: %s\n", fen.c_str());
    std::exit(2);
  }
  TranspositionTable tt(16);
  Search search(tt);
  SearchLimits lim;
  lim.depth = depth;
  return search.go(pos, lim);
}

int main() {
  // Win At Chess positions with a single clearly best move
  const Tactic tactics[] = {
      {"WAC.001", "2rr3k/pp3pp1/1nnqbN1p/3pN3/2pP4/2P3Q1/PPB4P/R4RK1 w - - 0 1", "g3g6"},
      {"WAC.003", "5rk1/1ppb3p/p1pb4/6q1/3P1p1r/2P1R2P/PP1BQ1P1/5RKN w - - 0 1", "e3g3"},
      {"WAC.004", "r1bq2rk/pp3pbp/2p1p1pQ/7P/3P4/2PB1N2/PP3PPR/2KR4 w - - 0 1", "h6h7"},
      {"WAC.005", "5k2/6pp/p1qN4/1p1p4/3P4/2PKP2Q/PP3r2/3R4 b - - 0 1", "c6c4"},
      {"WAC.006", "7k/p7/1R5K/6r1/6p1/6P1/8/8 w - - 0 1", "b6b7"},
      {"WAC.007", "1k1r4/pp1b1R2/3q2pp/4p3/2B5/4Q3/PPP2B2/2K5 b - - 0 1", "d6d1"},
  };
  for (const Tactic& t : tactics) {
    SearchResult r = search_fen(t.fen, 9);
    if (move_to_uci(r.best) != t.best) {
      std::printf("FAIL %s: expected %s, got %s\n", t.id, t.best, move_to_uci(r.best).c_str());
      g_failures++;
    }
  }

  // Random K+Q vs K and K+R vs K positions: the engine's mate distance must agree with a brute-force solver
  uint64_t rng = 0x1234567ULL;
  auto next = [&rng] {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
  };
  int tested = 0;
  while (tested < 24) {
    int sq[4];
    for (int& s : sq) s = int(next() % 64);
    if (sq[0] == sq[1] || sq[0] == sq[2] || sq[0] == sq[3] || sq[1] == sq[2] || sq[1] == sq[3] || sq[2] == sq[3])
      continue;
    // sq[0] white king, sq[1] black king, sq[2] white queen/rook, sq[3] unused
    if (std::abs(file_of(sq[0]) - file_of(sq[1])) <= 1 && std::abs(rank_of(sq[0]) - rank_of(sq[1])) <= 1) continue;
    char board[64];
    for (char& c : board) c = '.';
    board[sq[0]] = 'K';
    board[sq[1]] = 'k';
    board[sq[2]] = tested % 2 ? 'Q' : 'R';
    std::string fen;
    for (int rank = 7; rank >= 0; rank--) {
      int empty = 0;
      for (int file = 0; file < 8; file++) {
        char c = board[rank * 8 + file];
        if (c == '.') { empty++; continue; }
        if (empty) fen += char('0' + empty);
        empty = 0;
        fen += c;
      }
      if (empty) fen += char('0' + empty);
      if (rank) fen += '/';
    }
    fen += " w - - 0 1";

    Position pos;
    if (!pos.set_fen(fen)) continue;  // illegal setup (e.g. the side not to move is in check)
    tested++;
    bool m1 = forced_mate(pos, 1), m2 = !m1 && forced_mate(pos, 2);
    SearchResult r = search_fen(fen, 7);
    if (m1) {
      CHECK_EQ(r.score, MATE - 1);
    } else if (m2) {
      CHECK_EQ(r.score, MATE - 3);
    } else {
      CHECK(r.score > -MATE_BOUND);  // the side with the extra piece is never shown as getting mated
    }
    CHECK(parse_uci_move(pos, move_to_uci(r.best)) == r.best);
  }
  TEST_MAIN_END();
}
