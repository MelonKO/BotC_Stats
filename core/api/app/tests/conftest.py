"""
Shared fixtures for core/api tests.
"""
import asyncio
from datetime import datetime
from unittest.mock import AsyncMock, MagicMock, patch
from contextlib import asynccontextmanager

import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient

import sys
from pathlib import Path

# Add the app directory to the path so we can import modules
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))


# ============================================================
# Helper: proper async context manager mock for pool.acquire()
# ============================================================

def make_mock_acquire(conn):
    """
    Create a proper async context manager factory for pool.acquire().
    asyncpg.Pool.acquire() returns an async context manager, not a coroutine.
    """
    @asynccontextmanager
    async def mock_acquire():
        yield conn
    return mock_acquire


# ============================================================
# Fixtures: mock database
# ============================================================

@pytest.fixture
def mock_api_key_row():
    """Standard mock api_keys row for a valid key."""
    return {
        "id": 1,
        "owner_name": "Test User",
        "active": True,
        "created_at": datetime(2026, 1, 1),
        "last_used_at": None,
    }


@pytest.fixture
def mock_revoked_api_key_row():
    """Mock api_keys row for a revoked key."""
    return {
        "id": 2,
        "owner_name": "Revoked User",
        "active": False,
        "created_at": datetime(2025, 1, 1),
        "last_used_at": None,
    }


@pytest.fixture
def mock_conn():
    """Mock asyncpg connection."""
    conn = AsyncMock()
    conn.fetchrow = AsyncMock(return_value=None)
    conn.fetch = AsyncMock(return_value=[])
    conn.execute = AsyncMock(return_value="INSERT 0 1")
    conn.fetchval = AsyncMock(return_value=None)
    return conn


@pytest.fixture
def mock_pool(mock_conn):
    """Mock asyncpg pool with proper async context manager for acquire()."""
    pool = AsyncMock()
    pool.acquire = make_mock_acquire(mock_conn)
    return pool, mock_conn


@pytest_asyncio.fixture
async def mock_db_pool(mock_pool):
    """Patch get_pool and get_connection to use mock pool."""
    pool, conn = mock_pool

    # Patch get_pool to return our mock
    with patch("app.db.get_pool", new_callable=AsyncMock, return_value=pool):
        # Patch get_connection — it uses get_pool() internally, but we also
        # need to patch it directly for modules that import it
        with patch("app.db.get_connection") as mock_get_conn:
            mock_get_conn.return_value = make_mock_acquire(conn)
            yield {"pool": pool, "conn": conn}


@pytest_asyncio.fixture
async def client(mock_db_pool):
    """HTTPX AsyncClient connected to the test app with mocked DB."""
    from app.main import app

    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac


@pytest.fixture
def valid_api_key():
    """A plain-text API key for testing."""
    return "sk-test-key-123"


@pytest.fixture
def valid_game_payload():
    """Standard valid game import payload."""
    return {
        "game_date": "2026-01-15",
        "scenario_name": "Вселенная зла",
        "storyteller_name": "МелонКО",
        "alignment_win": "добро",
        "location": "Москва, Антикафе на Арбате",
        "game_number": 1,
        "duration": "01:30:00",
        "notes": "Отличная партия",
        "players": [
            {
                "name": "Анна Никитина",
                "seat_number": 1,
                "role_start": "Дамочка",
                "role_end": "Дамочка",
                "alignment_end": "добро",
                "is_alive": True,
            },
            {
                "name": "Борис Петров",
                "seat_number": 2,
                "role_start": "Убийца",
                "role_end": "Убийца",
                "alignment_end": "зло",
                "is_alive": False,
            },
        ],
    }


@pytest.fixture
def valid_roles_import_payload():
    """Standard valid roles import payload."""
    return {
        "roles": [
            {
                "name": "Chambermaid",
                "alignment": "good",
                "role_type": "Outsider",
                "description": "Simple, but not harmless",
                "translations": {
                    "ru": {
                        "name": "Горничная",
                        "description": "Просто, но не безобидно",
                    }
                },
            },
            {
                "name": "Imp",
                "alignment": "evil",
                "role_type": "Demon",
                "translations": {
                    "ru": {"name": "Имп", "description": "Каждую ночь* выбирайте"}
                },
            },
        ]
    }
