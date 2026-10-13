"""Tests for the rating maths and the sample opponents (run with pytest; needs python-chess and a built engine/)."""

import subprocess
import sys
from pathlib import Path

import chess
import pytest

sys.path.insert(0, str(Path(__file__).parent))
import regression as r  # noqa: E402

SAMPLE = r.BUILD / "sample_engine"
OPP = {str(i): 1000 + 200 * i for i in range(7)}


def test_expected_score():
    assert r.expected(1500, 1500) == pytest.approx(0.5)
    assert r.expected(1900, 1500) == pytest.approx(0.909, abs=0.001)
    assert r.expected(1500, 1900) == pytest.approx(1 - r.expected(1900, 1500))


def test_rating_matches_the_opponents_when_scoring_fifty_percent():
    per_level = {i: (10.0, 20) for i in range(7)}  # 50% against everyone
    rating, se = r.rate_engine(per_level, OPP)
    assert rating == pytest.approx(1600, abs=5)  # the middle of the ladder
    assert 0 < se < 100


def test_rating_goes_up_with_more_points():
    low = r.rate_engine({i: (5.0, 20) for i in range(7)}, OPP)[0]
    mid = r.rate_engine({i: (10.0, 20) for i in range(7)}, OPP)[0]
    high = r.rate_engine({i: (15.0, 20) for i in range(7)}, OPP)[0]
    assert low < mid < high


def test_rating_is_finite_for_perfect_and_zero_scores():
    top = r.rate_engine({i: (20.0, 20) for i in range(7)}, OPP)[0]
    bottom = r.rate_engine({i: (0.0, 20) for i in range(7)}, OPP)[0]
    assert OPP["6"] < top < 4000
    assert 0 < bottom < OPP["0"]


def test_fit_ratings_orders_players_and_centres_them():
    results = {(0, 1): (1.0, 20), (1, 2): (2.0, 20), (0, 2): (0.0, 20)}  # 2 > 1 > 0
    fitted = r.fit_ratings(results, [0, 1, 2])
    assert fitted[2] > fitted[1] > fitted[0]
    assert sum(fitted.values()) / 3 == pytest.approx(r.SCALE_MEAN)


def test_points_for_white():
    assert r.points_for_white("1-0") == 1
    assert r.points_for_white("0-1") == 0
    assert r.points_for_white("1/2-1/2") == 0.5


def ask(level: int, position_cmd: str, seed: int = 1) -> str:
    script = f"setoption name Level value {level}\nsetoption name Seed value {seed}\n{position_cmd}\ngo\nquit\n"
    out = subprocess.run([str(SAMPLE)], input=script, capture_output=True, text=True, timeout=60).stdout
    return next(line.split()[1] for line in out.splitlines() if line.startswith("bestmove"))


POSITIONS = [
    "position startpos",
    "position startpos moves e2e4 e7e5 g1f3 b8c6 f1b5 a7a6",
    "position fen r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "position fen 4k3/8/8/8/8/8/4q3/4K3 w - - 0 1",
]


@pytest.mark.skipif(not SAMPLE.is_file(), reason="sample_engine not built")
@pytest.mark.parametrize("level", range(7))
def test_sample_engine_plays_legal_and_deterministic_moves(level):
    for cmd in POSITIONS:
        board = chess.Board()
        if cmd.startswith("position fen"):
            board = chess.Board(cmd[len("position fen "):])
        elif " moves " in cmd:
            for m in cmd.split(" moves ")[1].split():
                board.push_uci(m)
        move = ask(level, cmd)
        assert chess.Move.from_uci(move) in board.legal_moves
        assert ask(level, cmd) == move  # same level + seed + position -> same move


@pytest.mark.skipif(not SAMPLE.is_file(), reason="sample_engine not built")
def test_sample_engine_takes_a_free_queen():
    # white can capture the hanging black queen with the rook; every level from 1 up should see it
    for level in range(1, 7):
        assert ask(level, "position fen 4k3/8/8/3q4/8/8/8/3RK3 w - - 0 1") == "d1d5"


@pytest.mark.skipif(not SAMPLE.is_file(), reason="sample_engine not built")
def test_sample_engine_finds_mate_in_one():
    for level in range(3, 7):  # levels 0-2 don't search deep enough to see the mate
        assert ask(level, "position fen 6k1/5ppp/8/8/8/8/8/R3K3 w - - 0 1") == "a1a8"
