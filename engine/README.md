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

## Strength regression test

`tools/regression.py` rates the engine against deterministic sample opponents (`tools/sample_engine.cpp`: levels 0-6,
from a random mover to a material-only search 5 plies deep) and fails CI if the rating falls more than `max_drop`
(150) below the stored baseline, if the engine plays an illegal move, crashes or hangs, or if it scores under 90%
against the two weakest opponents. About 420 games take ~20 s on 8 cores.

```
backend/.venv/bin/python engine/tools/regression.py                    # check against the baseline
backend/.venv/bin/python engine/tools/regression.py --update-baseline  # after an intentional strength change
backend/.venv/bin/python engine/tools/regression.py --calibrate        # re-fit the opponents' ratings (rarely needed)
backend/.venv/bin/pytest engine/tools/test_regression.py               # tests for the rating maths and opponents
```

The engine searches to a fixed depth (3) and the opponents are seeded, so the same code always plays the same games
and gets the same rating on any machine. The ratings are on an internal scale (fitted from games between the
opponents, centred on 1500), not comparable to Lichess or FIDE Elo. A change that adds strength raises the number;
if you intentionally trade strength for something else, update the baseline in the same commit and say so.

## Layout

| File | What it does |
| ---- | ------------ |
| `src/types.h`, `bitboard.*` | Squares, pieces, move encoding, attack tables |
| `src/position.*` | Board state, FEN, make/unmake, Zobrist hashing, draw detection |
| `src/movegen.*` | Legal move generation, perft, checkmate/stalemate/draw detection |
| `src/eval.*` | Material, piece-square tables, bishop pair, passed pawns |
| `src/tt.*` | Transposition table |
| `src/book.*`, `book/` | Opening book (compiled in), built from the Lichess openings dataset |
| `src/search.*` | Iterative deepening alpha-beta (PVS, null move, LMR, quiescence) and time management |
| `src/uci.*`, `src/main.cpp` | UCI protocol loop |
| `tools/` | `match.py` (vs Stockfish), `regression.py` + `sample_engine.cpp` (rating regression test), `ratings.json` |
| `tests/*_test.cpp` | Perft (published node counts), position, eval, search (mates checked by brute force), TT, UCI |

## Options

- `Hash` (MB, default 16, range 1-4096)
- `OwnBook` (default false): answer `go` from the opening book while the game is in known theory. Ignored for
  `go infinite`. The backend turns it on.
- `BookFile` (default `<embedded>`): load a different book (same text format as `book/openings.txt`).
- `BookSeed` (default 0): seed for choosing among book moves; 0 gives a different choice each time.

The default book is built from the Lichess opening dataset (CC0) by `tools/build_book.py` and compiled into the
binary; see `book/README.md`.

## Ideas for later

Magic bitboards, SEE, aspiration windows, a better evaluation (mobility, king safety, pawn structure),
an endgame tablebase, a larger opening book, pondering and multithreading.
