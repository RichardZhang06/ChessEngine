#!/usr/bin/env python3
"""Strength regression test: rate the engine against the sample opponents and compare with a stored baseline.

The opponents (tools/sample_engine.cpp, levels 0-6) and the engine under test (searching to a fixed depth) are all
deterministic, so every run plays exactly the same games for the same code. The engine's rating is fitted on an
internal scale defined by tools/ratings.json, and the check fails if

  * the engine played an illegal move, crashed or hung,
  * it scores less than 90% against the two weakest opponents, or
  * its rating dropped by more than --max-drop below the stored baseline.

    python3 engine/tools/regression.py                  # run and check (what CI does)
    python3 engine/tools/regression.py --calibrate      # re-fit the opponents' ratings (round robin between them)
    python3 engine/tools/regression.py --update-baseline  # store the current rating as the new baseline

Needs python-chess (the backend's virtualenv has it) and a built engine (cmake -B build && cmake --build build).
"""

import argparse
import json
import math
import random
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import chess
import chess.engine

ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "engine" / "build"
RATINGS_FILE = Path(__file__).with_name("ratings.json")
LEVELS = [0, 1, 2, 3, 4, 5, 6]
MAX_PLIES = 240
OPENING_PLIES = 4
SCALE_MEAN = 1500  # opponents' ratings are centred here


# ---------------------------------------------------------------- rating maths

def expected(r_a: float, r_b: float) -> float:
    return 1 / (1 + 10 ** ((r_b - r_a) / 400))


def fit_ratings(results: dict, players: list) -> dict:
    """Maximum-likelihood ratings from {(a, b): score_of_a_summed} / game counts, centred on SCALE_MEAN.

    results[(a, b)] = (points_for_a, games). One virtual draw per pair keeps ratings finite for 100% scores.
    """
    pairs = {}
    for (a, b), (pts, n) in results.items():
        pairs[(a, b)] = (pts + 0.5, n + 1)
    for a in players:
        for b in players:
            if a < b and (a, b) not in pairs and (b, a) not in pairs:
                pairs[(a, b)] = (0.5, 1)
    r = {p: float(SCALE_MEAN) for p in players}
    k = math.log(10) / 400
    for _ in range(500):
        for p in players:
            grad = info = 0.0
            for (a, b), (pts, n) in pairs.items():
                if p == a:
                    e = expected(r[a], r[b])
                    grad += pts - n * e
                    info += n * e * (1 - e)
                elif p == b:
                    e = expected(r[b], r[a])
                    grad += (n - pts) - n * e
                    info += n * e * (1 - e)
            if info:
                r[p] += max(-100.0, min(100.0, grad / (info * k)))
    mean = sum(r.values()) / len(r)
    return {p: v - mean + SCALE_MEAN for p, v in r.items()}


def rate_engine(per_level: dict, opp_ratings: dict):
    """Rating (and standard error) of the engine from {level: (points, games)} against rated opponents."""
    k = math.log(10) / 400
    entries = [(opp_ratings[str(lvl)], pts + 0.5, n + 1) for lvl, (pts, n) in per_level.items()]  # + a virtual draw each
    lo, hi = 0.0, 4000.0
    for _ in range(100):
        mid = (lo + hi) / 2
        if sum(pts - n * expected(mid, ro) for ro, pts, n in entries) > 0:
            lo = mid  # the engine scores more than expected at `mid`, so its rating is higher
        else:
            hi = mid
    r = (lo + hi) / 2
    info = sum(n * expected(r, ro) * (1 - expected(r, ro)) for ro, _, n in entries) * k * k
    return r, 1 / math.sqrt(info)


# ---------------------------------------------------------------- playing games

def opening(seed: int) -> list:
    rng = random.Random(seed)
    while True:
        board, moves = chess.Board(), []
        for _ in range(OPENING_PLIES):
            m = rng.choice(list(board.legal_moves))
            board.push(m)
            moves.append(m)
        if not board.is_game_over():
            return moves


def open_engine(path, options):
    eng = chess.engine.SimpleEngine.popen_uci(str(path), timeout=60)
    if options:
        eng.configure(options)
    return eng


def play_game(job):
    """job: (white_spec, black_spec, opening_seed). spec: (path, options, depth|None). Returns (result, problem)."""
    white, black, seed = job
    board = chess.Board()
    for m in opening(seed):
        board.push(m)
    engines = {chess.WHITE: open_engine(white[0], white[1]), chess.BLACK: open_engine(black[0], black[1])}
    limits = {chess.WHITE: chess.engine.Limit(depth=white[2]) if white[2] else chess.engine.Limit(nodes=1),
              chess.BLACK: chess.engine.Limit(depth=black[2]) if black[2] else chess.engine.Limit(nodes=1)}
    problem = None
    try:
        while not board.is_game_over(claim_draw=True) and len(board.move_stack) < MAX_PLIES:
            side = board.turn
            try:
                move = engines[side].play(board, limits[side]).move
            except (chess.engine.EngineError, chess.engine.EngineTerminatedError, TimeoutError) as e:
                problem = (side, f"engine error/crash/hang: {e!r}", board.fen())
                break
            if move is None or move not in board.legal_moves:
                problem = (side, f"illegal or missing move {move}", board.fen())
                break
            board.push(move)
    finally:
        for e in engines.values():
            try:
                e.quit()
            except Exception:
                pass
    if problem:
        return ("0-1" if problem[0] == chess.WHITE else "1-0"), problem
    outcome = board.outcome(claim_draw=True)
    return (outcome.result() if outcome else "1/2-1/2"), None


def points_for_white(result: str) -> float:
    return {"1-0": 1.0, "0-1": 0.0}.get(result, 0.5)


# ---------------------------------------------------------------- commands

def sample(level: int, seed: int, path: Path):
    return (path, {"Level": level, "Seed": seed}, None)


def run_pool(jobs, workers):
    with ProcessPoolExecutor(workers) as pool:
        return list(pool.map(play_game, jobs))


def calibrate(args):
    sample_path = BUILD / "sample_engine"
    pairs = args.games // 2
    jobs, keys = [], []
    for a in LEVELS:
        for b in LEVELS:
            if a < b:
                for i in range(pairs):
                    jobs.append((sample(a, 11, sample_path), sample(b, 22, sample_path), 1000 + i))
                    keys.append((a, b, True))
                    jobs.append((sample(b, 22, sample_path), sample(a, 11, sample_path), 1000 + i))
                    keys.append((a, b, False))
    print(f"calibrating: {len(jobs)} games between the sample opponents", flush=True)
    results = {}
    for (res, problem), (a, b, a_white) in zip(run_pool(jobs, args.workers), keys):
        pts_white = points_for_white(res)
        pts_a = pts_white if a_white else 1 - pts_white
        pts, n = results.get((a, b), (0.0, 0))
        results[(a, b)] = (pts + pts_a, n + 1)
    ratings = fit_ratings(results, LEVELS)
    data = json.loads(RATINGS_FILE.read_text()) if RATINGS_FILE.exists() else {}
    data["opponents"] = {str(l): round(r) for l, r in ratings.items()}
    RATINGS_FILE.write_text(json.dumps(data, indent=2) + "\n")
    print("opponent ratings:", data["opponents"])


def evaluate(args, data):
    cfg = data["engine"]
    engine_path = Path(args.engine)
    sample_path = BUILD / "sample_engine"
    depth, games = args.depth or cfg["depth"], args.games or cfg["games_per_level"]
    pairs = (games + 1) // 2
    ours = (engine_path, {"OwnBook": False}, depth)  # measure the search, not the book
    jobs, keys = [], []
    for lvl in LEVELS:
        for i in range(pairs):
            for white in (True, False):
                opp = sample(lvl, 7, sample_path)
                jobs.append((ours, opp, 5000 + i) if white else (opp, ours, 5000 + i))
                keys.append((lvl, white))
    print(f"engine depth {depth} vs sample levels {LEVELS}: {len(jobs)} games", flush=True)

    per_level = {l: (0.0, 0) for l in LEVELS}
    detail = {l: [0, 0, 0] for l in LEVELS}
    problems = []
    for (res, problem), (lvl, white) in zip(run_pool(jobs, args.workers), keys):
        pts_white = points_for_white(res)
        pts = pts_white if white else 1 - pts_white
        p, n = per_level[lvl]
        per_level[lvl] = (p + pts, n + 1)
        detail[lvl][0 if pts == 1 else 1 if pts == 0.5 else 2] += 1
        if problem and problem[0] == (chess.WHITE if white else chess.BLACK):
            problems.append((lvl, problem))

    rating, se = rate_engine(per_level, data["opponents"])
    print("\nopponent level  rating    W   D   L   score")
    for lvl in LEVELS:
        w, d, l = detail[lvl]
        pts, n = per_level[lvl]
        print(f"      {lvl}          {data['opponents'][str(lvl)]:>5}   {w:>3} {d:>3} {l:>3}   {pts / n:5.1%}")
    print(f"\nengine rating: {rating:.0f} +/- {1.96 * se:.0f} (95%)   baseline: {cfg['baseline_rating']}   "
          f"allowed drop: {cfg['max_drop']}")

    failures = []
    for lvl, (_, msg, fen) in problems:
        failures.append(f"engine misbehaved vs level {lvl}: {msg} at {fen}")
    for lvl in (0, 1):
        pts, n = per_level[lvl]
        if pts / n < 0.9:
            failures.append(f"scored only {pts / n:.0%} against sample level {lvl} (expected at least 90%)")
    if rating < cfg["baseline_rating"] - cfg["max_drop"]:
        failures.append(f"rating {rating:.0f} is more than {cfg['max_drop']} below the baseline {cfg['baseline_rating']}")
    return rating, failures


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--engine", default=str(BUILD / "engine"))
    p.add_argument("--workers", type=int, default=4)
    p.add_argument("--games", type=int, default=0, help="games per opponent level (default: from ratings.json)")
    p.add_argument("--depth", type=int, default=0, help="search depth of the engine (default: from ratings.json)")
    p.add_argument("--calibrate", action="store_true", help="re-fit the opponents' ratings; --games is per pair")
    p.add_argument("--update-baseline", action="store_true", help="store the measured rating as the baseline")
    args = p.parse_args()

    if args.calibrate:
        args.games = args.games or 40
        calibrate(args)
        return 0
    data = json.loads(RATINGS_FILE.read_text())
    rating, failures = evaluate(args, data)
    if args.update_baseline:
        data["engine"]["baseline_rating"] = round(rating)
        RATINGS_FILE.write_text(json.dumps(data, indent=2) + "\n")
        print(f"baseline updated to {round(rating)}")
        return 1 if any("misbehaved" in f for f in failures) else 0
    if failures:
        print("\nREGRESSION CHECK FAILED:")
        for f in failures:
            print(" -", f)
        return 1
    print("\nregression check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
