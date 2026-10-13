#include "book.h"

#include <fstream>
#include <sstream>

std::string OpeningBook::key(const Position& pos) {
  std::istringstream in(pos.fen());
  std::string placement, side, castling;
  in >> placement >> side >> castling;
  return placement + ' ' + side + ' ' + castling;
}

bool OpeningBook::load_text(const std::string& text) {
  entries_.clear();
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) continue;
    std::istringstream fields(line);
    std::string placement, side, castling, token;
    if (!(fields >> placement >> side >> castling)) {
      entries_.clear();
      return false;
    }
    auto& list = entries_[placement + ' ' + side + ' ' + castling];
    while (fields >> token) {
      size_t colon = token.find(':');
      if (colon == std::string::npos || colon < 4 || colon > 5) {
        entries_.clear();
        return false;
      }
      try {
        list.emplace_back(token.substr(0, colon), std::stoi(token.substr(colon + 1)));
      } catch (...) {
        entries_.clear();
        return false;
      }
    }
  }
  return true;
}

bool OpeningBook::load_file(const std::string& path) {
  std::ifstream f(path);
  if (!f) return false;
  std::stringstream ss;
  ss << f.rdbuf();
  return load_text(ss.str());
}

bool OpeningBook::load_embedded() {
  return load_text(std::string(reinterpret_cast<const char*>(EMBEDDED_BOOK), EMBEDDED_BOOK_SIZE));
}

std::vector<std::pair<Move, int>> OpeningBook::moves(Position& pos) const {
  std::vector<std::pair<Move, int>> out;
  auto it = entries_.find(key(pos));
  if (it == entries_.end()) return out;
  for (const auto& [uci, weight] : it->second) {
    Move m = parse_uci_move(pos, uci);  // ignore anything that isn't legal here
    if (m != NO_MOVE && weight > 0) out.emplace_back(m, weight);
  }
  return out;
}

Move OpeningBook::pick(Position& pos, std::mt19937_64& rng) const {
  auto candidates = moves(pos);
  if (candidates.empty()) return NO_MOVE;
  int total = 0;
  for (auto& c : candidates) total += c.second;
  int roll = int(rng() % uint64_t(total));
  for (auto& c : candidates) {
    if (roll < c.second) return c.first;
    roll -= c.second;
  }
  return candidates.back().first;
}
