"""Game flow shared by the route handlers."""

import chess

from .engine import Engine, EngineError
from .models import Color, CreatedGame, GameState
from .rules import evaluate
from .store import Game


def is_over(game: Game) -> bool:
    status, _ = evaluate(game.board, game.resigned_by)
    return status.value != "playing"


def game_state(game: Game) -> GameState:
    status, result = evaluate(game.board, game.resigned_by)
    moves = [m.uci() for m in game.board.move_stack]
    return GameState(
        moves=moves,
        last_move=moves[-1] if moves else None,
        fen=game.board.fen(),
        turn=Color.from_chess(game.board.turn),
        status=status,
        result=result,
    )


def created_game(game: Game) -> CreatedGame:
    return CreatedGame(game_id=game.id, **game_state(game).model_dump())


async def engine_reply(game: Game, engine: Engine, movetime_ms: int) -> None:
    """Ask the engine for a move in the current position and play it."""
    moves = [m.uci() for m in game.board.move_stack]
    uci = await engine.best_move(moves, movetime_ms)
    if uci is None:
        raise EngineError("engine returned no move")
    try:
        move = chess.Move.from_uci(uci)
    except ValueError as e:
        raise EngineError(f"engine returned a malformed move: {uci!r}") from e
    if move not in game.board.legal_moves:
        raise EngineError(f"engine returned an illegal move: {uci}")
    game.board.push(move)
