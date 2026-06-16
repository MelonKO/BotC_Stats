import asyncpg
import os
from contextlib import asynccontextmanager
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    DATABASE_URL: str = "postgresql://api_service:REPLACE_ME_IN_ENV@db:5432/botc_stats"


settings = Settings()

_pool: asyncpg.Pool | None = None


async def get_pool() -> asyncpg.Pool:
    """Return the asyncpg connection pool, creating it on first call (singleton)."""
    global _pool
    if _pool is None:
        _pool = await asyncpg.create_pool(
            dsn=settings.DATABASE_URL,
            min_size=2,
            max_size=10,
            command_timeout=30,
        )
    return _pool


async def close_pool():
    """Close the connection pool and reset the singleton."""
    global _pool
    if _pool is not None:
        await _pool.close()
        _pool = None


@asynccontextmanager
async def get_connection():
    """Async context manager that yields a single connection from the pool."""
    pool = await get_pool()
    async with pool.acquire() as conn:
        yield conn
