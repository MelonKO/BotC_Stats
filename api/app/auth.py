import hashlib
from fastapi import HTTPException, Header
from app.db import get_connection


def hash_api_key(key: str) -> str:
    """SHA-256 хеш API-ключа для сравнения с БД."""
    return hashlib.sha256(key.encode()).hexdigest()


async def validate_api_key(x_api_key: str = Header(..., alias="X-API-Key")) -> dict:
    """
    Валидирует API-ключ из заголовка X-API-Key.

    Возвращает информацию о владельце ключа.
    Бросает HTTPException 401/403 при ошибке.
    """
    key_hash = hash_api_key(x_api_key)

    async with get_connection() as conn:
        row = await conn.fetchrow(
            """
            SELECT id, owner_name, active, created_at, last_used_at
            FROM api_keys
            WHERE key_hash = $1
            """,
            key_hash,
        )

        if row is None:
            raise HTTPException(status_code=401, detail="Invalid API key")

        if not row["active"]:
            raise HTTPException(status_code=403, detail="API key has been revoked")

        # Обновляем last_used_at
        await conn.execute(
            """
            UPDATE api_keys SET last_used_at = now() WHERE id = $1
            """,
            row["id"],
        )

    return {
        "id": row["id"],
        "owner_name": row["owner_name"],
        "created_at": row["created_at"],
    }
