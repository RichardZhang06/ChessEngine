#pragma once
#include <condition_variable>
#include <functional>
#include <istream>
#include <memory>
#include <random>
#include <mutex>
#include <string>
#include <thread>

#include "book.h"
#include "position.h"
#include "search.h"
#include "tt.h"

// UCI front end (see UCI_protocol.txt). Reads commands, drives the search on a background thread
// so that `stop` and `isready` are answered while thinking, and writes replies through `out`.
class Uci {
 public:
  using Output = std::function<void(const std::string&)>;

  explicit Uci(Output out);
  ~Uci();

  // Reads commands until `quit` or end of input.
  void loop(std::istream& in);
  // Handles one command line. Returns false once the engine should exit (`quit`).
  bool handle(const std::string& line);
  // Blocks until the current search (if any) has printed its bestmove.
  void wait_for_search();

 private:
  void cmd_position(std::istringstream& args);
  void cmd_go(std::istringstream& args);
  void cmd_setoption(std::istringstream& args);
  void cmd_perft(int depth);
  void start_search(const SearchLimits& limits);
  void stop_search();
  void send(const std::string& line);

  bool play_book_move();

  Output out_;
  std::mutex out_mutex_;
  Position pos_;
  TranspositionTable tt_;
  Search search_;
  OpeningBook book_;
  bool own_book_ = false;
  std::mt19937_64 book_rng_;
  std::thread worker_;

  // Lets an `infinite` search wait for `stop` before reporting its best move.
  std::mutex stop_mutex_;
  std::condition_variable stop_cv_;
  bool stop_requested_ = false;
};
