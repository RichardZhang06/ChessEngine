def new_game(client, color="white"):
    return client.post("/games", json={"color": color}).json()["game_id"]


def play(client, gid, uci):
    return client.post(f"/games/{gid}/moves", json={"action": "move", "move": uci}).json()


def test_move_history(client):
    gid = new_game(client)
    assert client.get(f"/games/{gid}/moves").json() == {"moves": []}
    play(client, gid, "e2e4")
    assert client.get(f"/games/{gid}/moves").json() == {"moves": ["e2e4", "a7a5"]}
    assert client.get("/games/nope/moves").status_code == 404


def test_undo_takes_back_human_and_engine_move(client):
    gid = new_game(client)
    play(client, gid, "e2e4")
    play(client, gid, "d2d4")
    res = client.post(f"/games/{gid}/undo")
    assert res.status_code == 200
    assert res.json()["moves"] == ["e2e4", "a7a5"]
    assert res.json()["turn"] == "white"
    assert client.get(f"/games/{gid}").json()["moves"] == ["e2e4", "a7a5"]


def test_undo_with_nothing_to_undo_is_409(client):
    gid = new_game(client)
    assert client.post(f"/games/{gid}/undo").status_code == 409


def test_undo_as_black_keeps_engine_opening(client):
    gid = new_game(client, "black")  # engine opened a2a3
    assert client.post(f"/games/{gid}/undo").status_code == 409  # human hasn't moved yet
    play(client, gid, "e7e5")
    res = client.post(f"/games/{gid}/undo")
    assert res.status_code == 200
    assert res.json()["moves"] == ["a2a3"]
    assert res.json()["turn"] == "black"


def test_undo_after_checkmate_reopens_game(client):
    gid = new_game(client)
    game = client.app.state.store.get(gid)
    for m in ["f2f3", "e7e5", "g2g4"]:
        game.board.push_uci(m)
    game.human = game.human.__class__.BLACK
    assert play(client, gid, "d8h4")["status"] == "checkmate"
    res = client.post(f"/games/{gid}/undo")
    assert res.status_code == 200
    assert res.json()["status"] == "playing"
    assert res.json()["moves"] == ["f2f3", "e7e5", "g2g4"]


def test_cannot_undo_resigned_game(client):
    gid = new_game(client)
    client.post(f"/games/{gid}/moves", json={"action": "resign"})
    assert client.post(f"/games/{gid}/undo").status_code == 409


def test_undo_unknown_game_is_404(client):
    assert client.post("/games/nope/undo").status_code == 404


def test_analysis(client):
    gid = new_game(client)
    res = client.post(f"/games/{gid}/analysis")
    assert res.status_code == 200
    assert res.json() == {"bestmove": "a2a3", "depth": 1, "score_cp": 25, "mate": None, "pv": ["a2a3"]}


def test_analysis_with_movetime_and_no_state_change(client, engine):
    gid = new_game(client)
    res = client.post(f"/games/{gid}/analysis", json={"movetime_ms": 200})
    assert res.status_code == 200
    assert client.get(f"/games/{gid}").json()["moves"] == []


def test_analysis_unknown_game_is_404(client):
    assert client.post("/games/nope/analysis").status_code == 404
