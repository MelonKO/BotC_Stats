"""
Tests for db.py — connection pool management.
"""
from unittest.mock import AsyncMock, patch
from contextlib import asynccontextmanager

import pytest

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.db import get_pool, close_pool, get_connection


class TestDbPool:
    """Tests for pool management functions."""

    @pytest.mark.asyncio
    async def test_get_pool_creates_pool(self):
        """get_pool creates asyncpg pool on first call."""
        mock_pool = AsyncMock()

        with patch("app.db.asyncpg.create_pool", new_callable=AsyncMock, return_value=mock_pool) as create_mock:
            import app.db
            app.db._pool = None

            result = await get_pool()

            create_mock.assert_called_once()
            assert result == mock_pool
            assert app.db._pool == mock_pool

    @pytest.mark.asyncio
    async def test_get_pool_returnss_existing_pool(self):
        """get_pool returns existing pool without creating new one."""
        mock_pool = AsyncMock()

        import app.db
        app.db._pool = mock_pool

        with patch("app.db.asyncpg.create_pool", new_callable=AsyncMock) as create_mock:
            result = await get_pool()

            create_mock.assert_not_called()
            assert result == mock_pool

    @pytest.mark.asyncio
    async def test_close_pool_sets_none(self):
        """close_pool closes pool and sets to None."""
        mock_pool = AsyncMock()

        import app.db
        app.db._pool = mock_pool

        await close_pool()

        mock_pool.close.assert_called_once()
        assert app.db._pool is None

    @pytest.mark.asyncio
    async def test_close_pool_noop_when_none(self):
        """close_pool does nothing when pool is already None."""
        import app.db
        app.db._pool = None

        await close_pool()  # Should not raise

    @pytest.mark.asyncio
    async def test_get_connection_context_manager(self):
        """get_connection yields a connection from the pool."""
        mock_pool = AsyncMock()
        mock_conn = AsyncMock()

        @asynccontextmanager
        async def mock_acquire():
            yield mock_conn

        mock_pool.acquire = mock_acquire

        import app.db
        app.db._pool = mock_pool

        async with get_connection() as conn:
            assert conn == mock_conn

    @pytest.mark.asyncio
    async def test_get_pool_passes_correct_parameters(self):
        """create_pool is called with correct parameters."""
        mock_pool = AsyncMock()

        import app.db
        app.db._pool = None

        with patch("app.db.asyncpg.create_pool", new_callable=AsyncMock, return_value=mock_pool) as create_mock:
            await get_pool()

        create_mock.assert_called_once()
        call_kwargs = create_mock.call_args[1]
        assert call_kwargs["min_size"] == 2
        assert call_kwargs["max_size"] == 10
        assert call_kwargs["command_timeout"] == 30
