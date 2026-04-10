"""
Shared fixtures for uploader tests.
"""
import csv
import os
import tempfile
from pathlib import Path
from unittest.mock import MagicMock

import pytest


# ============================================================
# Fixtures: temporary .env files
# ============================================================

@pytest.fixture
def tmp_env_file(tmp_path):
    """Create a temporary directory with a valid .env file."""
    env_content = """\
API_URL=https://localhost:443
API_KEY=sk-test-key-123
SSL_VERIFY=false
"""
    env_path = tmp_path / ".env"
    env_path.write_text(env_content, encoding="utf-8")
    return tmp_path


@pytest.fixture
def tmp_env_no_api_key(tmp_path):
    """.env file without API_KEY."""
    env_content = """\
API_URL=https://localhost:443
SSL_VERIFY=false
"""
    env_path = tmp_path / ".env"
    env_path.write_text(env_content, encoding="utf-8")
    return tmp_path


@pytest.fixture
def tmp_env_defaults(tmp_path):
    """.env file with only API_KEY (other values use defaults)."""
    env_content = """\
API_KEY=sk-default-key
"""
    env_path = tmp_path / ".env"
    env_path.write_text(env_content, encoding="utf-8")
    return tmp_path


# ============================================================
# Fixtures: temporary CSV files (games)
# ============================================================

@pytest.fixture
def valid_games_csv(tmp_path):
    """CSV with 2 games, 3 players each."""
    csv_path = tmp_path / "games.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "game_date", "scenario_name", "location", "game_number",
            "storyteller_name", "alignment_win", "player_name",
            "role_start_name", "role_end_name", "alignment_end",
            "is_alive", "seat_number", "duration", "notes"
        ])
        # Game 1
        for seat, (name, role, align, alive) in enumerate([
            ("Игрок1", "Дамочка", "добро", "true"),
            ("Игрок2", "Убийца", "зло", "false"),
            ("Игрок3", "Политик", "добро", "true"),
        ], start=1):
            writer.writerow([
                "2026-01-15", "Сценарий1", "Москва", 1, "Рассказчик", "добро",
                name, role, role, align, alive, seat, "01:30:00", "Тест"
            ])
        # Game 2
        for seat, (name, role, align, alive) in enumerate([
            ("Игрок1", "Амнезиак", "добро", "true"),
            ("Игрок2", "Имп", "зло", "false"),
        ], start=1):
            writer.writerow([
                "2026-01-20", "Сценарий2", "Питер", 1, "Рассказчик2", "зло",
                name, role, role, align, alive, seat, "", ""
            ])
    return csv_path


@pytest.fixture
def single_game_csv(tmp_path):
    """CSV with a single game and a single player."""
    csv_path = tmp_path / "single_game.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "game_date", "scenario_name", "location", "game_number",
            "storyteller_name", "alignment_win", "player_name",
            "role_start_name", "role_end_name", "alignment_end",
            "is_alive", "seat_number", "duration", "notes"
        ])
        writer.writerow([
            "2026-03-01", "Тест", "Локация", 1, "СТ", "добро",
            "Один", "Роль", "Роль", "добро", "true", 1, "00:30:00", "Заметка"
        ])
    return csv_path


# ============================================================
# Fixtures: temporary CSV files (roles)
# ============================================================

@pytest.fixture
def valid_roles_csv(tmp_path):
    """CSV with valid roles including translations."""
    csv_path = tmp_path / "roles.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "name", "alignment", "role_type", "description",
            "ru_name", "ru_description"
        ])
        writer.writerow([
            "Chambermaid", "good", "Townsfolk", "Simple, but not harmless",
            "Горничная", "Просто, но не безобидно"
        ])
        writer.writerow([
            "Imp", "evil", "Demon", "Each night* choose a player",
            "Имп", "Каждую ночь* выбирайте игрока"
        ])
        writer.writerow([
            "Spy", "evil", "Minion", None,
            "Шпион", None
        ])
    return csv_path


@pytest.fixture
def roles_no_translations_csv(tmp_path):
    """CSV with roles but no translation columns."""
    csv_path = tmp_path / "roles_no_trans.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["name", "alignment", "role_type", "description"])
        writer.writerow(["Washerwoman", "good", "Townsfolk", "Each night, choose"])
        writer.writerow(["Poisoner", "evil", "Minion", None])
    return csv_path


@pytest.fixture
def roles_invalid_csv(tmp_path):
    """CSV with some invalid roles for validation testing."""
    csv_path = tmp_path / "roles_invalid.csv"
    with open(csv_path, "w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["name", "alignment", "role_type", "description", "ru_name", "ru_description"])
        # Valid row
        writer.writerow(["Cook", "good", "Townsfolk", "Night ability", "Повар", "Ночная способность"])
        # Invalid alignment
        writer.writerow(["BadRole", "wrong", "Townsfolk", "", "Плохой", ""])
        # Invalid role_type
        writer.writerow(["BadType", "good", "InvalidType", "", "Тип", ""])
        # Empty name
        writer.writerow(["", "good", "Townsfolk", "", "Пусто", ""])
    return csv_path


# ============================================================
# Fixtures: mock API responses
# ============================================================

@pytest.fixture
def mock_success_response():
    """Mock successful API response."""
    mock = MagicMock()
    mock.status_code = 200
    mock.json.return_value = {
        "status": "ok",
        "game_id": 1,
        "players_created": 3,
    }
    return mock


@pytest.fixture
def mock_roles_success_response():
    """Mock successful roles import API response."""
    mock = MagicMock()
    mock.status_code = 200
    mock.json.return_value = {
        "status": "ok",
        "roles_created": 2,
        "roles_updated": 1,
    }
    return mock


@pytest.fixture
def mock_http_error_response():
    """Mock HTTP error response with JSON detail."""
    mock = MagicMock()
    mock.status_code = 400
    mock.json.return_value = {
        "detail": {
            "status": "error",
            "errors": ["Role 'FakeRole': invalid alignment"],
        }
    }
    mock.raise_for_status.side_effect = Exception("400 Client Error")
    return mock
