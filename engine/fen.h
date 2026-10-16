#ifndef FEN_H
#define FEN_H


#include "board.h"
#include <string>

// using const std::string& to avoid copying while preventing altering input
Board parse_fen(const std::string& fen_input);

// using const Board& to avoid copying while preventing altering input
std::string board_to_fen(const Board& board_input);


#endif