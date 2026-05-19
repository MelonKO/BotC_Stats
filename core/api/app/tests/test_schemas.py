"""
Tests for Pydantic schemas — validation, patterns, constraints.
"""
import pytest

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.models import (
    PlayerImportRequest,
    GameImportRequest,
    RoleImportItem,
    RolesImportRequest,
    RoleTranslation,
    ImportStatusResponse,
    RolesImportResponse,
    HealthResponse,
    RolesResponse,
)
from datetime import date


# ============================================================
# PlayerImportRequest
# ============================================================

class TestPlayerImportRequest:
    """Tests for PlayerImportRequest validation."""

    def test_valid_player(self):
        """Valid player passes validation."""
        player = PlayerImportRequest(
            name="Анна",
            seat_number=1,
            role_start="Дамочка",
            role_end="Дамочка",
            alignment_end="добро",
            is_alive=True,
        )
        assert player.name == "Анна"
        assert player.seat_number == 1

    def test_empty_name_fails(self):
        """Empty name fails validation."""
        with pytest.raises(Exception):  # Pydantic ValidationError
            PlayerImportRequest(
                name="",
                seat_number=1,
                role_start="Роль",
                role_end="Роль",
                alignment_end="добро",
                is_alive=True,
            )

    def test_invalid_alignment_end(self):
        """Non-allowed alignment_end fails."""
        with pytest.raises(Exception):
            PlayerImportRequest(
                name="Игрок",
                role_start="Роль",
                role_end="Роль",
                alignment_end="wrong",
                is_alive=True,
            )

    def test_valid_alignments_end(self):
        """All valid alignment_end values pass."""
        for align in ("добро", "зло", "нейтральный"):
            player = PlayerImportRequest(
                name="Игрок",
                role_start="Роль",
                role_end="Роль",
                alignment_end=align,
                is_alive=True,
            )
            assert player.alignment_end == align

    def test_seat_number_none(self):
        """seat_number can be None."""
        player = PlayerImportRequest(
            name="Игрок",
            seat_number=None,
            role_start="Роль",
            role_end="Роль",
            alignment_end="добро",
            is_alive=True,
        )
        assert player.seat_number is None

    def test_seat_number_negative_fails(self):
        """Negative seat_number fails validation."""
        with pytest.raises(Exception):
            PlayerImportRequest(
                name="Игрок",
                seat_number=-1,
                role_start="Роль",
                role_end="Роль",
                alignment_end="добро",
                is_alive=True,
            )

    def test_seat_number_zero_fails(self):
        """seat_number=0 fails validation (ge=1)."""
        with pytest.raises(Exception):
            PlayerImportRequest(
                name="Игрок",
                seat_number=0,
                role_start="Роль",
                role_end="Роль",
                alignment_end="добро",
                is_alive=True,
            )


# ============================================================
# GameImportRequest
# ============================================================

class TestGameImportRequest:
    """Tests for GameImportRequest validation."""

    def _minimal_valid_game(self):
        return {
            "game_date": date(2026, 1, 15),
            "scenario_name": "Тест",
            "storyteller_name": "СТ",
            "alignment_win": "добро",
            "location": "Локация",
            "game_number": 1,
            "players": [
                {
                    "name": "Игрок",
                    "seat_number": 1,
                    "role_start": "Роль",
                    "role_end": "Роль",
                    "alignment_end": "добро",
                    "is_alive": True,
                }
            ],
        }

    def test_valid_game(self):
        """Valid game passes."""
        data = self._minimal_valid_game()
        game = GameImportRequest(**data)
        assert game.scenario_name == "Тест"

    def test_invalid_date(self):
        """Invalid date string fails."""
        data = self._minimal_valid_game()
        data["game_date"] = "not-a-date"
        with pytest.raises(Exception):
            GameImportRequest(**data)

    def test_invalid_alignment_win(self):
        """Invalid alignment_win pattern fails."""
        data = self._minimal_valid_game()
        data["alignment_win"] = "neutral"
        with pytest.raises(Exception):
            GameImportRequest(**data)

    def test_valid_alignment_win_values(self):
        """Only 'добро' and 'зло' are valid."""
        for align in ("добро", "зло"):
            data = self._minimal_valid_game()
            data["alignment_win"] = align
            game = GameImportRequest(**data)
            assert game.alignment_win == align

    def test_empty_scenario_fails(self):
        """Empty scenario_name fails."""
        data = self._minimal_valid_game()
        data["scenario_name"] = ""
        with pytest.raises(Exception):
            GameImportRequest(**data)

    def test_empty_players_fails(self):
        """Empty players list fails (min_length=1)."""
        data = self._minimal_valid_game()
        data["players"] = []
        with pytest.raises(Exception):
            GameImportRequest(**data)

    def test_game_number_zero_fails(self):
        """game_number=0 fails (ge=1)."""
        data = self._minimal_valid_game()
        data["game_number"] = 0
        with pytest.raises(Exception):
            GameImportRequest(**data)

    def test_optional_duration_none(self):
        """duration can be None."""
        data = self._minimal_valid_game()
        data["duration"] = None
        game = GameImportRequest(**data)
        assert game.duration is None

    def test_optional_notes_none(self):
        """notes can be None."""
        data = self._minimal_valid_game()
        data["notes"] = None
        game = GameImportRequest(**data)
        assert game.notes is None

    def test_string_date(self):
        """String date '2026-01-15' is parsed to date."""
        data = self._minimal_valid_game()
        data["game_date"] = "2026-01-15"
        game = GameImportRequest(**data)
        assert game.game_date == date(2026, 1, 15)


# ============================================================
# RoleImportItem
# ============================================================

class TestRoleImportItem:
    """Tests for RoleImportItem validation."""

    def test_valid_role(self):
        """Valid role passes."""
        role = RoleImportItem(
            name="Chambermaid",
            alignment="good",
            role_type="Outsider",
        )
        assert role.name == "Chambermaid"

    def test_invalid_alignment(self):
        """Invalid alignment fails."""
        with pytest.raises(Exception):
            RoleImportItem(
                name="Bad",
                alignment="wrong",
                role_type="Townsfolk",
            )

    def test_valid_alignments(self):
        """good, evil, neutral are valid."""
        for a in ("good", "evil", "neutral"):
            role = RoleImportItem(name="R", alignment=a, role_type="Townsfolk")
            assert role.alignment == a

    def test_invalid_role_type(self):
        """Invalid role_type fails."""
        with pytest.raises(Exception):
            RoleImportItem(
                name="Bad",
                alignment="good",
                role_type="InvalidType",
            )

    def test_valid_role_types(self):
        """All 5 role types are valid."""
        for rt in ("Townsfolk", "Outsider", "Minion", "Demon", "Traveller"):
            role = RoleImportItem(name="R", alignment="good", role_type=rt)
            assert role.role_type == rt

    def test_description_optional(self):
        """description is optional."""
        role = RoleImportItem(name="R", alignment="good", role_type="Townsfolk")
        assert role.description is None

    def test_translations_optional(self):
        """translations is optional."""
        role = RoleImportItem(name="R", alignment="good", role_type="Townsfolk")
        assert role.translations is None

    def test_translations_with_valid_data(self):
        """Valid translations pass."""
        role = RoleImportItem(
            name="R",
            alignment="good",
            role_type="Townsfolk",
            translations={
                "ru": {"name": "Роль", "description": "Описание"},
            },
        )
        assert role.translations["ru"].name == "Роль"

    def test_empty_name_fails(self):
        """Empty name fails."""
        with pytest.raises(Exception):
            RoleImportItem(name="", alignment="good", role_type="Townsfolk")


# ============================================================
# RoleTranslation
# ============================================================

class TestRoleTranslation:
    """Tests for RoleTranslation validation."""

    def test_valid_translation(self):
        """Valid translation passes."""
        tr = RoleTranslation(name="Дамочка", description="Описание")
        assert tr.name == "Дамочка"

    def test_description_optional(self):
        """description is optional."""
        tr = RoleTranslation(name="Дамочка")
        assert tr.description is None

    def test_empty_name_fails(self):
        """Empty name fails."""
        with pytest.raises(Exception):
            RoleTranslation(name="")


# ============================================================
# RolesImportRequest
# ============================================================

class TestRolesImportRequest:
    """Tests for RolesImportRequest validation."""

    def test_valid_request(self):
        """Valid request with roles passes."""
        req = RolesImportRequest(roles=[
            {"name": "Chambermaid", "alignment": "good", "role_type": "Outsider"},
        ])
        assert len(req.roles) == 1

    def test_empty_roles_fails(self):
        """Empty roles list fails (min_length=1)."""
        with pytest.raises(Exception):
            RolesImportRequest(roles=[])


# ============================================================
# Response schemas
# ============================================================

class TestResponseSchemas:
    """Tests for response model defaults."""

    def test_import_status_response_defaults(self):
        """ImportStatusResponse has correct defaults."""
        r = ImportStatusResponse(status="ok")
        assert r.game_id is None
        assert r.players_created == 0
        assert r.errors == []

    def test_roles_import_response_defaults(self):
        """RolesImportResponse has correct defaults."""
        r = RolesImportResponse(status="ok")
        assert r.roles_created == 0
        assert r.roles_updated == 0
        assert r.errors == []

    def test_health_response(self):
        """HealthResponse works."""
        r = HealthResponse(status="ok", db_connected=True)
        assert r.status == "ok"
        assert r.db_connected is True

    def test_roles_response(self):
        """RolesResponse works."""
        r = RolesResponse(roles=[{"name": "Chambermaid", "alignment": "good"}])
        assert len(r.roles) == 1
