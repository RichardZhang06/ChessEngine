"""A tiny UCI engine used to test the wrapper: plays the first legal move in UCI order.

--crash-once MARKER   exit without answering the first `go` (if MARKER doesn't exist yet), to test restarts
--hang                never answer `go`, to test timeouts
"""

import sys
from pathlib import Path

import chess

args = sys.argv[1:]
crash_marker = Path(args[args.index("--crash-once") + 1]) if "--crash-once" in args else None
hang = "--hang" in args
moves: list[str] = []
options: dict[str, str] = {}

for line in sys.stdin:
    parts = line.split()
    if not parts:
        continue
    cmd = parts[0]
    if cmd == "uci":
        print("id name FakeEngine\nid author tests\nuciok", flush=True)
    elif cmd == "setoption":
        # setoption name <name> value <value>
        options[parts[2]] = parts[parts.index("value") + 1] if "value" in parts else ""
    elif cmd == "isready":
        print("readyok", flush=True)
    elif cmd == "position":
        moves = parts[parts.index("moves") + 1 :] if "moves" in parts else []
    elif cmd == "go":
        if hang:
            continue
        if crash_marker and not crash_marker.exists():
            crash_marker.write_text("crashed")
            sys.exit(1)
        board = chess.Board()
        for m in moves:
            board.push_uci(m)
        legal = sorted(m.uci() for m in board.legal_moves)
        if legal:
            # with OwnBook on, play the last move instead of the first, so tests can tell the option arrived
            pick = legal[-1] if options.get("OwnBook") == "true" else legal[0]
            print(f"info depth 1 score cp 25 nodes 10 pv {pick}", flush=True)
            print(f"bestmove {pick}", flush=True)
        else:
            print("bestmove 0000", flush=True)
    elif cmd == "quit":
        break
