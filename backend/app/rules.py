"""Game rules: move validation and game-end detection (checkmate, stalemate, draws, resignation)."""

import chess

from .models import Color, Status


class IllegalMove(ValueError):
    pass


def parse_legal_move(board: chess.Board, uci: str) -> chess.Move:
    """Parse a UCI move (e2e4, e7e8q) and make sure it is legal in this position."""
    try:
        move = chess.Move.from_uci(uci.strip())
    except ValueError as e:
        raise IllegalMove(f"Malformed move: {uci!r}") from e
    if move not in board.legal_moves:
        raise IllegalMove(f"Illegal move: {uci}")
    return move


def evaluate(board: chess.Board, resigned_by: Color | None = None) -> tuple[Status, str | None]:
    """Return (status, result) for a position. Result is non-null whenever the game is over."""
    if resigned_by is not None:
        return Status.RESIGNED, "0-1" if resigned_by is Color.WHITE else "1-0"
    # Checkmate must be tested before the draw rules: a mating move on the 100th half-move still mates.
    if board.is_checkmate():
        return Status.CHECKMATE, "0-1" if board.turn == chess.WHITE else "1-0"
    if board.is_stalemate():
        return Status.STALEMATE, "1/2-1/2"
    if board.is_insufficient_material() or board.is_fifty_moves() or board.is_repetition(3):
        return Status.DRAW, "1/2-1/2"
    return Status.PLAYING, None
