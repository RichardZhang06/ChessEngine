#include "fen.h"
#include "board.h"

#include <string>
#include <cstdint>
#include <array>


// helper function to split the fen string into the necessary pieces
std::array<std::string, 6> split_fen_str(const std::string& fen) {
    std::array<std::string, 6> sections{};
    int section_index {0};
    for (size_t i; i < fen.length(); i++) {
        
    }
    return sections;
}


Board parse_fen(const std::string& fen_input){
    Board output_board {};
    return output_board;
};


