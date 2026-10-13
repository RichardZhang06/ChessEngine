#include "search.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "eval.h"

namespace {

constexpr int HISTORY_MAX = 600000;

int score_to_tt(int score, int ply) {
  if (score >= MATE_BOUND) return score + ply;
  if (score <= -MATE_BOUND) return score - ply;
  return score;
}

int score_from_tt(int score, int ply) {
  if (score >= MATE_BOUND) return score - ply;
  if (score <= -MATE_BOUND) return score + ply;
  return score;
}

bool is_quiet(Move m) { return !is_capture(m) && !is_promotion(m); }

}  // namespace

void Search::new_game() {
  std::memset(killers_, 0, sizeof(killers_));
  std::memset(history_, 0, sizeof(history_));
}

int64_t Search::elapsed_ms() const {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_).count();
}

bool Search::should_stop() {
  if (stop_.load(std::memory_order_relaxed)) return true;
  if (time_checks_ && hard_ms_ > 0 && (nodes_ & 2047) == 0 && elapsed_ms() >= hard_ms_) stop_.store(true);
  return stop_.load(std::memory_order_relaxed);
}

void Search::score_moves(const Position& pos, const MoveList& list, int* scores, Move tt_move, int ply) const {
  const Color us = pos.side_to_move();
  for (int i = 0; i < list.count; i++) {
    Move m = list.moves[i];
    int s;
    if (m == tt_move) {
      s = 2000000;
    } else if (is_capture(m) || is_promotion(m)) {
      int victim = move_flag(m) == EN_PASSANT ? PAWN : (is_capture(m) ? type_of(pos.piece_on(move_to(m))) : 0);
      int attacker = type_of(pos.piece_on(move_from(m)));
      s = 1000000 + (is_capture(m) ? victim * 16 - attacker : 0) + (is_promotion(m) ? promotion_type(m) * 100 : 0);
    } else if (m == killers_[ply][0]) {
      s = 900000;
    } else if (m == killers_[ply][1]) {
      s = 800000;
    } else {
      s = history_[us][move_from(m)][move_to(m)];
    }
    scores[i] = s;
  }
}

int Search::quiescence(Position& pos, int alpha, int beta, int ply) {
  nodes_++;
  if (should_stop()) return 0;
  if (ply >= MAX_PLY - 1) return evaluate(pos);

  int stand_pat = evaluate(pos);
  if (stand_pat >= beta) return stand_pat;
  if (stand_pat > alpha) alpha = stand_pat;

  MoveList list;
  generate_pseudo_legal(pos, list, true);
  int scores[256];
  score_moves(pos, list, scores, NO_MOVE, ply);

  int best = stand_pat;
  for (int i = 0; i < list.count; i++) {
    int pick = i;
    for (int j = i + 1; j < list.count; j++)
      if (scores[j] > scores[pick]) pick = j;
    std::swap(list.moves[i], list.moves[pick]);
    std::swap(scores[i], scores[pick]);
    Move m = list.moves[i];

    // Delta pruning: even winning the captured piece can't bring the score up to alpha.
    if (!is_promotion(m)) {
      int victim = move_flag(m) == EN_PASSANT ? PAWN : type_of(pos.piece_on(move_to(m)));
      if (stand_pat + PIECE_VALUE[victim] + 200 < alpha) continue;
    }

    pos.make(m);
    if (pos.left_king_in_check()) {
      pos.unmake();
      continue;
    }
    int score = -quiescence(pos, -beta, -alpha, ply + 1);
    pos.unmake();
    if (stop_.load(std::memory_order_relaxed)) return 0;

    if (score > best) best = score;
    if (score > alpha) {
      alpha = score;
      if (alpha >= beta) break;
    }
  }
  return best;
}

int Search::negamax(Position& pos, int depth, int alpha, int beta, int ply, bool allow_null) {
  pv_len_[ply] = ply;
  if (should_stop()) return 0;

  const bool in_check = pos.in_check();
  const bool is_pv = beta - alpha > 1;

  if (ply > 0) {
    if (pos.is_repetition() || pos.is_insufficient_material() || (pos.is_fifty_move_draw() && !in_check)) return 0;
    if (ply >= MAX_PLY - 1) return evaluate(pos);
    // Mate distance pruning: no line from here can beat a mate we've already found closer to the root.
    alpha = std::max(alpha, -MATE + ply);
    beta = std::min(beta, MATE - ply - 1);
    if (alpha >= beta) return alpha;
  }

  if (in_check) depth++;  // check extension
  if (depth <= 0) return quiescence(pos, alpha, beta, ply);
  nodes_++;

  // Transposition table
  const TTEntry* entry = tt_.probe(pos.key());
  Move tt_move = entry ? entry->move : NO_MOVE;
  if (ply == 0 && root_best_ != NO_MOVE) tt_move = root_best_;
  if (entry && !is_pv && ply > 0 && entry->depth >= depth) {
    int s = score_from_tt(entry->score, ply);
    if (entry->bound == BOUND_EXACT) return s;
    if (entry->bound == BOUND_LOWER && s >= beta) return s;
    if (entry->bound == BOUND_UPPER && s <= alpha) return s;
  }

  // Null-move pruning: if passing the move still leaves us above beta, this node will fail high.
  if (allow_null && !is_pv && !in_check && depth >= 3 && pos.has_non_pawn_material(pos.side_to_move()) &&
      evaluate(pos) >= beta) {
    int reduction = 2 + depth / 4;
    pos.make_null();
    int score = -negamax(pos, depth - 1 - reduction, -beta, -beta + 1, ply + 1, false);
    pos.unmake_null();
    if (stop_.load(std::memory_order_relaxed)) return 0;
    if (score >= beta) return score >= MATE_BOUND ? beta : score;
  }

  MoveList list;
  generate_pseudo_legal(pos, list);
  int scores[256];
  score_moves(pos, list, scores, tt_move, ply);

  const int alpha_orig = alpha;
  int best = -INF;
  Move best_move = NO_MOVE;
  int legal = 0;
  const Color us = pos.side_to_move();

  for (int i = 0; i < list.count; i++) {
    int pick = i;
    for (int j = i + 1; j < list.count; j++)
      if (scores[j] > scores[pick]) pick = j;
    std::swap(list.moves[i], list.moves[pick]);
    std::swap(scores[i], scores[pick]);
    Move m = list.moves[i];

    pos.make(m);
    if (pos.left_king_in_check()) {
      pos.unmake();
      continue;
    }
    legal++;
    const bool quiet = is_quiet(m);
    const bool gives_check = pos.in_check();

    int score;
    const int new_depth = depth - 1;
    if (legal == 1) {
      score = -negamax(pos, new_depth, -beta, -alpha, ply + 1, true);
    } else {
      // Late move reductions for quiet moves that come late in the ordering
      int reduction = 0;
      if (depth >= 3 && legal > 3 && quiet && !in_check && !gives_check) reduction = 1 + (depth >= 6 && legal > 8);
      score = -negamax(pos, new_depth - reduction, -alpha - 1, -alpha, ply + 1, true);
      if (score > alpha && reduction > 0) score = -negamax(pos, new_depth, -alpha - 1, -alpha, ply + 1, true);
      if (score > alpha && score < beta) score = -negamax(pos, new_depth, -beta, -alpha, ply + 1, true);
    }
    pos.unmake();
    if (stop_.load(std::memory_order_relaxed)) return 0;

    if (score > best) {
      best = score;
      best_move = m;
      if (score > alpha) {
        alpha = score;
        pv_[ply][ply] = m;
        for (int j = ply + 1; j < pv_len_[ply + 1]; j++) pv_[ply][j] = pv_[ply + 1][j];
        pv_len_[ply] = std::max(pv_len_[ply + 1], ply + 1);
        if (alpha >= beta) {
          if (quiet) {
            if (killers_[ply][0] != m) {
              killers_[ply][1] = killers_[ply][0];
              killers_[ply][0] = m;
            }
            int& h = history_[us][move_from(m)][move_to(m)];
            h += depth * depth;
            if (h > HISTORY_MAX) {
              for (auto& side : history_)
                for (auto& from : side)
                  for (int& v : from) v /= 2;
            }
          }
          break;
        }
      }
    }
  }

  if (legal == 0) return in_check ? -MATE + ply : 0;  // checkmate or stalemate

  Bound bound = best >= beta ? BOUND_LOWER : (best > alpha_orig ? BOUND_EXACT : BOUND_UPPER);
  tt_.store(pos.key(), best_move, score_to_tt(best, ply), depth, bound);
  return best;
}

void Search::report(int depth, int score) {
  if (!info_) return;
  int64_t ms = elapsed_ms();
  std::string line = "info depth " + std::to_string(depth);
  if (std::abs(score) >= MATE_BOUND) {
    int moves = score > 0 ? (MATE - score + 1) / 2 : -(MATE + score) / 2;
    line += " score mate " + std::to_string(moves);
  } else {
    line += " score cp " + std::to_string(score);
  }
  line += " nodes " + std::to_string(nodes_) + " nps " + std::to_string(nodes_ * 1000 / std::max<int64_t>(1, ms)) +
          " time " + std::to_string(ms) + " pv";
  for (int i = 0; i < pv_len_[0]; i++) line += " " + move_to_uci(pv_[0][i]);
  info_(line);
}

SearchResult Search::go(Position& pos, const SearchLimits& limits, InfoCallback info) {
  info_ = std::move(info);
  start_ = std::chrono::steady_clock::now();
  nodes_ = 0;
  root_best_ = NO_MOVE;
  time_checks_ = false;
  std::memset(killers_, 0, sizeof(killers_));
  for (auto& side : history_)
    for (auto& from : side)
      for (int& v : from) v /= 2;  // keep some history between moves, but let it fade

  SearchResult result;
  MoveList legal;
  generate_legal(pos, legal);
  if (legal.count == 0) return result;  // checkmate or stalemate: no move to play
  result.best = legal.moves[0];         // fallback if we're stopped before depth 1 finishes

  // Time budget (milliseconds)
  const int64_t overhead = 20;
  soft_ms_ = hard_ms_ = 0;
  if (limits.movetime > 0) {
    soft_ms_ = hard_ms_ = std::max<int64_t>(1, limits.movetime - overhead / 2);
  } else if (limits.use_clock && !limits.infinite) {
    const int us = pos.side_to_move();
    int64_t remaining = std::max<int64_t>(1, limits.time[us] - overhead);
    int mtg = limits.movestogo > 0 ? std::min(limits.movestogo, 50) : 30;
    int64_t base = remaining / mtg + limits.inc[us] * 3 / 4;
    soft_ms_ = std::max<int64_t>(1, std::min(base, remaining / 2));
    hard_ms_ = std::max<int64_t>(1, std::min(soft_ms_ * 3, remaining * 8 / 10));
  }

  int max_depth = limits.depth > 0 ? std::min(limits.depth, MAX_DEPTH) : MAX_DEPTH;
  const bool timed = !limits.infinite && limits.depth == 0 && soft_ms_ > 0;
  if (legal.count == 1 && timed) max_depth = 1;  // forced move: don't burn the clock

  for (int depth = 1; depth <= max_depth; depth++) {
    time_checks_ = depth > 1;  // always finish depth 1 so there is a real move to play
    int score = negamax(pos, depth, -INF, INF, 0, true);
    if (stop_.load() && depth > 1) break;  // iteration was cut short: its result is unreliable
    if (pv_len_[0] > 0) {
      result.best = pv_[0][0];
      result.ponder = pv_len_[0] > 1 ? pv_[0][1] : NO_MOVE;
      result.score = score;
      result.depth = depth;
      root_best_ = result.best;
      report(depth, score);
    }
    if (stop_.load()) break;
    if (timed && elapsed_ms() * 2 > soft_ms_) break;  // the next iteration would most likely not finish
    if (!limits.infinite && std::abs(score) >= MATE_BOUND && depth > MATE - std::abs(score)) break;  // mate proven
  }
  result.nodes = nodes_;
  return result;
}
