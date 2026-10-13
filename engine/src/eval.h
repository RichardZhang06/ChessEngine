#pragma once
#include "position.h"

constexpr int PIECE_VALUE[6] = {100, 320, 330, 500, 900, 0};

// Static evaluation in centipawns from the point of view of the side to move.
int evaluate(const Position& pos);
