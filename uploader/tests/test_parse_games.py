"""
Tests for parse_csv() function — game parsing and grouping logic.
"""
import csv
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from uploader import parse_csv


class TestParseCsv:
    """Tests for parse_csv()."""

    def test_parses_two_games(self, valid_games_csv):
        """Correctly parses CSV with 2 games."""
        games = parse_csv(valid_games_csv)

        assert len(games) == 2

    def test_game_1_has_3_players(self, valid_games_csv):
        """First game has 3 players."""
        games = parse_csv(valid_games_csv)
        game1 = [g for g in games if g["game_number"] == 1][0]

        assert len(game1["players"]) == 3

    def test_game_2_has_2_players(self, valid_games_csv):
        """Second game has 2 players."""
        games = parse_csv(valid_games_csv)
        game2 = [g for g in games if g["game_number"] == 1 and g["scenario_name"] == "Сценарий2"][0]

        assert len(game2["players"]) == 2

    def test_game_fields(self, valid_games_csv):
        """Game fields are correctly populated."""
        games = parse_csv(valid_games_csv)
        game1 = [g for g in games if g["game_number"] == 1 and g["scenario_name"] == "Сценарий1"][0]

        assert game1["game_date"] == "2026-01-15"
        assert game1["scenario_name"] == "Сценарий1"
        assert game1["storyteller_name"] == "Рассказчик"
        assert game1["alignment_win"] == "добро"
        assert game1["location"] == "Москва"
        assert game1["duration"] == "01:30:00"
        assert game1["notes"] == "Тест"

    def test_empty_duration_and_notes(self, valid_games_csv):
        """Empty duration and notes become None."""
        games = parse_csv(valid_games_csv)
        game2 = [g for g in games if g["scenario_name"] == "Сценарий2"][0]

        assert game2["duration"] is None
        assert game2["notes"] is None

    def test_player_fields(self, valid_games_csv):
        """Player fields are correctly populated."""
        games = parse_csv(valid_games_csv)
        game1 = [g for g in games if g["game_number"] == 1 and g["scenario_name"] == "Сценарий1"][0]
        player = game1["players"][0]

        assert player["name"] == "Игрок1"
        assert player["seat_number"] == 1
        assert player["role_start"] == "Дамочка"
        assert player["role_end"] == "Дамочка"
        assert player["alignment_end"] == "добро"
        assert player["is_alive"] is True

    def test_is_alive_false(self, valid_games_csv):
        """is_alive=False for 'false' string."""
        games = parse_csv(valid_games_csv)
        game1 = [g for g in games if g["game_number"] == 1 and g["scenario_name"] == "Сценарий1"][0]
        player = game1["players"][1]  # Игрок2 with is_alive=false

        assert player["is_alive"] is False

    def test_is_alive_variants(self, tmp_path):
        """Various is_alive truthy variants are parsed correctly."""
        csv_path = tmp_path / "alive.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "game_date", "scenario_name", "location", "game_number",
                "storyteller_name", "alignment_win", "player_name",
                "role_start_name", "role_end_name", "alignment_end",
                "is_alive", "seat_number", "duration", "notes"
            ])
            for val in ("true", "1", "yes", "да", "false", "0", "no", "нет"):
                writer.writerow([
                    "2026-01-01", "Тест", "X", 1, "СТ", "добро",
                    f"P{val}", "Роль", "Роль", "добро", val, 1, "", ""
                ])

        games = parse_csv(csv_path)
        assert len(games) == 1
        players = {p["name"]: p["is_alive"] for p in games[0]["players"]}

        assert players["Ptrue"] is True
        assert players["P1"] is True
        assert players["Pyes"] is True
        assert players["Pда"] is True
        assert players["Pfalse"] is False
        assert players["P0"] is False
        assert players["Pno"] is False
        assert players["Pнет"] is False

    def test_seat_number_is_int(self, valid_games_csv):
        """seat_number is converted to int."""
        games = parse_csv(valid_games_csv)
        game1 = [g for g in games if g["game_number"] == 1 and g["scenario_name"] == "Сценарий1"][0]

        for player in game1["players"]:
            assert isinstance(player["seat_number"], int)

    def test_game_number_is_int(self, valid_games_csv):
        """game_number is converted to int."""
        games = parse_csv(valid_games_csv)

        for game in games:
            assert isinstance(game["game_number"], int)

    def test_whitespace_stripped(self, tmp_path):
        """Leading/trailing whitespace is stripped from all fields."""
        csv_path = tmp_path / "ws.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "game_date", "scenario_name", "location", "game_number",
                "storyteller_name", "alignment_win", "player_name",
                "role_start_name", "role_end_name", "alignment_end",
                "is_alive", "seat_number", "duration", "notes"
            ])
            writer.writerow([
                " 2026-01-01 ", " Тест ", " Локация ", " 1 ", " СТ ", " добро ",
                " Игрок ", " Роль ", " Роль ", " добро ", " true ", " 1 ", " 00:30:00 ", " Заметка "
            ])

        games = parse_csv(csv_path)
        game = games[0]

        assert game["game_date"] == "2026-01-01"
        assert game["scenario_name"] == "Тест"
        assert game["location"] == "Локация"
        assert game["storyteller_name"] == "СТ"
        assert game["players"][0]["name"] == "Игрок"

    def test_single_game_single_player(self, single_game_csv):
        """Parses a CSV with exactly one game and one player."""
        games = parse_csv(single_game_csv)

        assert len(games) == 1
        assert len(games[0]["players"]) == 1
        assert games[0]["game_date"] == "2026-03-01"
        assert games[0]["scenario_name"] == "Тест"
        assert games[0]["players"][0]["name"] == "Один"

    def test_same_game_number_different_scenarios(self, tmp_path):
        """Same game_number but different scenario_name creates separate games."""
        csv_path = tmp_path / "multi_scenario.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "game_date", "scenario_name", "location", "game_number",
                "storyteller_name", "alignment_win", "player_name",
                "role_start_name", "role_end_name", "alignment_end",
                "is_alive", "seat_number", "duration", "notes"
            ])
            # Same game_number=1, different dates/scenarios
            writer.writerow([
                "2026-01-01", "СценарийА", "X", 1, "СТ", "добро",
                "Игрок1", "Роль", "Роль", "добро", "true", 1, "", ""
            ])
            writer.writerow([
                "2026-01-02", "СценарийБ", "X", 1, "СТ", "зло",
                "Игрок2", "Роль", "Роль", "зло", "false", 1, "", ""
            ])

        games = parse_csv(csv_path)
        assert len(games) == 2

    def test_grouping_key_includes_location(self, tmp_path):
        """Same date/scenario but different location creates separate games."""
        csv_path = tmp_path / "multi_loc.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "game_date", "scenario_name", "location", "game_number",
                "storyteller_name", "alignment_win", "player_name",
                "role_start_name", "role_end_name", "alignment_end",
                "is_alive", "seat_number", "duration", "notes"
            ])
            writer.writerow([
                "2026-01-01", "Сценарий", "Москва", 1, "СТ", "добро",
                "Игрок1", "Роль", "Роль", "добро", "true", 1, "", ""
            ])
            writer.writerow([
                "2026-01-01", "Сценарий", "Питер", 1, "СТ", "добро",
                "Игрок2", "Роль", "Роль", "добро", "true", 1, "", ""
            ])

        games = parse_csv(csv_path)
        assert len(games) == 2
        locations = {g["location"] for g in games}
        assert locations == {"Москва", "Питер"}

    def test_returns_list_of_dicts(self, valid_games_csv):
        """Returns a list of dictionaries."""
        games = parse_csv(valid_games_csv)

        assert isinstance(games, list)
        for game in games:
            assert isinstance(game, dict)
            assert "players" in game
            assert isinstance(game["players"], list)

    def test_role_start_and_role_end(self, valid_games_csv):
        """role_start and role_end are parsed correctly."""
        games = parse_csv(valid_games_csv)
        game = games[0]

        for player in game["players"]:
            assert isinstance(player["role_start"], str)
            assert isinstance(player["role_end"], str)
            assert len(player["role_start"]) > 0
            assert len(player["role_end"]) > 0

    def test_alignment_end(self, valid_games_csv):
        """alignment_end is parsed correctly."""
        games = parse_csv(valid_games_csv)
        game = games[0]

        alignments = {p["name"]: p["alignment_end"] for p in game["players"]}
        assert alignments["Игрок1"] == "добро"
        assert alignments["Игрок2"] == "зло"
