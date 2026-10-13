#pragma once
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "movegen.h"

// Opening book: positions (by piece placement, side to move and castling rights) mapped to weighted moves.
// The default book is compiled into the binary from book/openings.txt (built by tools/build_book.py).
class OpeningBook {
 public:
  // Replaces the contents. Returns false (leaving the book empty) if the text is malformed.
  bool load_text(const std::string& text);
  bool load_file(const std::string& path);
  bool load_embedded();

  size_t size() const { return entries_.size(); }
  bool empty() const { return entries_.empty(); }

  // A legal book move for the position chosen at random in proportion to its weight, or NO_MOVE.
  Move pick(Position& pos, std::mt19937_64& rng) const;
  // Every legal book move for the position with its weight.
  std::vector<std::pair<Move, int>> moves(Position& pos) const;

  static std::string key(const Position& pos);

  // Calls fn(key, uci, weight) for every stored move (used by tests to validate the whole book).
  template <typename F>
  void for_each(F fn) const {
    for (const auto& [k, list] : entries_)
      for (const auto& [uci, weight] : list) fn(k, uci, weight);
  }

 private:
  std::unordered_map<std::string, std::vector<std::pair<std::string, int>>> entries_;
};

extern const unsigned char EMBEDDED_BOOK[];
extern const size_t EMBEDDED_BOOK_SIZE;
