import asyncio
from contextlib import asynccontextmanager
from typing import Annotated, Optional

from fastapi import FastAPI, Depends, HTTPException, Query

from app.auth import validate_api_key
from app.db import get_pool, close_pool
from app.import_logic import (
    import_game,
    import_games_batch,
    get_all_roles,
    import_roles,
    check_db_connection,
    get_all_players
)
from app.models import (
    GameImportRequest,
    GamesImportRequest,
    GameImportStatusResponse,
    GamesImportStatusResponse,
    RolesResponse,
    RolesImportRequest,
    RolesImportResponse,
    HealthResponse,
    Status,
    PlayersResponse
)


# ============================================================
#  Application lifecycle
# ============================================================

async def wait_for_db(max_retries: int = 30, delay: float = 2.0):
    """Wait for DB to be ready at startup, retrying up to max_retries times."""
    for attempt in range(1, max_retries + 1):
        try:
            pool = await get_pool()
            async with pool.acquire() as conn:
                await conn.fetchval("SELECT 1")
            print(f"Database connected on attempt {attempt}")
            return True
        except Exception as e:
            print(f"DB attempt {attempt}/{max_retries} failed: {e}")
            if attempt < max_retries:
                await asyncio.sleep(delay)
    return False


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup: wait for DB to be ready
    db_ready = await wait_for_db()
    if not db_ready:
        print("WARNING: Could not connect to database, API will be degraded")
    yield
    # Shutdown: close connection pool
    await close_pool()


app = FastAPI(
    title="BotC Import API",
    description="API for importing Blood on the Clocktower game records",
    version="1.0.0",
    lifespan=lifespan,
)


# ============================================================
#  Health check
# ============================================================

@app.get("/health", response_model=HealthResponse)
async def health():
    """Check service health and database connectivity."""
    db_ok = await check_db_connection()
    return HealthResponse(status=Status.ok if db_ok else Status.degraded, db_connected=db_ok)


# ============================================================
#  API endpoints
# ============================================================

@app.post("/api/games/import", response_model=GameImportStatusResponse)
async def create_import(
        data: GameImportRequest,
        owner: dict = Depends(validate_api_key),
):
    """
    Import a single game into the database.

    Requires a valid API key in the X-API-Key header.
    """
    result: GameImportStatusResponse = await import_game(data, owner)
    if result.status == "error":
        raise HTTPException(status_code=400, detail=result.model_dump())
    return result


@app.post("/api/games/import_batch", response_model=GamesImportStatusResponse)
async def create_import_batch(
        data: GamesImportRequest,
        owner: dict = Depends(validate_api_key),
):
    """
    Import multiple games in a single request.

    Each game is processed independently — a failure in one does not roll back the others.
    Requires a valid API key in the X-API-Key header.
    """
    return await import_games_batch(data.games, owner)


@app.get("/api/roles", response_model=RolesResponse)
async def list_roles(
        lang: Annotated[Optional[str], Query(min_length=2, max_length=5)] = None,
        owner: dict = Depends(validate_api_key)):
    """
    Return the list of available roles.

    Requires a valid API key in the X-API-Key header.
    """
    roles = await get_all_roles(lang)
    return RolesResponse(roles=roles)


@app.post("/api/roles/import", response_model=RolesImportResponse)
async def create_roles_import(
        data: RolesImportRequest,
        owner: dict = Depends(validate_api_key),
):
    """
    Upsert a list of roles into the database.

    Existing roles are updated; new ones are created.
    Requires a valid API key in the X-API-Key header.
    """
    result: RolesImportResponse = await import_roles(data)
    if result["status"] == "error":
        raise HTTPException(status_code=400, detail=result)
    return result


@app.get("/api/players", response_model=PlayersResponse)
async def list_players(owner: dict = Depends(validate_api_key)):
    players = await get_all_players()
    return PlayersResponse(players=players)
