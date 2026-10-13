// Simple deterministic UCI opponents of increasing strength, used to rate the real engine in regression tests.
//
//   Level 0  random legal move
//   Level 1  best capture (highest-value victim), otherwise random
//   Level 2+ material-only alpha-beta with quiescence, searching Level-1 plies
//            (2: 1 ply, 3: 2 plies, ... 6: 5 plies)
//
// Moves among equally scored candidates are picked pseudo-randomly from a seed and the position hash, so a
// given (Level, Seed) always plays the same moves in the same positions.
#include <iostream>
#include <sstream>
#include <vector>

#include "eval.h"
#include "movegen.h"
#include "position.h"

namespace {

constexpr int SAMPLE_MATE = 30000;

int material(const Position& pos) {
  int score = 0;
  for (int t = PAWN; t <= QUEEN; t++)
    score += (popcount(pos.pieces(pos.side_to_move(), PieceType(t))) - popcount(pos.pieces(~pos.side_to_move(), PieceType(t)))) *
             PIECE_VALUE[t];
  return score;
}

int victim_value(const Position& pos, Move m) {
  if (move_flag(m) == EN_PASSANT) return PIECE_VALUE[PAWN];
  return is_capture(m) ? PIECE_VALUE[type_of(pos.piece_on(move_to(m)))] : 0;
}

// Captures of valuable pieces first (stable, so equal moves keep generation order and results stay deterministic).
void order_by_victim(const Position& pos, MoveList& list) {
  int keys[256];
  for (int i = 0; i < list.count; i++) keys[i] = victim_value(pos, list.moves[i]) + (is_promotion(list.moves[i]) ? 800 : 0);
  for (int i = 1; i < list.count; i++) {  // insertion sort
    Move m = list.moves[i];
    int k = keys[i], j = i - 1;
    while (j >= 0 && keys[j] < k) {
      list.moves[j + 1] = list.moves[j];
      keys[j + 1] = keys[j];
      j--;
    }
    list.moves[j + 1] = m;
    keys[j + 1] = k;
  }
}

int quiescence(Position& pos, int alpha, int beta) {
  int stand = material(pos);
  if (stand >= beta) return stand;
  if (stand > alpha) alpha = stand;
  MoveList list;
  generate_pseudo_legal(pos, list, true);
  order_by_victim(pos, list);
  for (Move m : list) {
    pos.make(m);
    if (pos.left_king_in_check()) {
      pos.unmake();
      continue;
    }
    int score = -quiescence(pos, -beta, -alpha);
    pos.unmake();
    if (score >= beta) return score;
    if (score > alpha) alpha = score;
  }
  return alpha;
}

int alphabeta(Position& pos, int depth, int alpha, int beta, int ply) {
  if (depth == 0) return quiescence(pos, alpha, beta);
  MoveList list;
  generate_pseudo_legal(pos, list);
  order_by_victim(pos, list);
  int legal = 0;
  for (Move m : list) {
    pos.make(m);
    if (pos.left_king_in_check()) {
      pos.unmake();
      continue;
    }
    legal++;
    int score = pos.is_draw() ? 0 : -alphabeta(pos, depth - 1, -beta, -alpha, ply + 1);
    pos.unmake();
    if (score >= beta) return score;
    if (score > alpha) alpha = score;
  }
  if (legal == 0) return pos.in_check() ? -SAMPLE_MATE + ply : 0;
  return alpha;
}

struct Rng {
  uint64_t s;
  explicit Rng(uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL) {
    if (!s) s = 1;
    next();
    next();
  }
  uint64_t next() {
    s ^= s >> 12;
    s ^= s << 25;
    s ^= s >> 27;
    return s * 0x2545F4914F6CDD1DULL;
  }
  size_t pick(size_t n) { return size_t(next() % n); }
};

Move choose_move(Position& pos, int level, uint64_t seed) {
  MoveList legal;
  generate_legal(pos, legal);
  if (legal.count == 0) return NO_MOVE;
  Rng rng(seed ^ pos.key());

  std::vector<Move> best;
  if (level <= 0) {
    best.assign(legal.begin(), legal.end());
  } else if (level == 1) {
    int top = 0;
    for (Move m : legal) {
      int v = victim_value(pos, m);
      if (v > top) {
        top = v;
        best.clear();
      }
      if (v == top) best.push_back(m);
    }
  } else {
    int top = -SAMPLE_MATE - 1;
    for (Move m : legal) {
      pos.make(m);
      int score = pos.is_draw() ? 0 : -alphabeta(pos, level - 2, -SAMPLE_MATE - 1, SAMPLE_MATE + 1, 1);
      pos.unmake();
      if (score > top) {
        top = score;
        best.clear();
      }
      if (score == top) best.push_back(m);
    }
  }
  return best[rng.pick(best.size())];
}

}  // namespace

int main() {
  std::ios::sync_with_stdio(false);
  Position pos;
  int level = 3;
  uint64_t seed = 1;
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    if (cmd == "uci") {
      std::cout << "id name SampleEngine\nid author tests\n"
                << "option name Level type spin default 3 min 0 max 6\n"
                << "option name Seed type spin default 1 min 0 max 1000000\nuciok\n" << std::flush;
    } else if (cmd == "isready") {
      std::cout << "readyok\n" << std::flush;
    } else if (cmd == "setoption") {
      std::string token, name, value;
      in >> token;
      while (in >> token && token != "value") name += token;
      in >> value;
      if (name == "Level") level = std::stoi(value);
      if (name == "Seed") seed = std::stoull(value);
    } else if (cmd == "ucinewgame") {
      pos.set_startpos();
    } else if (cmd == "position") {
      std::string token;
      in >> token;
      if (token == "startpos") {
        pos.set_startpos();
        in >> token;
      } else if (token == "fen") {
        std::string fen;
        while (in >> token && token != "moves") fen += (fen.empty() ? "" : " ") + token;
        pos.set_fen(fen);
      }
      if (token == "moves") {
        std::string uci;
        while (in >> uci) {
          Move m = parse_uci_move(pos, uci);
          if (m == NO_MOVE) break;
          pos.make(m);
        }
      }
    } else if (cmd == "go") {
      std::cout << "bestmove " << move_to_uci(choose_move(pos, level, seed)) << "\n" << std::flush;
    } else if (cmd == "quit") {
      break;
    }
  }
  return 0;
}
