"""
Tests for import_logic.py — import_game, import_roles, _parse_interval, check_db.
"""
from datetime import timedelta
from unittest.mock import AsyncMock
from asyncpg.exceptions import UniqueViolationError

import pytest

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.import_logic import import_game, import_roles, _parse_interval, check_db_connection
from app.models import GameImportRequest, RolesImportRequest, PlayerImportRequest
from tests.test_utils import (
    PatchGetConnection,
    PatchGetConnectionImportLogic,
    make_mock_transaction,
)


# ============================================================
# _parse_interval
# ============================================================

class TestParseInterval:
    """Tests for _parse_interval()."""

    def test_hh_mm_ss(self):
        result = _parse_interval("01:30:00")
        assert result == timedelta(hours=1, minutes=30)

    def test_zero_duration(self):
        result = _parse_interval("00:00:00")
        assert result == timedelta(0)

    def test_d_hh_mm_ss(self):
        result = _parse_interval("1 02:30:00")
        assert result == timedelta(days=1, hours=2, minutes=30)

    def test_multi_day(self):
        result = _parse_interval("3 12:00:00")
        assert result == timedelta(days=3, hours=12)

    def test_none_returns_none(self):
        assert _parse_interval(None) is None

    def test_empty_string_returns_none(self):
        assert _parse_interval("") is None

    def test_whitespace_stripped(self):
        result = _parse_interval("  02:15:30  ")
        assert result == timedelta(hours=2, minutes=15, seconds=30)

    def test_large_hours(self):
        result = _parse_interval("25:00:00")
        assert result == timedelta(hours=25)


# ============================================================
# check_db_connection
# ============================================================

class TestCheckDbConnection:
    """Tests for check_db_connection()."""

    @pytest.mark.asyncio
    async def test_db_connected(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(return_value=1)

        with PatchGetConnectionImportLogic(mock_conn):
            result = await check_db_connection()

        assert result is True

    @pytest.mark.asyncio
    async def test_db_disconnected(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(side_effect=Exception("Connection refused"))

        with PatchGetConnectionImportLogic(mock_conn):
            result = await check_db_connection()

        assert result is False


# ============================================================
# import_game
# ============================================================

@pytest.fixture
def sample_game_request():
    return GameImportRequest(
        game_date="2026-01-15",
        scenario_name="Вселенная зла",
        storyteller_names=["МелонКО"],
        alignment_win="добро",
        location="Москва",
        game_number=1,
        duration="01:30:00",
        notes="Тест",
        players=[
            PlayerImportRequest(
                name="Анна", seat_number=1,
                role_start="Дамочка", role_end="Дамочка",
                alignment_end="добро", is_alive=True,
            ),
            PlayerImportRequest(
                name="Борис", seat_number=2,
                role_start="Убийца", role_end="Убийца",
                alignment_end="зло", is_alive=False,
            ),
        ],
    )


class TestImportGame:
    """Tests for import_game()."""

    @pytest.mark.asyncio
    async def test_success(self, sample_game_request):
        mock_conn = AsyncMock()
        mock_conn.transaction = make_mock_transaction()
        mock_conn.fetch = AsyncMock(return_value=[
            {"name": "Дамочка"}, {"name": "Убийца"},
        ])
        mock_conn.fetchrow = AsyncMock(side_effect=[
            {"games_created": 1, "players_created": 2, "errors": None},
            {"id": "42"},
        ])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        with PatchGetConnection(mock_conn):
            result = await import_game(sample_game_request, {"id": 1})

        assert result.status == "ok"
        assert result.game_id == "42"
        assert result.players_created == 2
        assert result.errors == []

    @pytest.mark.asyncio
    async def test_missing_roles(self, sample_game_request):
        mock_conn = AsyncMock()
        mock_conn.transaction = make_mock_transaction()
        mock_conn.fetch = AsyncMock(return_value=[{"name": "Дамочка"}])

        with PatchGetConnection(mock_conn):
            result = await import_game(sample_game_request, {"id": 1})

        assert result.status == "error"
        assert "Убийца" in result.errors[0]

    @pytest.mark.asyncio
    async def test_duplicate_game(self, sample_game_request):
        mock_conn = AsyncMock()
        mock_conn.transaction = make_mock_transaction()
        mock_conn.fetch = AsyncMock(return_value=[
            {"name": "Дамочка"}, {"name": "Убийца"},
        ])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")
        mock_conn.fetchrow = AsyncMock(
            side_effect=UniqueViolationError("duplicate key")
        )

        with PatchGetConnection(mock_conn):
            result = await import_game(sample_game_request, {"id": 1})

        assert result.status == "error"
        assert "Duplicate" in result.errors[0]

    @pytest.mark.asyncio
    async def test_staging_insert_called_for_each_player(self, sample_game_request):
        mock_conn = AsyncMock()
        mock_conn.transaction = make_mock_transaction()
        mock_conn.fetch = AsyncMock(return_value=[
            {"name": "Дамочка"}, {"name": "Убийца"},
        ])
        mock_conn.fetchrow = AsyncMock(side_effect=[
            {"games_created": 1, "players_created": 2, "errors": None},
            {"id": "42"},
        ])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        with PatchGetConnection(mock_conn):
            await import_game(sample_game_request, {"id": 1})

        staging_calls = [
            c for c in mock_conn.execute.call_args_list
            if "games_import_staging" in str(c)
        ]
        assert len(staging_calls) == 2

    @pytest.mark.asyncio
    async def test_process_errors(self, sample_game_request):
        mock_conn = AsyncMock()
        mock_conn.transaction = make_mock_transaction()
        mock_conn.fetch = AsyncMock(return_value=[
            {"name": "Дамочка"}, {"name": "Убийца"},
        ])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")
        mock_conn.fetchrow = AsyncMock(return_value={
            "games_created": 0, "players_created": 0,
            "errors": "Some DB error",
        })

        with PatchGetConnection(mock_conn):
            result = await import_game(sample_game_request, {"id": 1})

        assert result.status == "error"
        assert "Some DB error" in result.errors[0]


# ============================================================
# import_roles
# ============================================================

class TestImportRoles:
    """Tests for import_roles()."""

    @pytest.mark.asyncio
    async def test_create_new_roles(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(side_effect=[False, False])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {"name": "Chambermaid", "alignment": "good", "role_type": "Outsider"},
            {"name": "Imp", "alignment": "evil", "role_type": "Demon"},
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["status"] == "ok"
        assert result["roles_created"] == 2

    @pytest.mark.asyncio
    async def test_update_existing_roles(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(side_effect=[True, True])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {"name": "Chambermaid", "alignment": "good", "role_type": "Townsfolk"},
            {"name": "Imp", "alignment": "evil", "role_type": "Demon"},
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["status"] == "ok"
        assert result["roles_updated"] == 2

    @pytest.mark.asyncio
    async def test_mixed_create_and_update(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(side_effect=[True, False])
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {"name": "Existing", "alignment": "good", "role_type": "Townsfolk"},
            {"name": "NewRole", "alignment": "evil", "role_type": "Demon"},
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["roles_created"] == 1
        assert result["roles_updated"] == 1

    @pytest.mark.asyncio
    async def test_upsert_translations(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(return_value=False)
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {
                "name": "Chambermaid", "alignment": "good", "role_type": "Outsider",
                "translations": {"ru": {"name": "Горничная", "description": "Описание"}},
            }
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["status"] == "ok"
        execute_calls = [str(c) for c in mock_conn.execute.call_args_list]
        assert any("role_translations" in c for c in execute_calls)

    @pytest.mark.asyncio
    async def test_without_translations(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(return_value=False)
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {"name": "Cook", "alignment": "good", "role_type": "Townsfolk"},
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["status"] == "ok"
        assert result["roles_created"] == 1

    @pytest.mark.asyncio
    async def test_partial_error(self):
        mock_conn = AsyncMock()
        call_count = [0]

        async def mock_fetchval(query, *args):
            call_count[0] += 1
            if call_count[0] == 1:
                return False
            raise Exception("DB error")

        mock_conn.fetchval = AsyncMock(side_effect=mock_fetchval)
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {"name": "GoodRole", "alignment": "good", "role_type": "Townsfolk"},
            {"name": "BadRole", "alignment": "good", "role_type": "Townsfolk"},
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["status"] == "error"
        assert len(result["errors"]) >= 1
        assert any("BadRole" in e for e in result["errors"])

    @pytest.mark.asyncio
    async def test_multiple_language_translations(self):
        mock_conn = AsyncMock()
        mock_conn.fetchval = AsyncMock(return_value=False)
        mock_conn.execute = AsyncMock(return_value="INSERT 0 1")

        payload = RolesImportRequest(roles=[
            {
                "name": "Chambermaid", "alignment": "good", "role_type": "Outsider",
                "translations": {
                    "ru": {"name": "Горничная", "description": "RU desc"},
                    "de": {"name": "Kammerdiener", "description": "DE desc"},
                },
            }
        ])

        with PatchGetConnection(mock_conn):
            result = await import_roles(payload)

        assert result["status"] == "ok"
        assert mock_conn.execute.call_count >= 3
