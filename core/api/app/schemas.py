import hashlib
from pydantic import BaseModel, Field
from datetime import date
from typing import Optional


# ============================================================
#  Request schemas
# ============================================================

class PlayerImportRequest(BaseModel):
    """Один игрок в запросе импорта."""
    name: str = Field(..., min_length=1, max_length=200, examples=["Мая Вишневская"])
    seat_number: int | None = Field(None, ge=1, examples=[1])
    role_start: str = Field(..., min_length=1, max_length=200, examples=["Дамочка"])
    role_end: str = Field(..., min_length=1, max_length=200, examples=["Дамочка"])
    alignment_end: str = Field(..., pattern="^(добро|зло|нейтральный)$", examples=["добро"])
    is_alive: bool = Field(..., examples=[True])


class GameImportRequest(BaseModel):
    """Полный запрос импорта партии."""
    game_date: date = Field(..., examples=["2026-01-15"])
    scenario_name: str = Field(..., min_length=1, max_length=300, examples=["Вселенная зла"])
    storyteller_name: str = Field(..., min_length=1, max_length=200, examples=["МелонКО"])
    alignment_win: str = Field(..., pattern="^(добро|зло)$", examples=["добро"])
    location: str = Field(..., min_length=1, max_length=300, examples=["Москва, Антикафе на Арбате"])
    game_number: int = Field(..., ge=1, examples=[1])
    duration: str | None = Field(None, examples=["01:30:00", "00:40:00"])
    notes: str | None = Field(None, max_length=2000, examples=["Отличная партия, все получили удовольствие"])
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


# ============================================================
#  Roles import
# ============================================================

class RoleTranslation(BaseModel):
    """Перевод одной роли на один язык."""
    name: str = Field(..., min_length=1, max_length=200, examples=["Дамочка"])
    description: str | None = Field(None, max_length=2000, examples=["Служанка, которая следит за гостями"])


class RoleImportItem(BaseModel):
    """Одна роль в запросе импорта."""
    name: str = Field(..., min_length=1, max_length=200, examples=["Chambermaid"])
    alignment: str = Field(..., pattern="^(good|evil|neutral)$", examples=["good"])
    role_type: str = Field(..., pattern="^(Townsfolk|Outsider|Minion|Demon|Traveller)$", examples=["Outsider"])
    description: str | None = Field(None, max_length=2000, examples=["Simple, but not harmless"])
    translations: dict[str, RoleTranslation] | None = Field(
        None,
        examples=[{"ru": {"name": "Дамочка", "description": "Служанка, которая следит за гостями"}}],
    )


class RolesImportRequest(BaseModel):
    """Полный запрос импорта списка ролей."""
    roles: list[RoleImportItem] = Field(..., min_length=1, max_length=200)


class RolesImportResponse(BaseModel):
    status: str
    roles_created: int = 0
    roles_updated: int = 0
    errors: list[str] = []


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
