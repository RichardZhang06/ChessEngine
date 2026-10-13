START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"


def test_create_as_white(client):
    res = client.post("/games", json={"color": "white"})
    assert res.status_code == 201
    body = res.json()
    assert body["game_id"]
    assert body["moves"] == []
    assert body["last_move"] is None
    assert body["fen"] == START_FEN
    assert body["turn"] == "white"
    assert body["status"] == "playing"
    assert body["result"] is None


def test_create_as_black_engine_opens(client, engine):
    res = client.post("/games", json={"color": "black"})
    assert res.status_code == 201
    body = res.json()
    assert body["moves"] == ["a2a3"]
    assert body["last_move"] == "a2a3"
    assert body["turn"] == "black"
    assert body["fen"].startswith("rnbqkbnr/pppppppp/8/8/8/P7/1PPPPPPP/RNBQKBNR b")
    assert engine.calls == [[]]


def test_invalid_color_is_400(client):
    assert client.post("/games", json={"color": "green"}).status_code == 400
    assert client.post("/games", json={}).status_code == 400
    assert client.post("/games", content="not json").status_code == 400


def test_engine_failure_is_500_and_game_not_kept(client, engine):
    engine.fail = True
    res = client.post("/games", json={"color": "black"})
    assert res.status_code == 500
    assert "detail" in res.json()
    assert len(client.app.state.store._games) == 0


def test_cors_header_for_frontend_origin(client):
    res = client.post("/games", json={"color": "white"}, headers={"Origin": "http://localhost:5173"})
    assert res.headers["access-control-allow-origin"] == "http://localhost:5173"
