import chess
import pytest

from app.models import Color, Status
from app.rules import IllegalMove, evaluate, parse_legal_move


def board_after(*moves: str) -> chess.Board:
    board = chess.Board()
    for m in moves:
        board.push_uci(m)
    return board


def test_legal_move_parses():
    assert parse_legal_move(chess.Board(), "e2e4") == chess.Move.from_uci("e2e4")


@pytest.mark.parametrize("bad", ["e2e5", "e7e5", "garbage", "", "e2", "e1g1", "e7e8q"])
def test_illegal_or_malformed_moves_rejected(bad):
    with pytest.raises(IllegalMove):
        parse_legal_move(chess.Board(), bad)


def test_promotion_requires_piece_letter():
    board = chess.Board("8/P7/8/8/8/8/k6K/8 w - - 0 1")
    with pytest.raises(IllegalMove):
        parse_legal_move(board, "a7a8")
    assert parse_legal_move(board, "a7a8q")


def test_castling_uci():
    board = chess.Board("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1")
    assert parse_legal_move(board, "e1g1")
    assert parse_legal_move(board, "e1c1")


def test_playing():
    assert evaluate(chess.Board()) == (Status.PLAYING, None)


def test_fools_mate_black_wins():
    board = board_after("f2f3", "e7e5", "g2g4", "d8h4")
    assert evaluate(board) == (Status.CHECKMATE, "0-1")


def test_scholars_mate_white_wins():
    board = board_after("e2e4", "e7e5", "f1c4", "b8c6", "d1h5", "g8f6", "h5f7")
    assert evaluate(board) == (Status.CHECKMATE, "1-0")


def test_stalemate():
    board = chess.Board("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1")
    assert evaluate(board) == (Status.STALEMATE, "1/2-1/2")


def test_insufficient_material():
    assert evaluate(chess.Board("8/8/4k3/8/8/4K3/8/8 w - - 0 1")) == (Status.DRAW, "1/2-1/2")
    assert evaluate(chess.Board("8/8/4k3/8/8/4KB2/8/8 w - - 0 1")) == (Status.DRAW, "1/2-1/2")


def test_fifty_move_rule():
    board = chess.Board("4k3/8/8/8/8/8/8/R3K3 w - - 100 80")
    assert evaluate(board) == (Status.DRAW, "1/2-1/2")
    board = chess.Board("4k3/8/8/8/8/8/8/R3K3 w - - 99 80")
    assert evaluate(board) == (Status.PLAYING, None)


def test_threefold_repetition():
    shuffle = ["g1f3", "g8f6", "f3g1", "f6g8"]
    board = board_after(*shuffle, *shuffle)
    assert evaluate(board) == (Status.DRAW, "1/2-1/2")
    assert evaluate(board_after(*shuffle)) == (Status.PLAYING, None)


def test_checkmate_beats_fifty_move_rule():
    board = chess.Board("7k/5Q2/6K1/8/8/8/8/8 w - - 99 80")
    board.push_uci("f7g7")
    assert board.halfmove_clock == 100
    assert evaluate(board) == (Status.CHECKMATE, "1-0")


def test_resignation_gives_opponent_the_win():
    assert evaluate(chess.Board(), Color.WHITE) == (Status.RESIGNED, "0-1")
    assert evaluate(chess.Board(), Color.BLACK) == (Status.RESIGNED, "1-0")
