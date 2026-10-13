from fastapi import APIRouter, Depends, HTTPException, Request

from . import service
from .config import Settings
from .engine import Engine
from .models import Color, CreatedGame, CreateGameRequest, GameState, MoveRequest
from .rules import IllegalMove, parse_legal_move
from .store import Game, GameStore

router = APIRouter()


def get_store(request: Request) -> GameStore:
    return request.app.state.store


def get_engine(request: Request) -> Engine:
    return request.app.state.engine


def find_game(game_id: str, store: GameStore) -> Game:
    game = store.get(game_id)
    if game is None:
        raise HTTPException(404, "Game not found")
    return game


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


@router.post("/games/{game_id}/moves", response_model=GameState)
async def submit_move(
    game_id: str,
    body: MoveRequest,
    store: GameStore = Depends(get_store),
    engine: Engine = Depends(get_engine),
    settings: Settings = Depends(get_settings),
):
    game = find_game(game_id, store)
    async with game.lock:
        if service.is_over(game):
            raise HTTPException(409, "Game is already over")
        if game.board.turn != game.human.chess:
            raise HTTPException(409, "It is not your turn")

        if body.action == "resign":
            game.resigned_by = game.human
            return service.game_state(game)

        try:
            move = parse_legal_move(game.board, body.move or "")
        except IllegalMove as e:
            raise HTTPException(400, str(e)) from e

        game.board.push(move)
        if not service.is_over(game):
            try:
                await service.engine_reply(game, engine, settings.engine_movetime_ms)
            except Exception:
                game.board.pop()  # leave the game as it was so the client can retry
                raise
        return service.game_state(game)
