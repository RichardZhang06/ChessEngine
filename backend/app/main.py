from contextlib import asynccontextmanager

from fastapi import FastAPI, Request
from fastapi.exceptions import RequestValidationError
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

from .config import Settings
from .engine import Engine, EngineError, build_engine
from .routes import router
from .store import GameStore


def create_app(engine: Engine | None = None, settings: Settings | None = None) -> FastAPI:
    """Build the app. Tests pass in a fake engine; production builds one from settings."""
    settings = settings or Settings()

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        app.state.engine = engine or build_engine(settings.engine_path, settings.engine_options)
        yield
        await app.state.engine.close()

    app = FastAPI(title="Chess Engine API", lifespan=lifespan)
    app.state.store = GameStore()
    app.state.settings = settings
    app.add_middleware(
        CORSMiddleware, allow_origins=settings.cors_origins, allow_methods=["*"], allow_headers=["*"]
    )
    app.include_router(router)

    # The README specifies 400 (not FastAPI's default 422) for malformed requests.
    @app.exception_handler(RequestValidationError)
    async def malformed_request(request: Request, exc: RequestValidationError):
        err = exc.errors()[0]
        where = ".".join(str(p) for p in err["loc"] if p != "body")
        return JSONResponse(status_code=400, content={"detail": f"Malformed request: {where}: {err['msg']}"})

    @app.exception_handler(EngineError)
    async def engine_failure(request: Request, exc: EngineError):
        return JSONResponse(status_code=500, content={"detail": f"Engine failure: {exc}"})

    return app


app = create_app()
