#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>

#include "movegen.h"
#include "position.h"
#include "tt.h"

constexpr int MAX_PLY = 128;
constexpr int MAX_DEPTH = 64;
constexpr int MATE = 32000;
constexpr int MATE_BOUND = MATE - MAX_PLY;  // scores beyond this mean "forced mate"
constexpr int INF = 32500;

struct SearchLimits {
  int depth = 0;           // 0: no depth limit
  int64_t movetime = 0;    // milliseconds, 0: none
  bool use_clock = false;  // true when wtime/btime were given
  int64_t time[2] = {0, 0};
  int64_t inc[2] = {0, 0};
  int movestogo = 0;
  bool infinite = false;
};

struct SearchResult {
  Move best = NO_MOVE;
  Move ponder = NO_MOVE;
  int score = 0;  // centipawns for the side to move, or +-(MATE - ply) for forced mates
  int depth = 0;
  uint64_t nodes = 0;
};

using InfoCallback = std::function<void(const std::string&)>;

// Iterative-deepening negamax with alpha-beta, PVS, a transposition table, null-move pruning,
// late move reductions, check extensions and quiescence search.
class Search {
 public:
  explicit Search(TranspositionTable& tt) : tt_(tt) {}

  // Blocks until the search finishes, hits a limit, or stop() is called from another thread.
  SearchResult go(Position& pos, const SearchLimits& limits, InfoCallback info = nullptr);
  void stop() { stop_.store(true); }
  // go() does not clear a pending stop(), so a stop() that arrives just before the search thread gets going still
  // ends the search. Call this before starting a new search.
  void clear_stop() { stop_.store(false); }
  void new_game();

 private:
  int negamax(Position& pos, int depth, int alpha, int beta, int ply, bool allow_null);
  int quiescence(Position& pos, int alpha, int beta, int ply);
  bool should_stop();
  int64_t elapsed_ms() const;
  void score_moves(const Position& pos, const MoveList& list, int* scores, Move tt_move, int ply) const;
  void report(int depth, int score);

  TranspositionTable& tt_;
  std::atomic<bool> stop_{false};
  InfoCallback info_;
  std::chrono::steady_clock::time_point start_;
  int64_t soft_ms_ = 0, hard_ms_ = 0;  // 0: no limit
  bool time_checks_ = false;
  uint64_t nodes_ = 0;
  Move root_best_ = NO_MOVE;

  Move killers_[MAX_PLY + 2][2] = {};
  int history_[2][64][64] = {};
  Move pv_[MAX_PLY + 1][MAX_PLY + 1] = {};
  int pv_len_[MAX_PLY + 1] = {};
};
