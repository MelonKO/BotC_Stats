"""
Tests for FastAPI endpoints in main.py.
Uses httpx.AsyncClient with ASGITransport and patched DB connections.
"""
import hashlib
from unittest.mock import AsyncMock
from datetime import datetime

import pytest
import pytest_asyncio
from httpx import ASGITransport, AsyncClient

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from tests.test_utils import PatchGetConnection, make_mock_transaction


def auth_headers(api_key: str) -> dict:
    return {"X-API-Key": api_key}


@pytest.fixture
def sha256_of_test_key():
    return hashlib.sha256("sk-test-key-123".encode()).hexdigest()


@pytest.fixture
def mock_conn():
    conn = AsyncMock()
    conn.fetchrow = AsyncMock(return_value=None)
    conn.fetch = AsyncMock(return_value=[])
    conn.execute = AsyncMock(return_value="INSERT 0 1")
    conn.fetchval = AsyncMock(return_value=None)
    conn.transaction = make_mock_transaction()
    return conn


@pytest_asyncio.fixture
async def app_client_with_auth(mock_conn):
    """
    App with DB connections mocked everywhere and auth returning a valid key.
    """
    conn = mock_conn

    async def mock_fetchrow(query, *args):
        if "api_keys" in query:
            return {
                "id": 1, "owner_name": "Test User", "active": True,
                "created_at": datetime(2026, 1, 1), "last_used_at": None,
            }
        if "role_translations" in query:
            return [{"name": "Дамочка"}, {"name": "Убийца"}]
        if "process_games_import" in query:
            return {"games_created": 1, "players_created": 2, "errors": None}
        if "id::text" in query:
            return {"id": "42"}
        return None

    conn.fetchrow = AsyncMock(side_effect=mock_fetchrow)
    conn.fetch = AsyncMock(side_effect=lambda q, *a: (
        [{"name": "Дамочка"}, {"name": "Убийца"}] if "role_translations" in q else []
    ))
    conn.execute = AsyncMock(return_value="INSERT 0 1")

    # Patch get_connection everywhere
    with PatchGetConnection(conn):
        from app.main import app
        transport = ASGITransport(app=app)
        async with AsyncClient(transport=transport, base_url="http://test") as ac:
            yield ac, conn


@pytest_asyncio.fixture
async def app_client_revoked_key(mock_conn):
    """App with a revoked API key."""
    conn = mock_conn

    async def mock_fetchrow(query, *args):
        if "api_keys" in query:
            return {
                "id": 2, "owner_name": "Revoked User", "active": False,
                "created_at": datetime(2025, 1, 1), "last_used_at": None,
            }
        return None

    conn.fetchrow = AsyncMock(side_effect=mock_fetchrow)

    with PatchGetConnection(conn):
        from app.main import app
        transport = ASGITransport(app=app)
        async with AsyncClient(transport=transport, base_url="http://test") as ac:
            yield ac


# ============================================================
# Health endpoint
# ============================================================

class TestHealthEndpoint:
    @pytest.mark.asyncio
    async def test_health_db_connected(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetchval = AsyncMock(return_value=1)

        response = await client.get("/health")
        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "ok"
        assert data["db_connected"] is True

    @pytest.mark.asyncio
    async def test_health_db_disconnected(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetchval = AsyncMock(side_effect=Exception("Connection lost"))

        response = await client.get("/health")
        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "degraded"
        assert data["db_connected"] is False


# ============================================================
# Game import endpoint
# ============================================================

class TestGameImportEndpoint:
    @pytest.mark.asyncio
    async def test_success(self, app_client_with_auth, valid_game_payload):
        client, conn = app_client_with_auth

        response = await client.post(
            "/api/games/import", json=valid_game_payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "ok"
        assert data["game_id"] == "42"
        assert data["players_created"] == 2

    @pytest.mark.asyncio
    async def test_missing_api_key(self, app_client_with_auth, valid_game_payload):
        client, conn = app_client_with_auth

        response = await client.post("/api/games/import", json=valid_game_payload)

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_invalid_api_key(self, app_client_with_auth, valid_game_payload):
        client, conn = app_client_with_auth
        conn.fetchrow = AsyncMock(return_value=None)

        response = await client.post(
            "/api/games/import", json=valid_game_payload,
            headers=auth_headers("sk-wrong-key"),
        )

        assert response.status_code == 401
        assert "Invalid API key" in response.json()["detail"]

    @pytest.mark.asyncio
    async def test_revoked_api_key(self, app_client_revoked_key, valid_game_payload):
        client = app_client_revoked_key

        response = await client.post(
            "/api/games/import", json=valid_game_payload,
            headers=auth_headers("sk-revoked-key"),
        )

        assert response.status_code == 403
        assert "revoked" in response.json()["detail"].lower()

    @pytest.mark.asyncio
    async def test_missing_roles(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetch = AsyncMock(return_value=[])

        payload = {
            "game_date": "2026-01-15", "scenario_name": "Тест",
            "storyteller_names": ["СТ"], "alignment_win": "добро",
            "location": "X", "game_number": 1,
            "players": [{
                "name": "Игрок", "seat_number": 1,
                "role_start": "НесуществующаяРоль", "role_end": "НесуществующаяРоль",
                "alignment_end": "добро", "is_alive": True,
            }],
        }

        response = await client.post(
            "/api/games/import", json=payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 400
        data = response.json()
        assert "НесуществующаяРоль" in data["detail"]["errors"][0]

    @pytest.mark.asyncio
    async def test_validation_error(self, app_client_with_auth):
        client, conn = app_client_with_auth

        payload = {
            "game_date": "not-a-date", "scenario_name": "",
            "storyteller_names": ["СТ"], "alignment_win": "wrong",
            "location": "X", "game_number": 0, "players": [],
        }

        response = await client.post(
            "/api/games/import", json=payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422


# ============================================================
# Roles list endpoint
# ============================================================

class TestRolesListEndpoint:
    @pytest.mark.asyncio
    async def test_success(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetch = AsyncMock(return_value=[
            {"id": "a09f3ff7-0f02-485a-b0e1-87388b27face", "name": "Chambermaid",
             "alignment": "good", "role_type": "Outsider", "description": None},
            {"id": "b1c2d3e4-f5a6-7890-abcd-1234567890ef", "name": "Imp",
             "alignment": "evil", "role_type": "Demon", "description": None},
        ])

        response = await client.get(
            "/api/roles", headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        data = response.json()
        assert len(data["roles"]) == 2
        assert data["roles"][0]["name"] == "Chambermaid"

    @pytest.mark.asyncio
    async def test_empty_roles(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetch = AsyncMock(return_value=[])

        response = await client.get(
            "/api/roles", headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        data = response.json()
        assert data["roles"] == []

    @pytest.mark.asyncio
    async def test_requires_auth(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.get("/api/roles")

        assert response.status_code == 422


# ============================================================
# Roles import endpoint
# ============================================================

class TestRolesImportEndpoint:
    @pytest.mark.asyncio
    async def test_success(self, app_client_with_auth, valid_roles_import_payload):
        client, conn = app_client_with_auth
        conn.fetchval = AsyncMock(return_value=False)
        conn.execute = AsyncMock(return_value="INSERT 0 1")

        response = await client.post(
            "/api/roles/import", json=valid_roles_import_payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        data = response.json()
        assert data["status"] == "ok"

    @pytest.mark.asyncio
    async def test_requires_auth(self, app_client_with_auth, valid_roles_import_payload):
        client, conn = app_client_with_auth

        response = await client.post(
            "/api/roles/import", json=valid_roles_import_payload,
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_invalid_role_alignment(self, app_client_with_auth):
        client, conn = app_client_with_auth
        payload = {
            "roles": [{"name": "BadRole", "alignment": "wrong", "role_type": "Townsfolk"}]
        }

        response = await client.post(
            "/api/roles/import", json=payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_invalid_role_type(self, app_client_with_auth):
        client, conn = app_client_with_auth
        payload = {
            "roles": [{"name": "BadType", "alignment": "good", "role_type": "InvalidType"}]
        }

        response = await client.post(
            "/api/roles/import", json=payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_empty_roles_list(self, app_client_with_auth):
        client, conn = app_client_with_auth
        payload = {"roles": []}

        response = await client.post(
            "/api/roles/import", json=payload,
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422


# ============================================================
# Fixtures
# ============================================================

@pytest.fixture
def valid_game_payload():
    return {
        "game_date": "2026-01-15",
        "scenario_name": "Вселенная зла",
        "storyteller_names": ["МелонКО"],
        "alignment_win": "добро",
        "location": "Москва, Антикафе на Арбате",
        "game_number": 1,
        "duration": "01:30:00",
        "notes": "Отличная партия",
        "players": [
            {
                "name": "Анна Никитина", "seat_number": 1,
                "role_start": "Дамочка", "role_end": "Дамочка",
                "alignment_end": "добро", "is_alive": True,
            },
            {
                "name": "Борис Петров", "seat_number": 2,
                "role_start": "Убийца", "role_end": "Убийца",
                "alignment_end": "зло", "is_alive": False,
            },
        ],
    }


@pytest.fixture
def valid_roles_import_payload():
    return {
        "roles": [
            {
                "name": "Chambermaid", "alignment": "good", "role_type": "Outsider",
                "description": "Simple, but not harmless",
                "translations": {
                    "ru": {"name": "Горничная", "description": "Просто, но не безобидно"},
                },
            },
            {
                "name": "Imp", "alignment": "evil", "role_type": "Demon",
                "translations": {
                    "ru": {"name": "Имп", "description": "Каждую ночь* выбирайте"}
                },
            },
        ]
    }
