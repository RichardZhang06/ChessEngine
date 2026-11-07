#ifndef FEN_H
#define FEN_H


#include "board.h"
#include <string>
#include <array>

// using const std::string& to avoid copying while preventing altering input
Board parse_fen(const std::string& fen_input);

// using const Board& to avoid copying while preventing altering input
std::string board_to_fen(const Board& board_input);

// returns 6 length string array, containing each section of the FEN
std::array<std::string, 6> split_fen_str(const std::string& fen);


#endif