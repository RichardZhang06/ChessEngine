# Backend

FastAPI service implementing the REST API described in the top-level README. It owns the game state and rules
(via [python-chess](https://python-chess.readthedocs.io)) and talks UCI to the engine over a subprocess.

## Run

```
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt
.venv/bin/uvicorn main:app --reload      # http://localhost:8000, docs at /docs
.venv/bin/pytest                          # tests
```

The frontend dev server proxies `/api/*` to port 8000.

## Configuration (environment variables)

| Variable | Default | Meaning |
| -------- | ------- | ------- |
| `ENGINE_PATH` | `../engine/build/engine` | UCI engine executable. If the file doesn't exist the backend logs a warning and plays random legal moves instead. |
| `ENGINE_MOVETIME_MS` | `1000` | Think time per engine move. |
| `CORS_ORIGINS` | `http://localhost:5173` | Comma-separated allowed origins. |

## Layout

- `app/models.py`: request/response models
- `app/store.py`: in-memory game store (games are lost on restart)
- `app/rules.py`: move validation, checkmate/stalemate/draw detection
- `app/engine.py`: `UCIEngine` subprocess wrapper (one shared process, restarts once on crash/timeout) and the random fallback
- `app/service.py`, `app/routes.py`: game flow and endpoints

## Endpoints

`POST /games`, `POST /games/{id}/moves`, `GET /games/{id}`, plus the optional `GET /games/{id}/moves`,
`POST /games/{id}/undo` and `POST /games/{id}/analysis`.
`GET /games/{id}` additionally returns `color` (the human's side) so a client can resume a game.
