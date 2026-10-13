def test_get_game_matches_latest_state(client):
    created = client.post("/games", json={"color": "black"}).json()
    gid = created["game_id"]
    moved = client.post(f"/games/{gid}/moves", json={"action": "move", "move": "e7e5"}).json()

    res = client.get(f"/games/{gid}")
    assert res.status_code == 200
    body = res.json()
    assert body["game_id"] == gid
    assert body["color"] == "black"
    for key in ("moves", "last_move", "fen", "turn", "status", "result"):
        assert body[key] == moved[key]


def test_get_resigned_game(client):
    gid = client.post("/games", json={"color": "white"}).json()["game_id"]
    client.post(f"/games/{gid}/moves", json={"action": "resign"})
    body = client.get(f"/games/{gid}").json()
    assert (body["status"], body["result"]) == ("resigned", "0-1")


def test_get_unknown_game_is_404(client):
    assert client.get("/games/nope").status_code == 404
