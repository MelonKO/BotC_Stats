"""
Tests for send_import() and send_roles_import() — HTTP API calls.
"""
import sys
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
import requests

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from uploader import send_import, send_roles_import


class TestSendImport:
    """Tests for send_import()."""

    def test_success(self, mock_success_response):
        """Successful import returns parsed JSON."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            result = send_import("https://localhost:443", "sk-key", True, {"game_date": "2026-01-01"})

        assert result["status"] == "ok"
        assert result["game_id"] == 1
        assert result["players_created"] == 3

    def test_correct_url(self, mock_success_response):
        """URL is correctly constructed with trailing slash handling."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443/", "sk-key", True, {})

        mock_post.assert_called_once()
        call_args = mock_post.call_args
        assert call_args[1] is None or call_args[0][0] == "https://localhost:443/api/import"

    def test_url_without_trailing_slash(self, mock_success_response):
        """URL without trailing slash works correctly."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443", "sk-key", True, {})

        call_url = mock_post.call_args[0][0]
        assert call_url == "https://localhost:443/api/import"

    def test_url_with_trailing_slash(self, mock_success_response):
        """URL with trailing slash has duplicate removed."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443/", "sk-key", True, {})

        call_url = mock_post.call_args[0][0]
        assert call_url == "https://localhost:443/api/import"

    def test_correct_headers(self, mock_success_response):
        """Correct headers are sent."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443", "sk-my-key", True, {})

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["headers"]["X-API-Key"] == "sk-my-key"
        assert call_kwargs["headers"]["Content-Type"] == "application/json"

    def test_correct_payload(self, mock_success_response):
        """Game data is sent as JSON."""
        game_data = {"game_date": "2026-01-01", "scenario_name": "Тест", "players": []}
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443", "sk-key", True, game_data)

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["json"] == game_data

    def test_ssl_verify_false(self, mock_success_response):
        """ssl_verify=False is passed to requests."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443", "sk-key", False, {})

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["verify"] is False

    def test_ssl_verify_true(self, mock_success_response):
        """ssl_verify=True is passed to requests."""
        with patch("uploader.requests.post", return_value=mock_success_response) as mock_post:
            send_import("https://localhost:443", "sk-key", True, {})

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["verify"] is True

    def test_timeout(self):
        """Timeout returns error dict."""
        with patch("uploader.requests.post", side_effect=requests.exceptions.Timeout):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert any("время ожидания" in e.lower() for e in result["errors"])

    def test_connection_error(self):
        """ConnectionError returns error dict with helpful message."""
        with patch("uploader.requests.post", side_effect=requests.exceptions.ConnectionError):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert any("подключиться" in e.lower() for e in result["errors"])

    def test_http_error_with_json_detail(self):
        """HTTP error with JSON detail extracts errors."""
        mock_resp = MagicMock()
        mock_resp.status_code = 400
        mock_resp.json.return_value = {
            "detail": {
                "status": "error",
                "errors": ["Role 'X' not found"],
            }
        }
        mock_resp.raise_for_status.side_effect = requests.exceptions.HTTPError("400 Error")

        with patch("uploader.requests.post", return_value=mock_resp):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert "Role 'X' not found" in result["errors"]

    def test_http_error_with_dict_detail(self):
        """HTTP error with dict detail (non-errors key)."""
        mock_resp = MagicMock()
        mock_resp.status_code = 400
        mock_resp.json.return_value = {
            "detail": {"status": "error", "message": "Bad request"}
        }
        mock_resp.raise_for_status.side_effect = requests.exceptions.HTTPError("400 Error")

        with patch("uploader.requests.post", return_value=mock_resp):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert len(result["errors"]) == 1

    def test_http_error_with_string_detail(self):
        """HTTP error with string detail wraps it in list."""
        mock_resp = MagicMock()
        mock_resp.status_code = 400
        mock_resp.json.return_value = {"detail": "Simple error"}
        mock_resp.raise_for_status.side_effect = requests.exceptions.HTTPError("400 Error")

        with patch("uploader.requests.post", return_value=mock_resp):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert result["errors"] == ["Simple error"]

    def test_http_error_with_invalid_json(self):
        """HTTP error with invalid JSON falls back to string message."""
        mock_resp = MagicMock()
        mock_resp.status_code = 400
        mock_resp.json.side_effect = ValueError("Not JSON")
        mock_resp.raise_for_status.side_effect = requests.exceptions.HTTPError("400 Error")

        with patch("uploader.requests.post", return_value=mock_resp):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert len(result["errors"]) == 1

    def test_general_request_exception(self):
        """General RequestException returns error dict."""
        with patch(
            "uploader.requests.post",
            side_effect=requests.exceptions.RequestException("Unknown error")
        ):
            result = send_import("https://localhost:443", "sk-key", True, {})

        assert result["status"] == "error"
        assert any("Unknown error" in e for e in result["errors"])


class TestSendRolesImport:
    """Tests for send_roles_import()."""

    def test_success(self, mock_roles_success_response):
        """Successful roles import returns parsed JSON."""
        with patch("uploader.requests.post", return_value=mock_roles_success_response) as mock_post:
            result = send_roles_import("https://localhost:443", "sk-key", True, [
                {"name": "Chambermaid", "alignment": "good", "role_type": "Townsfolk"}
            ])

        assert result["status"] == "ok"
        assert result["roles_created"] == 2
        assert result["roles_updated"] == 1

    def test_correct_url(self, mock_roles_success_response):
        """URL is correctly constructed."""
        with patch("uploader.requests.post", return_value=mock_roles_success_response) as mock_post:
            send_roles_import("https://localhost:443", "sk-key", True, [])

        call_url = mock_post.call_args[0][0]
        assert call_url == "https://localhost:443/api/roles/import"

    def test_correct_payload(self, mock_roles_success_response):
        """Roles are sent wrapped in {'roles': ...}."""
        roles = [
            {"name": "Imp", "alignment": "evil", "role_type": "Demon"},
            {"name": "Cook", "alignment": "good", "role_type": "Townsfolk"},
        ]
        with patch("uploader.requests.post", return_value=mock_roles_success_response) as mock_post:
            send_roles_import("https://localhost:443", "sk-key", True, roles)

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["json"] == {"roles": roles}

    def test_correct_headers(self, mock_roles_success_response):
        """Correct headers are sent."""
        with patch("uploader.requests.post", return_value=mock_roles_success_response) as mock_post:
            send_roles_import("https://localhost:443", "sk-my-key", True, [])

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["headers"]["X-API-Key"] == "sk-my-key"
        assert call_kwargs["headers"]["Content-Type"] == "application/json"

    def test_timeout(self):
        """Timeout returns error dict."""
        with patch("uploader.requests.post", side_effect=requests.exceptions.Timeout):
            result = send_roles_import("https://localhost:443", "sk-key", True, [])

        assert result["status"] == "error"
        assert any("время ожидания" in e.lower() for e in result["errors"])

    def test_connection_error(self):
        """ConnectionError returns error dict."""
        with patch("uploader.requests.post", side_effect=requests.exceptions.ConnectionError):
            result = send_roles_import("https://localhost:443", "sk-key", True, [])

        assert result["status"] == "error"
        assert any("подключиться" in e.lower() for e in result["errors"])

    def test_http_error_with_json_detail(self):
        """HTTP error with JSON detail extracts errors."""
        mock_resp = MagicMock()
        mock_resp.status_code = 400
        mock_resp.json.return_value = {
            "detail": {
                "status": "error",
                "errors": ["Invalid alignment for role 'X'"],
            }
        }
        mock_resp.raise_for_status.side_effect = requests.exceptions.HTTPError("400 Error")

        with patch("uploader.requests.post", return_value=mock_resp):
            result = send_roles_import("https://localhost:443", "sk-key", True, [])

        assert result["status"] == "error"
        assert "Invalid alignment for role 'X'" in result["errors"]

    def test_general_request_exception(self):
        """General RequestException returns error dict."""
        with patch(
            "uploader.requests.post",
            side_effect=requests.exceptions.RequestException("Network issue")
        ):
            result = send_roles_import("https://localhost:443", "sk-key", True, [])

        assert result["status"] == "error"
        assert any("Network issue" in e for e in result["errors"])

    def test_empty_roles_list(self, mock_roles_success_response):
        """Empty roles list is sent correctly."""
        with patch("uploader.requests.post", return_value=mock_roles_success_response) as mock_post:
            send_roles_import("https://localhost:443", "sk-key", True, [])

        call_kwargs = mock_post.call_args[1]
        assert call_kwargs["json"] == {"roles": []}
