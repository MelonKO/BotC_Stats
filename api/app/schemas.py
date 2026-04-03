import hashlib
from pydantic import BaseModel, Field
from datetime import date
from typing import Optional


# ============================================================
#  Request schemas
# ============================================================

class PlayerImportRequest(BaseModel):
    """Один игрок в запросе импорта."""
    name: str = Field(..., min_length=1, max_length=200, examples=["Анна Никитина"])
    role_start: str = Field(..., min_length=1, max_length=200, examples=["Дамочка"])
    role_end: str = Field(..., min_length=1, max_length=200, examples=["Дамочка"])
    color_end: str = Field(..., pattern="^(синий|красный)$", examples=["синий"])
    is_alive: bool = Field(..., examples=[True])


class GameImportRequest(BaseModel):
    """Полный запрос импорта партии."""
    game_date: date = Field(..., examples=["2026-01-15"])
    scenario_name: str = Field(..., min_length=1, max_length=300, examples=["Вселенная зла"])
    storyteller_name: str = Field(..., min_length=1, max_length=200, examples=["МелонКО"])
    color_win: str = Field(..., pattern="^(синий|красный)$", examples=["синий"])
    players: list[PlayerImportRequest] = Field(..., min_length=1, max_length=30)


# ============================================================
#  Response schemas
# ============================================================

class ImportStatusResponse(BaseModel):
    status: str
    game_id: Optional[str] = None
    players_created: int = 0
    errors: list[str] = []


class RolesResponse(BaseModel):
    roles: list[dict]


class ApiKeyInfo(BaseModel):
    """Информация о владельце API-ключа (для внутренних нужд)."""
    key_hash: str
    owner_name: str
    active: bool
    created_at: str
    last_used_at: Optional[str] = None


# ============================================================
#  Health check
# ============================================================

class HealthResponse(BaseModel):
    status: str
    db_connected: bool
