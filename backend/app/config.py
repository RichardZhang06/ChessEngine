import os
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class Settings:
    # Path to a UCI engine executable. If it doesn't exist the backend falls back to a random-move engine.
    engine_path: Path = Path(os.environ.get("ENGINE_PATH", REPO_ROOT / "engine" / "build" / "engine"))
    # Time the engine gets to think per move.
    engine_movetime_ms: int = int(os.environ.get("ENGINE_MOVETIME_MS", "1000"))
    # Let the engine play from its opening book (UCI option OwnBook) while the game is still in known theory. On by default.
    engine_own_book: bool = os.environ.get("ENGINE_OWNBOOK", "true").lower() != "false"
    # Origins allowed by CORS (the Vite dev server also proxies /api, so this is a convenience).
    cors_origins: list[str] = field(
        default_factory=lambda: os.environ.get("CORS_ORIGINS", "http://localhost:5173").split(",")
    )

    @property
    def engine_options(self) -> dict[str, str]:
        """UCI options sent to the engine at startup. Engines that don't know an option ignore it."""
        # Always sent, because the engine's own default is on
        return {"OwnBook": "true" if self.engine_own_book else "false"}
