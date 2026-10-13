import pytest


def new_game(client, color="white"):
    return client.post("/games", json={"color": color}).json()["game_id"]


def move(client, gid, uci):
    return client.post(f"/games/{gid}/moves", json={"action": "move", "move": uci})


def test_move_gets_engine_reply(client):
    gid = new_game(client)
    res = move(client, gid, "e2e4")
    assert res.status_code == 200
    body = res.json()
    # fake engine plays the first legal move in UCI order: a7a5
    assert body["moves"] == ["e2e4", "a7a5"]
    assert body["last_move"] == "a7a5"
    assert body["turn"] == "white"
    assert body["status"] == "playing"
    assert body["result"] is None
    assert body["fen"].startswith("rnbqkbnr/1ppppppp/8/p7/4P3/8/PPPP1PPP/RNBQKBNR w")


def test_history_accumulates(client):
    gid = new_game(client)
    move(client, gid, "e2e4")
    body = move(client, gid, "d2d4").json()
    assert body["moves"] == ["e2e4", "a7a5", "d2d4", "a5a4"]


def test_play_as_black(client):
    gid = new_game(client, "black")  # engine opened with a2a3
    body = move(client, gid, "e7e5").json()
    assert body["moves"] == ["a2a3", "e7e5", "a1a2"]
    assert body["turn"] == "black"


@pytest.mark.parametrize("bad", ["e2e5", "e7e5", "zzzz", "e1g1"])
def test_illegal_move_is_400_and_state_unchanged(client, bad):
    gid = new_game(client)
    assert move(client, gid, bad).status_code == 400
    assert move(client, gid, "e2e4").json()["moves"] == ["e2e4", "a7a5"]


def test_malformed_request_is_400(client):
    gid = new_game(client)
    assert client.post(f"/games/{gid}/moves", json={"action": "move"}).status_code == 400
    assert client.post(f"/games/{gid}/moves", json={"action": "dance"}).status_code == 400
    assert client.post(f"/games/{gid}/moves", json={}).status_code == 400


def test_unknown_game_is_404(client):
    assert move(client, "nope", "e2e4").status_code == 404


def test_resign(client):
    gid = new_game(client)
    res = client.post(f"/games/{gid}/moves", json={"action": "resign"})
    assert res.status_code == 200
    assert res.json()["status"] == "resigned"
    assert res.json()["result"] == "0-1"  # white resigned, black wins


def test_resign_as_black(client):
    gid = new_game(client, "black")
    body = client.post(f"/games/{gid}/moves", json={"action": "resign"}).json()
    assert (body["status"], body["result"]) == ("resigned", "1-0")


def test_moves_after_game_over_are_409(client):
    gid = new_game(client)
    client.post(f"/games/{gid}/moves", json={"action": "resign"})
    assert move(client, gid, "e2e4").status_code == 409
    assert client.post(f"/games/{gid}/moves", json={"action": "resign"}).status_code == 409


def test_checkmate_by_human_ends_game_without_engine_reply(client, engine):
    # Fool's mate with the human as black: engine (white) plays a2a3, then we need white to blunder.
    # Easier: drive the board directly to a position one move from mate.
    gid = new_game(client)
    game = client.app.state.store.get(gid)
    for m in ["f2f3", "e7e5", "g2g4"]:
        game.board.push_uci(m)
    game.human = game.human.__class__.BLACK  # human now plays black, to move
    calls_before = len(engine.calls)
    body = move(client, gid, "d8h4").json()
    assert body["status"] == "checkmate"
    assert body["result"] == "0-1"
    assert body["last_move"] == "d8h4"  # the human's move, since it ended the game
    assert len(engine.calls) == calls_before


def test_engine_failure_rolls_back_human_move(client, engine):
    gid = new_game(client)
    engine.fail = True
    assert move(client, gid, "e2e4").status_code == 500
    engine.fail = False
    assert move(client, gid, "d2d4").json()["moves"] == ["d2d4", "a7a5"]


def test_not_your_turn_is_409(client):
    gid = new_game(client)
    client.app.state.store.get(gid).board.push_uci("e2e4")  # simulate it being black's turn
    assert move(client, gid, "e7e5").status_code == 409
