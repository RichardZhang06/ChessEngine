"""Request/response models matching the REST API documented in the README."""

from enum import Enum
from typing import Literal

import chess
from pydantic import BaseModel, model_validator


class Color(str, Enum):
    WHITE = "white"
    BLACK = "black"

    @property
    def chess_color(self) -> chess.Color:
        return chess.WHITE if self is Color.WHITE else chess.BLACK

    @classmethod
    def from_chess(cls, color: chess.Color) -> "Color":
        return cls.WHITE if color == chess.WHITE else cls.BLACK


class Status(str, Enum):
    PLAYING = "playing"
    CHECKMATE = "checkmate"
    STALEMATE = "stalemate"
    DRAW = "draw"
    RESIGNED = "resigned"


class CreateGameRequest(BaseModel):
    color: Color


class MoveRequest(BaseModel):
    action: Literal["move", "resign"]
    move: str | None = None

    @model_validator(mode="after")
    def move_required_for_move_action(self) -> "MoveRequest":
        if self.action == "move" and not self.move:
            raise ValueError("'move' is required when action is 'move'")
        return self


class GameState(BaseModel):
    moves: list[str]
    last_move: str | None
    fen: str
    turn: Color
    status: Status
    result: str | None


class CreatedGame(GameState):
    game_id: str


class GameDetail(CreatedGame):
    """GET /games/{id}: same as the create response plus the human's color, so a client can resume."""

    color: Color


class MoveHistory(BaseModel):
    moves: list[str]


class AnalysisRequest(BaseModel):
    movetime_ms: int | None = None


class AnalysisResult(BaseModel):
    bestmove: str | None
    depth: int | None
    score_cp: int | None
    mate: int | None
    pv: list[str]
