from fastapi import APIRouter, Depends, Request

from . import service
from .config import Settings
from .engine import Engine
from .models import Color, CreatedGame, CreateGameRequest
from .store import GameStore

router = APIRouter()


def get_store(request: Request) -> GameStore:
    return request.app.state.store


def get_engine(request: Request) -> Engine:
    return request.app.state.engine


def get_settings(request: Request) -> Settings:
    return request.app.state.settings


@router.post("/games", status_code=201, response_model=CreatedGame)
async def create_game(
    body: CreateGameRequest,
    store: GameStore = Depends(get_store),
    engine: Engine = Depends(get_engine),
    settings: Settings = Depends(get_settings),
):
    game = store.create(body.color)
    if body.color is Color.BLACK:
        # The engine plays white's opening move before we respond.
        try:
            await service.engine_reply(game, engine, settings.engine_movetime_ms)
        except Exception:
            store.delete(game.id)
            raise
    return service.created_game(game)
