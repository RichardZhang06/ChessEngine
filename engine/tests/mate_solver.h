#pragma once
#include "movegen.h"

// Independent brute-force check: can the side to move force checkmate within `n` of its own moves?
inline bool forced_mate(Position& pos, int n) {
  MoveList moves;
  generate_legal(pos, moves);
  for (Move m : moves) {
    pos.make(m);
    MoveList replies;
    generate_legal(pos, replies);
    bool wins = false;
    if (replies.count == 0) {
      wins = pos.in_check();
    } else if (n > 1) {
      wins = true;
      for (Move r : replies) {
        pos.make(r);
        bool still = forced_mate(pos, n - 1);
        pos.unmake();
        if (!still) {
          wins = false;
          break;
        }
      }
    }
    pos.unmake();
    if (wins) return true;
  }
  return false;
}

