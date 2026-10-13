#include "uci.h"

#include <iostream>
#include <sstream>

#include "movegen.h"

namespace {
constexpr const char* ENGINE_NAME = "ChessEngine 1.0";
constexpr const char* ENGINE_AUTHOR = "Richard Zhang";
constexpr int DEFAULT_HASH_MB = 16;
}  // namespace

Uci::Uci(Output out) : out_(std::move(out)), tt_(DEFAULT_HASH_MB), search_(tt_) {}

Uci::~Uci() { stop_search(); }

void Uci::send(const std::string& line) {
  std::lock_guard<std::mutex> lock(out_mutex_);
  out_(line);
}

void Uci::loop(std::istream& in) {
  std::string line;
  while (std::getline(in, line))
    if (!handle(line)) return;
  stop_search();  // end of input: shut down like `quit`
}

void Uci::wait_for_search() {
  if (worker_.joinable()) worker_.join();
}

void Uci::stop_search() {
  if (!worker_.joinable()) return;
  search_.stop();
  {
    std::lock_guard<std::mutex> lock(stop_mutex_);
    stop_requested_ = true;
  }
  stop_cv_.notify_all();
  worker_.join();
}

bool Uci::handle(const std::string& line) {
  std::istringstream args(line);
  std::string cmd;
  if (!(args >> cmd)) return true;

  if (cmd == "uci") {
    send(std::string("id name ") + ENGINE_NAME);
    send(std::string("id author ") + ENGINE_AUTHOR);
    send("option name Hash type spin default " + std::to_string(DEFAULT_HASH_MB) + " min 1 max 4096");
    send("uciok");
  } else if (cmd == "isready") {
    send("readyok");
  } else if (cmd == "setoption") {
    cmd_setoption(args);
  } else if (cmd == "ucinewgame") {
    stop_search();
    tt_.clear();
    search_.new_game();
    pos_.set_startpos();
  } else if (cmd == "position") {
    stop_search();
    cmd_position(args);
  } else if (cmd == "go") {
    stop_search();
    cmd_go(args);
  } else if (cmd == "stop") {
    stop_search();
  } else if (cmd == "quit") {
    stop_search();
    return false;
  } else if (cmd == "d") {
    send(pos_.to_string());
  }
  // anything else (including `ponderhit`, `debug`, `register`) is ignored, as the protocol asks for unknown input
  return true;
}

void Uci::cmd_setoption(std::istringstream& args) {
  // setoption name <id> [value <x>]; the name may contain spaces
  std::string token, name, value;
  args >> token;  // "name"
  while (args >> token && token != "value") name += (name.empty() ? "" : " ") + token;
  std::getline(args, value);
  size_t first = value.find_first_not_of(' ');
  value = first == std::string::npos ? "" : value.substr(first);

  for (char& c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
  if (name == "hash") {
    try {
      int mb = std::stoi(value);
      if (mb < 1) mb = 1;
      if (mb > 4096) mb = 4096;
      stop_search();
      tt_.resize(size_t(mb));
    } catch (...) {
      send("info string invalid Hash value: " + value);
    }
  }
}

void Uci::cmd_position(std::istringstream& args) {
  std::string token;
  args >> token;
  Position next;
  if (token == "startpos") {
    next.set_startpos();
    args >> token;  // "moves" or nothing
  } else if (token == "fen") {
    std::string fen;
    while (args >> token && token != "moves") fen += (fen.empty() ? "" : " ") + token;
    if (!next.set_fen(fen)) {
      send("info string invalid fen: " + fen);
      return;
    }
  } else {
    return;
  }

  if (token == "moves") {
    std::string uci;
    while (args >> uci) {
      Move m = parse_uci_move(next, uci);
      if (m == NO_MOVE) {
        send("info string illegal move: " + uci);
        break;  // keep the position reached so far
      }
      next.make(m);
    }
  }
  pos_ = next;
}

void Uci::cmd_go(std::istringstream& args) {
  SearchLimits limits;
  std::string token;
  bool any = false;
  auto number = [&](int64_t& out) {
    long long v = 0;
    if (args >> v) out = v;
  };
  while (args >> token) {
    any = true;
    int64_t v = 0;
    if (token == "wtime") { number(limits.time[WHITE]); limits.use_clock = true; }
    else if (token == "btime") { number(limits.time[BLACK]); limits.use_clock = true; }
    else if (token == "winc") number(limits.inc[WHITE]);
    else if (token == "binc") number(limits.inc[BLACK]);
    else if (token == "movestogo") { number(v); limits.movestogo = int(v); }
    else if (token == "depth") { number(v); limits.depth = int(v); }
    else if (token == "movetime") number(limits.movetime);
    else if (token == "infinite" || token == "ponder") limits.infinite = true;
    else if (token == "perft") { number(v); cmd_perft(int(v)); return; }
    else if (token == "nodes" || token == "mate") number(v);  // accepted but not supported
  }
  if (!any) limits.infinite = true;  // plain `go` means think until told to stop
  start_search(limits);
}

void Uci::cmd_perft(int depth) {
  uint64_t total = 0;
  MoveList moves;
  generate_legal(pos_, moves);
  for (Move m : moves) {
    pos_.make(m);
    uint64_t n = depth > 1 ? perft(pos_, depth - 1) : 1;
    pos_.unmake();
    send(move_to_uci(m) + ": " + std::to_string(n));
    total += n;
  }
  send("Nodes searched: " + std::to_string(total));
}

void Uci::start_search(const SearchLimits& limits) {
  {
    std::lock_guard<std::mutex> lock(stop_mutex_);
    stop_requested_ = false;
  }
  worker_ = std::thread([this, limits] {
    Position pos = pos_;
    SearchResult r = search_.go(pos, limits, [this](const std::string& info) { send(info); });
    if (limits.infinite) {
      // The protocol forbids a bestmove before `stop` in infinite mode.
      std::unique_lock<std::mutex> lock(stop_mutex_);
      stop_cv_.wait(lock, [this] { return stop_requested_; });
    }
    std::string line = "bestmove " + move_to_uci(r.best);
    if (r.ponder != NO_MOVE) line += " ponder " + move_to_uci(r.ponder);
    send(line);
  });
}
