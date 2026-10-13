"""Talking to a chess engine.

`UCIEngine` drives any UCI engine executable over stdin/stdout (see UCI_protocol.txt).
`RandomEngine` is a stand-in used when no engine binary exists yet, so the rest of the stack stays usable.

One engine process is shared by all games and requests are serialized with a lock. Every request sends the
full move list from the start position, so the engine holds no per-game state.
"""

import asyncio
import logging
import random
from dataclasses import dataclass, field
from pathlib import Path
from typing import Protocol

import chess

log = logging.getLogger(__name__)


class EngineError(RuntimeError):
    pass


@dataclass
class Analysis:
    bestmove: str | None = None
    depth: int | None = None
    score_cp: int | None = None
    mate: int | None = None
    pv: list[str] = field(default_factory=list)


class Engine(Protocol):
    async def best_move(self, moves: list[str], movetime_ms: int) -> str | None: ...
    async def analyze(self, moves: list[str], movetime_ms: int) -> Analysis: ...
    async def close(self) -> None: ...


def _parse_info(line: str, analysis: Analysis) -> None:
    """Fold an `info ...` line into the running analysis (only lines that carry a pv)."""
    tokens = line.split()
    if "pv" not in tokens:
        return
    try:
        if "depth" in tokens:
            analysis.depth = int(tokens[tokens.index("depth") + 1])
        if "score" in tokens:
            i = tokens.index("score")
            kind, value = tokens[i + 1], int(tokens[i + 2])
            analysis.score_cp, analysis.mate = (value, None) if kind == "cp" else (None, value)
        analysis.pv = tokens[tokens.index("pv") + 1 :]
    except (ValueError, IndexError):
        pass  # ignore malformed info lines, per the UCI spec's tolerance rules


class UCIEngine:
    def __init__(
        self,
        path: Path,
        args: tuple[str, ...] = (),
        options: dict[str, str] | None = None,
        startup_timeout: float = 10.0,
        search_slack: float = 5.0,
    ):
        self._path = path
        self._args = args
        self._options = options or {}  # UCI options sent with `setoption` after the handshake
        self._startup_timeout = startup_timeout
        self._search_slack = search_slack
        self._proc: asyncio.subprocess.Process | None = None
        self._lock = asyncio.Lock()

    async def best_move(self, moves: list[str], movetime_ms: int) -> str | None:
        return (await self._run(moves, movetime_ms, use_book=True)).bestmove

    async def analyze(self, moves: list[str], movetime_ms: int) -> Analysis:
        # Analysis must really search (score, depth, pv), so it never uses the opening book
        return await self._run(moves, movetime_ms, use_book=False)

    async def close(self) -> None:
        async with self._lock:
            await self._kill(graceful=True)

    async def _run(self, moves: list[str], movetime_ms: int, use_book: bool) -> Analysis:
        async with self._lock:
            last_error: Exception | None = None
            for _ in range(2):  # one automatic restart if the engine died or hung
                try:
                    if self._proc is None or self._proc.returncode is not None:
                        await self._start()
                    return await self._search(moves, movetime_ms, use_book)
                except (EngineError, OSError, asyncio.TimeoutError) as e:
                    log.warning("engine failure, restarting: %r", e)
                    last_error = e
                    await self._kill()
            raise EngineError(f"engine failed: {last_error!r}")

    async def _start(self) -> None:
        self._proc = await asyncio.create_subprocess_exec(
            str(self._path),
            *self._args,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.DEVNULL,
        )
        async with asyncio.timeout(self._startup_timeout):
            await self._send("uci")
            await self._read_until("uciok")
            for name, value in self._options.items():
                await self._send(f"setoption name {name} value {value}")
            await self._send("isready")
            await self._read_until("readyok")

    async def _search(self, moves: list[str], movetime_ms: int, use_book: bool) -> Analysis:
        if self._options.get("OwnBook") == "true":
            await self._send(f"setoption name OwnBook value {'true' if use_book else 'false'}")
        position = "position startpos" + (" moves " + " ".join(moves) if moves else "")
        analysis = Analysis()
        async with asyncio.timeout(movetime_ms / 1000 + self._search_slack):
            await self._send(position)
            await self._send(f"go movetime {movetime_ms}")
            line = await self._read_until("bestmove", on_line=lambda l: _parse_info(l, analysis))
        tokens = line.split()
        best = tokens[1] if len(tokens) > 1 else None
        analysis.bestmove = None if best in (None, "0000", "(none)") else best
        return analysis

    async def _send(self, line: str) -> None:
        assert self._proc and self._proc.stdin
        self._proc.stdin.write((line + "\n").encode())
        await self._proc.stdin.drain()

    async def _read_until(self, prefix: str, on_line=None) -> str:
        assert self._proc and self._proc.stdout
        while True:
            raw = await self._proc.stdout.readline()
            if not raw:
                raise EngineError("engine closed its output")
            line = raw.decode(errors="replace").strip()
            if on_line:
                on_line(line)
            if line.split(" ", 1)[0] == prefix:
                return line

    async def _kill(self, graceful: bool = False) -> None:
        proc, self._proc = self._proc, None
        if proc is None or proc.returncode is not None:
            return
        try:
            if graceful and proc.stdin:
                proc.stdin.write(b"quit\n")
                await proc.stdin.drain()
                await asyncio.wait_for(proc.wait(), timeout=2)
                return
        except (OSError, asyncio.TimeoutError):
            pass
        try:
            proc.kill()
        except ProcessLookupError:
            pass
        await proc.wait()


class RandomEngine:
    """Plays a random legal move. Placeholder until the real engine exists."""

    async def best_move(self, moves: list[str], movetime_ms: int) -> str | None:
        board = chess.Board()
        for m in moves:
            board.push_uci(m)
        legal = list(board.legal_moves)
        return random.choice(legal).uci() if legal else None

    async def analyze(self, moves: list[str], movetime_ms: int) -> Analysis:
        best = await self.best_move(moves, movetime_ms)
        return Analysis(bestmove=best, depth=0, score_cp=0, pv=[best] if best else [])

    async def close(self) -> None:
        pass


def build_engine(path: Path, options: dict[str, str] | None = None) -> Engine:
    if path.is_file():
        return UCIEngine(path, options=options)
    log.warning("No engine binary at %s, falling back to a random-move engine", path)
    return RandomEngine()
