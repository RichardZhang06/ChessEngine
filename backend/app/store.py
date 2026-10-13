"""In-memory game store. Kept behind a small interface so a database can replace it later."""

import asyncio
import secrets
from dataclasses import dataclass, field

import chess

from .models import Color


@dataclass
class Game:
    id: str
    human: Color
    board: chess.Board = field(default_factory=chess.Board)
    resigned_by: Color | None = None
    # Serializes requests for one game (a move request awaits the engine).
    lock: asyncio.Lock = field(default_factory=asyncio.Lock)

    @property
    def engine_color(self) -> Color:
        return Color.BLACK if self.human is Color.WHITE else Color.WHITE


class GameStore:
    def __init__(self) -> None:
        self._games: dict[str, Game] = {}

    def create(self, human: Color) -> Game:
        game = Game(id=secrets.token_urlsafe(6), human=human)
        self._games[game.id] = game
        return game

    def get(self, game_id: str) -> Game | None:
        return self._games.get(game_id)

    def delete(self, game_id: str) -> None:
        self._games.pop(game_id, None)
