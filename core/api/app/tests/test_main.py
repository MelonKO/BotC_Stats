"""
Tests for FastAPI endpoints in main.py.
Uses httpx.AsyncClient with ASGITransport and patched DB connections.
"""
import hashlib
from unittest.mock import AsyncMock
from datetime import datetime
from uuid import UUID

import pytest
import pytest_asyncio
from asyncpg.exceptions import UniqueViolationError
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
    async def test_translated_type_and_side(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetch = AsyncMock(return_value=[
            {"id": "a09f3ff7-0f02-485a-b0e1-87388b27face", "name": "Chambermaid",
             "alignment": "good", "role_type": "Outsider", "description": None,
             "translation_name": "Дамочка", "translation_description": None,
             "role_type_translation": "Изгой", "alignment_translation": "добро"},
            {"id": "b1c2d3e4-f5a6-7890-abcd-1234567890ef", "name": "Imp",
             "alignment": "evil", "role_type": "Demon", "description": None,
             "translation_name": None, "translation_description": None,
             "role_type_translation": None, "alignment_translation": None},
        ])

        response = await client.get(
            "/api/roles", params={"lang": "ru"},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        data = response.json()
        assert data["roles"][0]["role_type_translation"] == "Изгой"
        assert data["roles"][0]["alignment_translation"] == "добро"
        assert data["roles"][0]["translation"]["name"] == "Дамочка"
        # A language without translations must not break the list: the localized
        # fields stay null so the client falls back to the reference values.
        assert data["roles"][1]["role_type_translation"] is None
        assert data["roles"][1]["alignment_translation"] is None
        assert data["roles"][1]["translation"] is None

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
# Players list endpoint
# ============================================================

class TestPlayersListEndpoint:
    @pytest.mark.asyncio
    async def test_success(self, app_client_with_auth):
        client, conn = app_client_with_auth
        # PlayerItem.id is a UUID in the generated models, so the mocked rows
        # have to carry real UUID strings.
        conn.fetch = AsyncMock(return_value=[
            {"id": "b1c2d3e4-f5a6-7890-abcd-1234567890ef", "name": "Мая Вишневская"},
            {"id": "a09f3ff7-0f02-485a-b0e1-87388b27face", "name": "Анна Никитина"},
        ])

        response = await client.get(
            "/api/players", headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        data = response.json()
        assert len(data["players"]) == 2
        assert data["players"][0]["name"] == "Мая Вишневская"
        assert data["players"][0]["id"] == "b1c2d3e4-f5a6-7890-abcd-1234567890ef"

    @pytest.mark.asyncio
    async def test_empty_players(self, app_client_with_auth):
        client, conn = app_client_with_auth
        conn.fetch = AsyncMock(return_value=[])

        response = await client.get(
            "/api/players", headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        assert response.json()["players"] == []

    @pytest.mark.asyncio
    async def test_requires_auth(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.get("/api/players")

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_invalid_api_key(self, app_client_with_auth):
        client, conn = app_client_with_auth
        # No api_keys row means the key is unknown.
        conn.fetchrow = AsyncMock(return_value=None)

        response = await client.get(
            "/api/players", headers=auth_headers("sk-unknown-key"),
        )

        assert response.status_code == 401


# ============================================================
# Player rename endpoint
# ============================================================

PLAYER_ID = "b1c2d3e4-f5a6-7890-abcd-1234567890ef"


def valid_api_key_row() -> dict:
    return {
        "id": 1, "owner_name": "Test User", "active": True,
        "created_at": datetime(2026, 1, 1), "last_used_at": None,
    }


def rename_conn(stored_row, calls: list):
    """Build a fetchrow mock answering auth and recording rename arguments."""
    async def mock_fetchrow(query, *args):
        if "api_keys" in query:
            return valid_api_key_row()
        if "update players" in query:
            calls.append(args)
            return stored_row
        return None

    return mock_fetchrow


class TestPlayerRenameEndpoint:
    @pytest.mark.asyncio
    async def test_success(self, app_client_with_auth):
        client, conn = app_client_with_auth
        calls = []
        conn.fetchrow = AsyncMock(
            side_effect=rename_conn({"id": PLAYER_ID, "name": "Мая Вишневская-Новая"}, calls)
        )

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "Мая Вишневская-Новая"},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        assert response.json() == {"id": PLAYER_ID, "name": "Мая Вишневская-Новая"}
        # The id and the trimmed name travel to the database as parameters.
        assert calls == [(UUID(PLAYER_ID), "Мая Вишневская-Новая")]

    @pytest.mark.asyncio
    async def test_trims_surrounding_spaces(self, app_client_with_auth):
        client, conn = app_client_with_auth
        calls = []
        conn.fetchrow = AsyncMock(side_effect=rename_conn({"id": PLAYER_ID, "name": "Мая"}, calls))

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "  Мая  "},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200
        assert calls[0][1] == "Мая"

    @pytest.mark.asyncio
    async def test_keeps_the_same_name(self, app_client_with_auth):
        client, conn = app_client_with_auth
        calls = []
        conn.fetchrow = AsyncMock(side_effect=rename_conn({"id": PLAYER_ID, "name": "Мая"}, calls))

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "Мая"},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 200

    @pytest.mark.asyncio
    async def test_unknown_player(self, app_client_with_auth):
        client, conn = app_client_with_auth
        calls = []
        conn.fetchrow = AsyncMock(side_effect=rename_conn(None, calls))

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "Мая"},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 404

    @pytest.mark.asyncio
    async def test_name_already_taken(self, app_client_with_auth):
        client, conn = app_client_with_auth

        async def mock_fetchrow(query, *args):
            if "api_keys" in query:
                return valid_api_key_row()
            raise UniqueViolationError('duplicate key value violates unique constraint "players_name_key"')

        conn.fetchrow = AsyncMock(side_effect=mock_fetchrow)

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "Анна Никитина"},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 409

    @pytest.mark.asyncio
    async def test_blank_name(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "   "},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_empty_name(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": ""},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_too_long_name(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "я" * 201},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_invalid_player_id(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.patch(
            "/api/players/not-a-uuid",
            json={"name": "Мая"},
            headers=auth_headers("sk-test-key-123"),
        )

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_requires_auth(self, app_client_with_auth):
        client, conn = app_client_with_auth

        response = await client.patch(f"/api/players/{PLAYER_ID}", json={"name": "Мая"})

        assert response.status_code == 422

    @pytest.mark.asyncio
    async def test_invalid_api_key(self, app_client_with_auth):
        client, conn = app_client_with_auth
        # No api_keys row means the key is unknown.
        conn.fetchrow = AsyncMock(return_value=None)

        response = await client.patch(
            f"/api/players/{PLAYER_ID}",
            json={"name": "Мая"},
            headers=auth_headers("sk-unknown-key"),
        )

        assert response.status_code == 401


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
