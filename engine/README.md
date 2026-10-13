# Engine

A UCI chess engine in C++17 (see `UCI_protocol.txt` at the repo root). The backend talks to it over stdin/stdout.

## Build and test

```
cd engine
cmake -B build && cmake --build build -j
ctest --test-dir build --output-on-failure
```

This produces `engine/build/engine`, which is where the backend looks for it by default
(override with `ENGINE_PATH`). Run it by hand and type UCI commands:

```
./build/engine
uci
position startpos moves e2e4
go movetime 1000
```

Extras: `d` prints the board, `go perft N` prints per-move node counts.

## Measuring strength

`tools/match.py` plays the engine against Stockfish at limited strength (`UCI_Elo`) and prints W/D/L plus an Elo
estimate. It needs Stockfish (`brew install stockfish`) and python-chess (the backend's virtualenv has it):

```
backend/.venv/bin/python engine/tools/match.py --elo 1600 1900 2200 --games 40 --movetime 100 --workers 8
```

Each opening is played with both colors. Stockfish's `UCI_Elo` is calibrated for long time controls, so at
100 ms per move the absolute numbers are unreliable (higher levels come out much weaker than nominal). Use it mainly
to compare two versions of this engine: build both and run each against the same Stockfish levels.

## Layout

| File | What it does |
| ---- | ------------ |
| `src/types.h`, `bitboard.*` | Squares, pieces, move encoding, attack tables |
| `src/position.*` | Board state, FEN, make/unmake, Zobrist hashing, draw detection |
| `src/movegen.*` | Legal move generation, perft, checkmate/stalemate/draw detection |
| `src/eval.*` | Material, piece-square tables, bishop pair, passed pawns |
| `src/tt.*` | Transposition table |
| `src/search.*` | Iterative deepening alpha-beta (PVS, null move, LMR, quiescence) and time management |
| `src/uci.*`, `src/main.cpp` | UCI protocol loop |
| `tests/*_test.cpp` | Perft (published node counts), position, eval, search (mates checked by brute force), TT, UCI |

## Options

`Hash` (MB, default 16, range 1-4096).

## Ideas for later

Magic bitboards, SEE, aspiration windows, a better evaluation (mobility, king safety, pawn structure),
an opening book, pondering and multithreading.
