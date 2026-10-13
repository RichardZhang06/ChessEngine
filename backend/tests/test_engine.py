import sys
from pathlib import Path

import pytest

from app.engine import EngineError, RandomEngine, UCIEngine, build_engine

FAKE = Path(__file__).parent / "fake_uci_engine.py"


def script_engine(*flags: str, **kw) -> UCIEngine:
    """UCIEngine that launches the fake engine script through the current interpreter."""
    return UCIEngine(Path(sys.executable), args=(str(FAKE), *flags), **kw)


async def test_best_move_from_startpos():
    engine = script_engine()
    try:
        assert await engine.best_move([], 50) == "a2a3"
    finally:
        await engine.close()


async def test_position_is_sent_with_moves():
    engine = script_engine()
    try:
        # the fake engine plays the first legal move in UCI order: a7a5 sorts before a7a6
        assert await engine.best_move(["a2a3"], 50) == "a7a5"
    finally:
        await engine.close()


async def test_options_are_sent_after_the_handshake():
    plain, with_book = script_engine(), UCIEngine(Path(sys.executable), args=(str(FAKE),), options={"OwnBook": "true"})
    try:
        assert await plain.best_move([], 50) == "a2a3"  # first legal move
        assert await with_book.best_move([], 50) == "h2h4"  # the fake plays the last move once OwnBook arrives
    finally:
        await plain.close()
        await with_book.close()


async def test_analysis_does_not_use_the_book():
    engine = UCIEngine(Path(sys.executable), args=(str(FAKE),), options={"OwnBook": "true"})
    try:
        assert await engine.best_move([], 50) == "h2h4"  # book on for game moves
        assert (await engine.analyze([], 50)).bestmove == "a2a3"  # off for analysis
        assert await engine.best_move([], 50) == "h2h4"  # and back on afterwards
    finally:
        await engine.close()


async def test_analysis_parses_info():
    engine = script_engine()
    try:
        result = await engine.analyze([], 50)
        assert (result.bestmove, result.depth, result.score_cp, result.mate) == ("a2a3", 1, 25, None)
        assert result.pv == ["a2a3"]
    finally:
        await engine.close()


async def test_no_legal_moves_returns_none():
    engine = script_engine()
    try:
        mated = ["f2f3", "e7e5", "g2g4", "d8h4"]
        assert await engine.best_move(mated, 50) is None
    finally:
        await engine.close()


async def test_restarts_after_crash(tmp_path):
    engine = script_engine("--crash-once", str(tmp_path / "marker"))
    try:
        assert await engine.best_move([], 50) == "a2a3"
    finally:
        await engine.close()


async def test_timeout_raises_engine_error():
    engine = script_engine("--hang", search_slack=0.2)
    try:
        with pytest.raises(EngineError):
            await engine.best_move([], 50)
    finally:
        await engine.close()


async def test_missing_binary_raises_engine_error():
    engine = UCIEngine(Path("/nonexistent/engine"))
    with pytest.raises(EngineError):
        await engine.best_move([], 50)


async def test_random_engine_plays_legal_moves():
    import chess

    engine = RandomEngine()
    move = await engine.best_move(["e2e4"], 0)
    board = chess.Board()
    board.push_uci("e2e4")
    assert chess.Move.from_uci(move) in board.legal_moves


def test_build_engine_falls_back_when_binary_missing(tmp_path):
    assert isinstance(build_engine(tmp_path / "nope"), RandomEngine)
    assert isinstance(build_engine(FAKE), UCIEngine)
