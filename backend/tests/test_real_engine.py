"""End-to-end check against the compiled C++ engine. Skipped if it hasn't been built (see engine/README.md)."""

import chess
import pytest
from fastapi.testclient import TestClient

from app.config import Settings
from app.engine import UCIEngine
from app.main import create_app

ENGINE = Settings().engine_path
pytestmark = pytest.mark.skipif(not ENGINE.is_file(), reason=f"engine binary not built at {ENGINE}")


@pytest.fixture
def client():
    settings = Settings(engine_movetime_ms=100)
    with TestClient(create_app(engine=UCIEngine(ENGINE, options=settings.engine_options), settings=settings)) as c:
        yield c


def test_engine_opens_as_white_when_human_is_black(client):
    body = client.post("/games", json={"color": "black"}).json()
    assert len(body["moves"]) == 1
    board = chess.Board()
    assert chess.Move.from_uci(body["moves"][0]) in board.legal_moves


def test_engine_opens_from_its_book(client):
    # OwnBook is on by default, so the engine's first move comes from the main opening lines
    openings = {client.post("/games", json={"color": "black"}).json()["moves"][0] for _ in range(12)}
    assert openings <= {"e2e4", "d2d4", "c2c4", "g1f3"}


def test_engine_replies_with_legal_moves(client):
    gid = client.post("/games", json={"color": "white"}).json()["game_id"]
    body = client.post(f"/games/{gid}/moves", json={"action": "move", "move": "e2e4"}).json()
    assert len(body["moves"]) == 2
    board = chess.Board()
    for m in body["moves"]:
        assert chess.Move.from_uci(m) in board.legal_moves
        board.push_uci(m)


def test_analysis_reports_a_score_and_pv(client):
    gid = client.post("/games", json={"color": "white"}).json()["game_id"]
    body = client.post(f"/games/{gid}/analysis").json()
    assert body["bestmove"]
    assert body["depth"] >= 1
    assert body["score_cp"] is not None
    assert body["pv"][0] == body["bestmove"]


def test_engine_mates_in_one(client):
    gid = client.post("/games", json={"color": "white"}).json()["game_id"]
    game = client.app.state.store.get(gid)
    # Fool's-mate setup: after 1.f3 e5 2.g4 the human (white) hands black a mate in one, engine plays Qh4#
    game.board.push_uci("f2f3")
    game.board.push_uci("e7e5")
    body = client.post(f"/games/{gid}/moves", json={"action": "move", "move": "g2g4"}).json()
    assert body["last_move"] == "d8h4"
    assert body["status"] == "checkmate"
    assert body["result"] == "0-1"
