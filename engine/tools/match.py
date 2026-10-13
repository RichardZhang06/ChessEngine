#!/usr/bin/env python3
"""Play the engine against Stockfish at limited strength to estimate its Elo.

Usage (from the repo root, using the backend's virtualenv for python-chess):
    backend/.venv/bin/python engine/tools/match.py --elo 1500 1800 2000 --games 40 --movetime 100

Each game starts from a short random opening (the same opening is played with both colors), so the
deterministic engines don't just repeat one game. Results go to the terminal and, as PGN, to --pgn.

Caveat: Stockfish's UCI_Elo is calibrated for long time controls (CCRL-style), and these games are played at a
fixed time per move, so treat the numbers as a rough strength estimate and use them to compare engine versions.
"""

import argparse
import math
import random
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import chess
import chess.engine
import chess.pgn

ROOT = Path(__file__).resolve().parents[2]
MAX_PLIES = 300


def random_opening(seed: int, plies: int = 6) -> list[chess.Move]:
    rng = random.Random(seed)
    while True:
        board = chess.Board()
        moves = []
        for _ in range(plies):
            move = rng.choice(list(board.legal_moves))
            board.push(move)
            moves.append(move)
            if board.is_game_over():
                break
        else:
            return moves


def play_game(args):
    """Returns (pgn_text, result_for_our_engine) where result is 1, 0.5 or 0."""
    engine_path, sf_path, sf_elo, movetime, opening_seed, engine_white, hash_mb = args
    limit = chess.engine.Limit(time=movetime / 1000)
    board = chess.Board()
    opening = random_opening(opening_seed)
    for m in opening:
        board.push(m)

    ours = chess.engine.SimpleEngine.popen_uci(str(engine_path))
    sf = chess.engine.SimpleEngine.popen_uci(sf_path)
    try:
        ours.configure({"Hash": hash_mb})
        sf.configure({"UCI_LimitStrength": True, "UCI_Elo": sf_elo, "Threads": 1, "Hash": hash_mb})
        forfeit = None
        while not board.is_game_over(claim_draw=True) and len(board.move_stack) < MAX_PLIES:
            our_turn = (board.turn == chess.WHITE) == engine_white
            player = ours if our_turn else sf
            try:
                move = player.play(board, limit).move
            except (chess.engine.EngineError, chess.engine.EngineTerminatedError) as e:
                forfeit = our_turn  # our engine crashed or errored -> it forfeits
                print(f"engine failure: {e!r}", file=sys.stderr)
                break
            if move is None or move not in board.legal_moves:
                forfeit = our_turn
                print(f"illegal/no move {move} in {board.fen()}", file=sys.stderr)
                break
            board.push(move)
    finally:
        for e in (ours, sf):
            try:
                e.quit()
            except Exception:
                pass

    if forfeit is not None:
        score = 0.0 if forfeit else 1.0
        result = "0-1" if (forfeit == engine_white) else "1-0"
    else:
        outcome = board.outcome(claim_draw=True)
        result = outcome.result() if outcome else "1/2-1/2"  # ply cap counts as a draw
        if result == "1/2-1/2":
            score = 0.5
        else:
            score = 1.0 if (result == "1-0") == engine_white else 0.0

    game = chess.pgn.Game.from_board(board)
    game.headers["White"] = "ChessEngine" if engine_white else f"Stockfish {sf_elo}"
    game.headers["Black"] = f"Stockfish {sf_elo}" if engine_white else "ChessEngine"
    game.headers["Result"] = result
    return str(game), score


def elo_estimate(wins: int, draws: int, losses: int):
    """Elo difference and 95% error bar from a match result (logistic model)."""
    n = wins + draws + losses
    s = (wins + draws / 2) / n
    s = min(max(s, 0.5 / n), 1 - 0.5 / n)  # keep the logit finite for 0%/100% scores
    diff = -400 * math.log10(1 / s - 1)
    var = (wins * (1 - s) ** 2 + draws * (0.5 - s) ** 2 + losses * s**2) / n
    err = 1.96 * math.sqrt(var / n)
    lo, hi = min(max(s - err, 1e-3), 1 - 1e-3), min(max(s + err, 1e-3), 1 - 1e-3)
    return diff, (-400 * math.log10(1 / hi - 1) - -400 * math.log10(1 / lo - 1)) / 2


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--engine", default=str(ROOT / "engine" / "build" / "engine"))
    p.add_argument("--stockfish", default="stockfish")
    p.add_argument("--elo", type=int, nargs="+", default=[1500, 1800], help="Stockfish UCI_Elo levels (1320-3190)")
    p.add_argument("--games", type=int, default=40, help="games per Elo level (rounded up to an even number)")
    p.add_argument("--movetime", type=int, default=100, help="milliseconds per move for both engines")
    p.add_argument("--workers", type=int, default=4, help="games to run in parallel")
    p.add_argument("--hash", type=int, default=16)
    p.add_argument("--seed", type=int, default=1)
    p.add_argument("--pgn", default="match.pgn")
    a = p.parse_args()

    pairs = (a.games + 1) // 2
    jobs, labels = [], []
    for elo in a.elo:
        for i in range(pairs):
            for white in (True, False):
                jobs.append((a.engine, a.stockfish, elo, a.movetime, a.seed * 100000 + i, white, a.hash))
                labels.append(elo)

    print(f"{len(jobs)} games, {a.movetime} ms/move, {a.workers} in parallel", flush=True)
    totals = {elo: [0, 0, 0] for elo in a.elo}  # wins, draws, losses
    with ProcessPoolExecutor(a.workers) as pool, open(a.pgn, "w") as pgn:
        for done, ((text, score), elo) in enumerate(zip(pool.map(play_game, jobs), labels), 1):
            totals[elo][0 if score == 1 else 1 if score == 0.5 else 2] += 1
            pgn.write(text + "\n\n")
            if done % 10 == 0:
                print(f"  {done}/{len(jobs)} games done", flush=True)

    print("\nvs Stockfish (UCI_Elo)   W   D   L   score   engine Elo estimate")
    estimates = []
    for elo, (w, d, l) in totals.items():
        n = w + d + l
        diff, err = elo_estimate(w, d, l)
        score = (w + d / 2) / n
        informative = 0.05 < score < 0.95  # 0% / 100% results only give a bound
        print(f"  {elo:>5}              {w:>4}{d:>4}{l:>4}   {score:5.1%}   " +
              (f"{elo + diff:.0f} +/- {err:.0f}" if informative else f"{'>' if score >= 0.95 else '<'} {elo + diff:.0f} (one-sided)"))
        if informative:
            estimates.append(elo + diff)
    if estimates:
        print(f"\nOverall estimate (informative levels only): ~{sum(estimates) / len(estimates):.0f} Elo")
    print(f"Games saved to {a.pgn}")


if __name__ == "__main__":
    main()
