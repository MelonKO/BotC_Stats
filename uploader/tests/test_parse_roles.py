"""
Tests for parse_roles_csv() function — role parsing and validation.
"""
import csv
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from uploader import parse_roles_csv


class TestParseRolesCsv:
    """Tests for parse_roles_csv()."""

    def test_parses_all_roles(self, valid_roles_csv):
        """Parses all 3 roles from valid CSV."""
        roles = parse_roles_csv(valid_roles_csv)

        assert len(roles) == 3

    def test_role_fields(self, valid_roles_csv):
        """Role fields are correctly populated."""
        roles = parse_roles_csv(valid_roles_csv)
        chambermaid = next(r for r in roles if r["name"] == "Chambermaid")

        assert chambermaid["alignment"] == "good"
        assert chambermaid["role_type"] == "Townsfolk"
        assert chambermaid["description"] == "Simple, but not harmless"

    def test_role_with_none_description(self, valid_roles_csv):
        """Role with empty description has description=None."""
        roles = parse_roles_csv(valid_roles_csv)
        spy = next(r for r in roles if r["name"] == "Spy")

        assert spy.get("description") is None

    def test_translations_present(self, valid_roles_csv):
        """Translations are parsed for roles with translation columns."""
        roles = parse_roles_csv(valid_roles_csv)
        chambermaid = next(r for r in roles if r["name"] == "Chambermaid")

        assert "translations" in chambermaid
        assert "ru" in chambermaid["translations"]
        assert chambermaid["translations"]["ru"]["name"] == "Горничная"
        assert chambermaid["translations"]["ru"]["description"] == "Просто, но не безобидно"

    def test_translation_with_none_description(self, valid_roles_csv):
        """Translation with empty description omits description key."""
        roles = parse_roles_csv(valid_roles_csv)
        spy = next(r for r in roles if r["name"] == "Spy")

        assert "ru" in spy["translations"]
        assert spy["translations"]["ru"]["name"] == "Шпион"
        assert "description" not in spy["translations"]["ru"]

    def test_no_translation_columns(self, roles_no_translations_csv):
        """Roles without translation columns have no translations key."""
        roles = parse_roles_csv(roles_no_translations_csv)

        assert len(roles) == 2
        for role in roles:
            assert "translations" not in role

    def test_valid_alignment_values(self):
        """All valid alignment values are accepted."""
        for alignment in ("good", "evil", "neutral"):
            import tempfile
            with tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False, encoding="utf-8", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["name", "alignment", "role_type"])
                writer.writerow([f"Role{alignment}", alignment, "Townsfolk"])
                fname = f.name

            roles = parse_roles_csv(Path(fname))
            assert len(roles) == 1
            assert roles[0]["alignment"] == alignment
            Path(fname).unlink()

    def test_invalid_alignment(self, roles_invalid_csv, capsys):
        """Invalid alignment raises validation error."""
        roles = parse_roles_csv(roles_invalid_csv)

        # Only the valid row should be returned
        names = {r["name"] for r in roles}
        assert "BadRole" not in names
        assert "Cook" in names

    def test_invalid_role_type(self, roles_invalid_csv):
        """Invalid role_type raises validation error."""
        roles = parse_roles_csv(roles_invalid_csv)

        names = {r["name"] for r in roles}
        assert "BadType" not in names

    def test_empty_name_skipped(self, roles_invalid_csv, capsys):
        """Empty name rows are skipped."""
        roles = parse_roles_csv(roles_invalid_csv)

        names = {r["name"] for r in roles}
        assert "" not in names

    def test_mixed_valid_invalid(self, roles_invalid_csv, capsys):
        """Valid roles are returned, invalid ones skipped with errors."""
        roles = parse_roles_csv(roles_invalid_csv)

        assert len(roles) == 1
        assert roles[0]["name"] == "Cook"

    def test_missing_required_columns(self, tmp_path):
        """SystemExit when required columns are missing."""
        csv_path = tmp_path / "bad_headers.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["name", "color", "type"])  # Wrong column names
            writer.writerow(["Role", "good", "Townsfolk"])

        with pytest.raises(SystemExit) as exc_info:
            parse_roles_csv(csv_path)

        assert exc_info.value.code == 1

    def test_all_valid_role_types(self):
        """All valid role_type values are accepted."""
        import tempfile
        role_types = ["Townsfolk", "Outsider", "Minion", "Demon", "Traveller"]

        for i, role_type in enumerate(role_types):
            with tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False, encoding="utf-8", newline="") as f:
                writer = csv.writer(f)
                writer.writerow(["name", "alignment", "role_type"])
                writer.writerow([f"Role{i}", "good", role_type])
                fname = f.name

            roles = parse_roles_csv(Path(fname))
            assert len(roles) == 1
            assert roles[0]["role_type"] == role_type
            Path(fname).unlink()

    def test_description_preserved(self):
        """Description text is preserved as-is."""
        import tempfile
        desc = "Each night*, choose a player: they die. If you die, the game continues"
        with tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False, encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["name", "alignment", "role_type", "description"])
            writer.writerow(["Imp", "evil", "Demon", desc])
            fname = f.name

        roles = parse_roles_csv(Path(fname))
        assert roles[0]["description"] == desc
        Path(fname).unlink()

    def test_translation_name_required_for_translation(self):
        """Translation without name is not added."""
        import tempfile
        csv_content = """\
name,alignment,role_type,ru_name,ru_description
TestRole,good,Townsfolk,,Some description
"""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False, encoding="utf-8") as f:
            f.write(csv_content)
            fname = f.name

        roles = parse_roles_csv(Path(fname))
        assert len(roles) == 1
        assert "translations" not in roles[0]
        Path(fname).unlink()

    def test_multiple_languages(self, tmp_path):
        """Multiple translation languages are parsed."""
        csv_path = tmp_path / "multi_lang.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "name", "alignment", "role_type",
                "ru_name", "ru_description",
                "de_name", "de_description"
            ])
            writer.writerow([
                "Chambermaid", "good", "Townsfolk",
                "Горничная", "Описание",
                "Kammerdiener", "Beschreibung"
            ])

        roles = parse_roles_csv(csv_path)

        assert len(roles) == 1
        role = roles[0]
        assert "ru" in role["translations"]
        assert "de" in role["translations"]
        assert role["translations"]["de"]["name"] == "Kammerdiener"

    def test_strips_whitespace(self):
        """Whitespace is stripped from role fields."""
        import tempfile
        csv_content = """\
name,alignment,role_type,description
 Chambermaid , good , Townsfolk , Simple, but not harmless 
"""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".csv", delete=False, encoding="utf-8") as f:
            f.write(csv_content)
            fname = f.name

        roles = parse_roles_csv(Path(fname))
        assert roles[0]["name"] == "Chambermaid"
        assert roles[0]["alignment"] == "good"
        assert roles[0]["role_type"] == "Townsfolk"
        Path(fname).unlink()

    def test_returns_list_of_dicts(self, valid_roles_csv):
        """Returns a list of dictionaries."""
        roles = parse_roles_csv(valid_roles_csv)

        assert isinstance(roles, list)
        for role in roles:
            assert isinstance(role, dict)
            assert "name" in role
            assert "alignment" in role
            assert "role_type" in role

    def test_translation_keys_sorted(self, tmp_path):
        """Translation keys are sorted by language code."""
        csv_path = tmp_path / "langs.csv"
        with open(csv_path, "w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow([
                "name", "alignment", "role_type",
                "de_name", "de_description",
                "ru_name", "ru_description",
                "fr_name", "fr_description"
            ])
            writer.writerow([
                "Role", "good", "Townsfolk",
                "DE", "de_desc", "RU", "ru_desc", "FR", "fr_desc"
            ])

        roles = parse_roles_csv(csv_path)
        translations = roles[0]["translations"]

        # Keys should be sorted
        keys = list(translations.keys())
        assert keys == sorted(keys)
        assert keys == ["de", "fr", "ru"]
