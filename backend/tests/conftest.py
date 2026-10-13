import chess
import pytest
from fastapi.testclient import TestClient

from app.engine import Analysis
from app.main import create_app


class FakeEngine:
    """Deterministic engine: plays the first legal move in UCI order. Can be told to fail."""

    def __init__(self):
        self.fail = False
        self.calls: list[list[str]] = []

    def _legal(self, moves):
        board = chess.Board()
        for m in moves:
            board.push_uci(m)
        return sorted(m.uci() for m in board.legal_moves)

    async def best_move(self, moves, movetime_ms):
        self.calls.append(list(moves))
        if self.fail:
            from app.engine import EngineError

            raise EngineError("boom")
        legal = self._legal(moves)
        return legal[0] if legal else None

    async def analyze(self, moves, movetime_ms):
        best = await self.best_move(moves, movetime_ms)
        return Analysis(bestmove=best, depth=1, score_cp=25, pv=[best] if best else [])

    async def close(self):
        pass


@pytest.fixture
def engine():
    return FakeEngine()


@pytest.fixture
def client(engine):
    with TestClient(create_app(engine=engine)) as c:
        yield c
