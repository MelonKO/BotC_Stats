"""
Tests for auth.py — API key validation.
"""
import hashlib
from datetime import datetime
from unittest.mock import AsyncMock

import pytest

import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from app.auth import hash_api_key, validate_api_key
from fastapi import HTTPException
from tests.test_utils import PatchGetConnectionAuth


class TestHashApiKey:
    def test_sha256_hash(self):
        result = hash_api_key("test-key")
        assert result == hashlib.sha256("test-key".encode()).hexdigest()

    def test_deterministic(self):
        key = "sk-my-secret-key"
        assert hash_api_key(key) == hash_api_key(key)

    def test_different_keys_different_hashes(self):
        assert hash_api_key("key-one") != hash_api_key("key-two")

    def test_hex_length(self):
        assert len(hash_api_key("any-key")) == 64

    def test_empty_string(self):
        assert hash_api_key("") == hashlib.sha256(b"").hexdigest()


class TestValidateApiKey:
    @pytest.mark.asyncio
    async def test_valid_key(self):
        mock_conn = AsyncMock()
        mock_conn.fetchrow = AsyncMock(return_value={
            "id": 1, "owner_name": "Alice", "active": True,
            "created_at": datetime(2026, 1, 1), "last_used_at": None,
        })
        mock_conn.execute = AsyncMock(return_value="UPDATE 1")

        with PatchGetConnectionAuth(mock_conn):
            result = await validate_api_key(x_api_key="sk-valid-key")

        assert result["id"] == 1
        assert result["owner_name"] == "Alice"
        call_query = mock_conn.execute.call_args[0][0]
        assert "last_used_at" in call_query

    @pytest.mark.asyncio
    async def test_invalid_key(self):
        mock_conn = AsyncMock()
        mock_conn.fetchrow = AsyncMock(return_value=None)

        with PatchGetConnectionAuth(mock_conn):
            with pytest.raises(HTTPException) as exc_info:
                await validate_api_key(x_api_key="sk-wrong")

        assert exc_info.value.status_code == 401
        assert "Invalid API key" in exc_info.value.detail

    @pytest.mark.asyncio
    async def test_revoked_key(self):
        mock_conn = AsyncMock()
        mock_conn.fetchrow = AsyncMock(return_value={
            "id": 2, "owner_name": "Bob", "active": False,
            "created_at": datetime(2025, 1, 1), "last_used_at": None,
        })

        with PatchGetConnectionAuth(mock_conn):
            with pytest.raises(HTTPException) as exc_info:
                await validate_api_key(x_api_key="sk-revoked")

        assert exc_info.value.status_code == 403
        assert "revoked" in exc_info.value.detail.lower()

    @pytest.mark.asyncio
    async def test_uses_sha256_hash(self):
        key = "sk-my-key"
        expected_hash = hash_api_key(key)

        mock_conn = AsyncMock()
        mock_conn.fetchrow = AsyncMock(return_value=None)

        with PatchGetConnectionAuth(mock_conn):
            try:
                await validate_api_key(x_api_key=key)
            except HTTPException:
                pass

        call_args = mock_conn.fetchrow.call_args
        assert call_args[0][1] == expected_hash

    @pytest.mark.asyncio
    async def test_returns_correct_fields(self):
        mock_conn = AsyncMock()
        mock_conn.fetchrow = AsyncMock(return_value={
            "id": 5, "owner_name": "Test User", "active": True,
            "created_at": "2026-01-01T00:00:00", "last_used_at": "2026-03-01T12:00:00",
        })
        mock_conn.execute = AsyncMock()

        with PatchGetConnectionAuth(mock_conn):
            result = await validate_api_key(x_api_key="sk-test")

        assert set(result.keys()) == {"id", "owner_name", "created_at"}
