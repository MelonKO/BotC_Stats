"""
Tests for validate_csv_file() function.
"""
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from uploader import validate_csv_file


class TestValidateCsvFile:
    """Tests for validate_csv_file()."""

    def test_valid_csv_file(self, tmp_path):
        """Returns Path for valid .csv file."""
        csv_path = tmp_path / "test.csv"
        csv_path.touch()

        result = validate_csv_file(str(csv_path))

        assert result == csv_path

    def test_nonexistent_file(self, tmp_path):
        """SystemExit for non-existent file."""
        with pytest.raises(SystemExit) as exc_info:
            validate_csv_file(str(tmp_path / "nonexistent.csv"))

        assert exc_info.value.code == 1

    def test_non_csv_extension(self, tmp_path):
        """SystemExit for non-.csv extension."""
        txt_file = tmp_path / "test.txt"
        txt_file.touch()

        with pytest.raises(SystemExit) as exc_info:
            validate_csv_file(str(txt_file))

        assert exc_info.value.code == 1

    def test_uppercase_csv_extension(self, tmp_path):
        """Accepts .CSV extension (case-insensitive)."""
        csv_path = tmp_path / "test.CSV"
        csv_path.touch()

        result = validate_csv_file(str(csv_path))

        assert result == csv_path

    def test_mixed_case_extension(self, tmp_path):
        """Accepts .Csv extension (case-insensitive)."""
        csv_path = tmp_path / "test.Csv"
        csv_path.touch()

        result = validate_csv_file(str(csv_path))

        assert result == csv_path

    def test_returns_path_object(self, tmp_path):
        """Returns a Path object, not a string."""
        csv_path = tmp_path / "test.csv"
        csv_path.touch()

        result = validate_csv_file(str(csv_path))

        assert isinstance(result, Path)

    def test_nested_path(self, tmp_path):
        """Works with nested directory paths."""
        subdir = tmp_path / "subdir"
        subdir.mkdir()
        csv_path = subdir / "data.csv"
        csv_path.touch()

        result = validate_csv_file(str(csv_path))

        assert result == csv_path
