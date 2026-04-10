"""
Tests for load_config() function.

Since load_config() uses Path(__file__).parent / ".env", we test it by
creating a temporary copy of the uploader module in a temp directory
with different .env files.
"""
import importlib.util
import sys
from pathlib import Path

import pytest


def _load_module_from_path(module_name, file_path):
    """Dynamically load a Python module from an arbitrary path."""
    spec = importlib.util.spec_from_file_location(module_name, file_path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def uploader_module():
    """Load the uploader module from its real path."""
    uploader_path = Path(__file__).resolve().parent.parent / "uploader.py"
    return _load_module_from_path("uploader_dynamic", uploader_path)


class TestLoadConfig:
    """Tests for load_config()."""

    def test_valid_env(self, tmp_env_file, uploader_module, monkeypatch):
        """Load config from valid .env file."""
        # Monkey-patch __file__ so load_config finds our temp .env
        original_file = uploader_module.__file__
        # Replace __file__ with a path pointing to tmp_env_file
        fake_file = tmp_env_file / "uploader.py"
        fake_file.touch()
        uploader_module.__file__ = str(fake_file)

        # Also clear any cached env vars from dotenv
        import os
        for key in ("API_URL", "API_KEY", "SSL_VERIFY"):
            if key in os.environ:
                del os.environ[key]

        config = uploader_module.load_config()

        assert config["api_url"] == "https://localhost:443"
        assert config["api_key"] == "sk-test-key-123"
        assert config["ssl_verify"] is False

    def test_missing_env_file(self, tmp_path, uploader_module):
        """SystemExit when .env file doesn существует."""
        fake_file = tmp_path / "uploader.py"
        fake_file.touch()
        uploader_module.__file__ = str(fake_file)

        # Clear cached env vars
        import os
        for key in ("API_URL", "API_KEY", "SSL_VERIFY"):
            if key in os.environ:
                del os.environ[key]

        with pytest.raises(SystemExit) as exc_info:
            uploader_module.load_config()

        assert exc_info.value.code == 1

    def test_missing_api_key(self, tmp_env_no_api_key, uploader_module):
        """SystemExit when API_KEY is not set."""
        fake_file = tmp_env_no_api_key / "uploader.py"
        fake_file.touch()
        uploader_module.__file__ = str(fake_file)

        import os
        for key in ("API_URL", "API_KEY", "SSL_VERIFY"):
            if key in os.environ:
                del os.environ[key]

        with pytest.raises(SystemExit) as exc_info:
            uploader_module.load_config()

        assert exc_info.value.code == 1

    def test_custom_api_url(self, tmp_path, uploader_module):
        """Custom API_URL is loaded correctly."""
        env_content = "API_URL=https://example.com:8443\nAPI_KEY=sk-custom\nSSL_VERIFY=true\n"
        env_path = tmp_path / ".env"
        env_path.write_text(env_content, encoding="utf-8")

        fake_file = tmp_path / "uploader.py"
        fake_file.touch()
        uploader_module.__file__ = str(fake_file)

        import os
        for key in ("API_URL", "API_KEY", "SSL_VERIFY"):
            if key in os.environ:
                del os.environ[key]

        config = uploader_module.load_config()

        assert config["api_url"] == "https://example.com:8443"
        assert config["api_key"] == "sk-custom"
        assert config["ssl_verify"] is True
