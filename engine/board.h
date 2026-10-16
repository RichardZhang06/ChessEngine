#ifndef BOARD_H
#define BOARD_H


#include <cstdint>

struct Board {
    // information about all of these is documented in ________ (fill once done)

    // all pieces represented as 64-bit bitboards, initialised to 0
    std::uint64_t white_pawns {};
    std::uint64_t white_bishops {};
    std::uint64_t white_knights {};
    std::uint64_t white_rooks {};
    std::uint64_t white_queens {};
    std::uint64_t white_kings {};

    std::uint64_t black_pawns {};
    std::uint64_t black_bishops {};
    std::uint64_t black_knights {};
    std::uint64_t black_rooks {};
    std::uint64_t black_queens {};
    std::uint64_t black_kings {};

    // boolean storing whose move is next
    bool white_to_move {true};

    // castling rights stored as 8 bits, using rightmost 4 bits as indicators
    // 1 means legal, 0 means illegal, bit's position corresponds to FEN "KQkq"
    std::uint8_t castling_rights {0b1111};

    // en passant square is -1 if not available, otherwise 0-63 (board square)
    std::int8_t en_passant_square {-1};

    // halfmove clock goes from 0 to 100
    std::uint8_t halfmove_clock {0};

    // indicates number of fullmoves in the game so far
    int fullmove_number {0};
};


#endif