#!/usr/bin/env python3
"""Build the opening book (book/openings.txt) from the Lichess opening dataset in book/sources/*.tsv.

Source: https://github.com/lichess-org/chess-openings (CC0). Each line of the dataset is a named opening as a
sequence of moves. For every position along every line we count how many lines continue with each move, so
moves backed by more named variations are picked more often.

Output, one line per position (move counts are the weights):

    <piece placement> <side to move> <castling> <uci>:<weight> <uci>:<weight> ...

    backend/.venv/bin/python engine/tools/build_book.py
"""

import csv
import re
from collections import defaultdict
from pathlib import Path

import chess

BOOK_DIR = Path(__file__).resolve().parents[1] / "book"
MAX_PLIES = 24  # stop following a line after this many half-moves
MIN_SHARE = 0.05  # drop moves played in fewer than this share of the most popular move's lines


def position_key(board: chess.Board) -> str:
    # The en passant square and move counters are left out: they rarely matter and make transpositions miss.
    return " ".join(board.fen().split()[:3])


def main():
    book = defaultdict(lambda: defaultdict(int))
    lines = 0
    for tsv in sorted((BOOK_DIR / "sources").glob("*.tsv")):
        with open(tsv, newline="") as f:
            for row in csv.DictReader(f, delimiter="\t"):
                board = chess.Board()
                tokens = [t for t in re.sub(r"\d+\.", " ", row["pgn"]).split() if t]
                for ply, san in enumerate(tokens[:MAX_PLIES]):
                    move = board.parse_san(san)
                    book[position_key(board)][move.uci()] += 1
                    board.push(move)
                lines += 1

    out = BOOK_DIR / "openings.txt"
    with open(out, "w") as f:
        for key in sorted(book):
            moves = sorted(book[key].items(), key=lambda kv: (-kv[1], kv[0]))
            moves = [(m, w) for m, w in moves if w >= MIN_SHARE * moves[0][1]]
            f.write(key + " " + " ".join(f"{m}:{w}" for m, w in moves) + "\n")
    print(f"{lines} opening lines -> {len(book)} positions in {out}")


if __name__ == "__main__":
    main()
