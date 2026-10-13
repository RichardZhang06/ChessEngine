from app.models import Color
from app.store import GameStore


def test_create_and_get():
    store = GameStore()
    game = store.create(Color.BLACK)
    assert store.get(game.id) is game
    assert game.human is Color.BLACK
    assert game.engine_color is Color.WHITE
    assert game.board.fen().startswith("rnbqkbnr/pppppppp")


def test_unknown_and_delete():
    store = GameStore()
    assert store.get("nope") is None
    game = store.create(Color.WHITE)
    store.delete(game.id)
    assert store.get(game.id) is None


def test_ids_are_unique():
    store = GameStore()
    assert len({store.create(Color.WHITE).id for _ in range(100)}) == 100
