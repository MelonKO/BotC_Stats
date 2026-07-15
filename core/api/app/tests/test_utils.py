"""
Shared test utilities for core/api tests.
"""
from contextlib import asynccontextmanager
from unittest.mock import AsyncMock, patch as _patch
import functools


def make_mock_acquire(conn):
    """
    Create a mock for get_connection() that works with `async with`.
    Usage: with patch_get_connection(mock_conn) as mock_gc: ...
    """
    @asynccontextmanager
    async def mock_get_conn():
        yield conn
    return mock_get_conn


def make_mock_transaction():
    """
    Mock for asyncpg's conn.transaction(): it returns an async context
    manager, not a coroutine, so a plain AsyncMock attribute breaks
    `async with conn.transaction():`.
    """
    @asynccontextmanager
    async def mock_transaction():
        yield
    return mock_transaction


class PatchGetConnection:
    """
    Context manager that patches get_connection in multiple modules
    so that `async with get_connection() as conn:` works correctly.

    Usage:
        with PatchGetConnection(mock_conn):
            await import_game(...)
    """
    MODULES = ["app.db", "app.auth", "app.import_logic"]

    def __init__(self, conn):
        self.conn = conn
        self._patches = []

    def __enter__(self):
        for module in self.MODULES:
            acquire_ctx = make_mock_acquire(self.conn)
            p = _patch(f"{module}.get_connection", acquire_ctx)
            p.start()
            self._patches.append(p)
        return self.conn

    def __exit__(self, *args):
        for p in self._patches:
            p.stop()


class PatchGetConnectionImportLogic:
    """Patches get_connection only in app.import_logic."""
    def __init__(self, conn):
        self.conn = conn

    def __enter__(self):
        acquire_ctx = make_mock_acquire(self.conn)
        self._patch = _patch("app.import_logic.get_connection", acquire_ctx)
        self._patch.start()
        return self.conn

    def __exit__(self, *args):
        self._patch.stop()


class PatchGetConnectionAuth:
    """Patches get_connection only in app.auth."""
    def __init__(self, conn):
        self.conn = conn

    def __enter__(self):
        acquire_ctx = make_mock_acquire(self.conn)
        self._patch = _patch("app.auth.get_connection", acquire_ctx)
        self._patch.start()
        return self.conn

    def __exit__(self, *args):
        self._patch.stop()
