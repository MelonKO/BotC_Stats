import asyncpg
import os
from contextlib import asynccontextmanager
from pydantic_settings import BaseSettings


class Settings(BaseSettings):
    DATABASE_URL: str = "postgresql://api_service:REPLACE_ME_IN_ENV@db:5432/botc_stats"


settings = Settings()

_pool: asyncpg.Pool | None = None


async def get_pool() -> asyncpg.Pool:
    """Получить пул подключений к БД (singleton)."""
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
    """Закрыть пул подключений."""
    global _pool
    if _pool is not None:
        await _pool.close()
        _pool = None


@asynccontextmanager
async def get_connection():
    """Контекстный менеджер для получения подключения."""
    pool = await get_pool()
    async with pool.acquire() as conn:
        yield conn
