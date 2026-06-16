import hashlib
from fastapi import HTTPException, Header
from app.db import get_connection


def hash_api_key(key: str) -> str:
    """Return the SHA-256 hex digest of an API key for comparison with the database."""
    return hashlib.sha256(key.encode()).hexdigest()


async def validate_api_key(x_api_key: str = Header(..., alias="X-API-Key")) -> dict:
    """
    Validate the API key from the X-API-Key header.

    Returns owner info on success.
    Raises HTTPException 401 if the key is unknown, 403 if it has been revoked.
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

        # Update last_used_at timestamp
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
