from datetime import timedelta
from typing import Optional

from asyncpg.exceptions import UniqueViolationError
from pygments.lexers import r

from app.db import get_connection
from app.models import GameImportRequest, RolesImportRequest, Role1, Translation, ImportStatusResponse, Player1

import asyncio

_game_import_lock = asyncio.Lock()


def _parse_interval(value: str | None) -> timedelta | None:
    """Convert 'HH:MM:SS' or 'D HH:MM:SS' string to timedelta."""
    if not value:
        return None
    parts = value.strip().split()
    if len(parts) == 1:
        h, m, s = parts[0].split(":")
        return timedelta(hours=int(h), minutes=int(m), seconds=int(s))
    else:
        d = int(parts[0])
        h, m, s = parts[1].split(":")
        return timedelta(days=d, hours=int(h), minutes=int(m), seconds=int(s))


async def import_game(data: GameImportRequest, owner: dict) -> ImportStatusResponse:
    """
    Импортирует партию через API.

    Процесс:
    1. Валидирует роли (проверяет существование в БД)
    2. Вставляет данные в games_import_staging
    3. Вызывает process_games_import()
    4. Возвращает результат

    Returns:
        dict со статусом, game_id, количеством созданных игроков и ошибками
    """
    async with _game_import_lock:
        async with get_connection() as conn:
            # Шаг 1: Валидация ролей на сервере (через русские переводы)
            role_names = set()
            for p in data.players:
                role_names.add(p.role_start)
                role_names.add(p.role_end)

            existing_roles = await conn.fetch(
                "SELECT name FROM role_translations WHERE lang_code = 'ru' AND name = ANY($1)",
                list(role_names),
            )
            existing_role_names = {r["name"] for r in existing_roles}
            missing_roles = role_names - existing_role_names

            if missing_roles:
                return ImportStatusResponse(
                    status="error",
                    errors=[
                        f"Отсутствуют роли: {', '.join(sorted(missing_roles))}.",
                        f"Обратитесь к администратору для добавления ролей."
                    ],
                    players_created=0)

            # Шаг 2: Вставляем в staging (русские значения)
            for p in data.players:
                await conn.execute(
                    """
                    INSERT INTO games_import_staging (game_date, scenario_name, storyteller_name, alignment_win_ru,
                                                      location, game_number, duration, notes,
                                                      player_name, seat_number,
                                                      role_start_name_ru, role_end_name_ru,
                                                      alignment_end_ru, is_alive)
                    VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14)
                    """,
                    data.game_date,
                    data.scenario_name,
                    data.storyteller_name,
                    data.alignment_win,
                    data.location,
                    data.game_number,
                    _parse_interval(data.duration),
                    data.notes,
                    p.name,
                    p.seat_number,
                    p.role_start,
                    p.role_end,
                    p.alignment_end,
                    p.is_alive,
                )

            # Шаг 3: Вызываем process_games_import()
            try:
                row = await conn.fetchrow("SELECT * FROM process_games_import()")
            except UniqueViolationError:
                return ImportStatusResponse(
                    status="error",
                    errors=[
                        f"Партия {data.scenario_name} ({data.game_date}, №{data.game_number}) ",
                        f"рассказчик {data.storyteller_name} уже существует в базе данных."
                    ],
                    players_created=0
                )

            players_created = row["players_created"]
            errors = row["errors"]

            if errors:
                return ImportStatusResponse(
                    status="error",
                    errors=[errors],
                    players_created=players_created
                )

            # Шаг 4: Получаем ID созданной игры
            game_row = await conn.fetchrow(
                """
                SELECT id::text
                FROM games
                WHERE game_date = $1
                  AND scenario_name = $2
                  AND game_number = $3
                ORDER BY id DESC
                LIMIT 1
                """,
                data.game_date,
                data.scenario_name,
                data.game_number,
            )

            return ImportStatusResponse(
                status="ok",
                game_id=game_row["id"] if game_row else None,
                players_created=players_created,
                errors=[],
            )


async def get_all_roles(lang: Optional[str] = None) -> list[Role1]:
    """Возвращает список всех доступных ролей."""
    async with get_connection() as conn:
        if lang:
            rows = await conn.fetch(
                """
                SELECT r.id,
                       r.name,
                       r.alignment,
                       r.role_type,
                       r.description,
                       t.name        AS translation_name,
                       t.description AS translation_description
                FROM roles r
                         LEFT JOIN role_translations t
                                   ON t.role_id = r.id AND t.lang_code = $1
                ORDER BY r.role_type, r.name
                """,
                lang,
            )
            return [
                Role1(
                    id=r["id"],
                    name=r["name"],
                    alignment=r["alignment"],
                    role_type=r["role_type"],
                    description=r["description"],
                    translation=Translation(
                        name=r["translation_name"],
                        description=r["translation_description"],
                    ) if r["translation_name"] is not None else None,
                )
                for r in rows
            ]
        else:
            rows = await conn.fetch(
                """
                SELECT id, name, alignment, role_type, description
                FROM roles
                ORDER BY role_type, name
                """
            )
            return [
                Role1(
                    id=r["id"],
                    name=r["name"],
                    alignment=r["alignment"],
                    role_type=r["role_type"],
                    description=r["description"],
                )
                for r in rows
            ]


async def check_db_connection() -> bool:
    """Проверяет подключение к БД."""
    try:
        async with get_connection() as conn:
            await conn.fetchval("SELECT 1")
        return True
    except Exception:
        return False


async def import_roles(data: RolesImportRequest) -> dict:
    """
    Импортирует (upsert) список ролей в БД.

    INSERT ... ON CONFLICT (name) DO UPDATE — существующие роли обновляются.
    Если у роли есть translations — upsert в role_translations для каждого языка.

    Returns:
        dict со статусом, количеством созданных/обновлённых ролей и ошибками.
    """
    created = 0
    updated = 0
    errors = []

    async with get_connection() as conn:
        for role in data.roles:
            try:
                existed = await conn.fetchval(
                    "SELECT EXISTS(SELECT 1 FROM roles WHERE name = $1)",
                    role.name,
                )

                await conn.execute(
                    """
                    INSERT INTO roles (name, alignment, role_type, description)
                    VALUES ($1, $2, $3, $4)
                    ON CONFLICT (name) DO UPDATE
                        SET alignment   = EXCLUDED.alignment,
                            role_type   = EXCLUDED.role_type,
                            description = EXCLUDED.description
                    """,
                    role.name,
                    role.alignment,
                    role.role_type,
                    role.description,
                )

                if existed:
                    updated += 1
                else:
                    created += 1

                # Upsert translations (if provided)
                if role.translations:
                    for lang_code, tr in role.translations.items():
                        await conn.execute(
                            """
                            INSERT INTO role_translations (role_id, lang_code, name, description)
                            VALUES ((SELECT id FROM roles WHERE name = $1),
                                    $2, $3, $4)
                            ON CONFLICT (role_id, lang_code) DO UPDATE
                                SET name        = EXCLUDED.name,
                                    description = EXCLUDED.description
                            """,
                            role.name,
                            lang_code,
                            tr.name,
                            tr.description,
                        )

            except Exception as e:
                errors.append(f"Role '{role.name}': {e}")

    if errors:
        return {
            "status": "error",
            "roles_created": created,
            "roles_updated": updated,
            "errors": errors,
        }

    return {
        "status": "ok",
        "roles_created": created,
        "roles_updated": updated,
        "errors": [],
    }


async def get_all_players() -> list[Player1]:
    async with get_connection() as conn:
        rows = await conn.fetch(
            """
            select id, name
            from players
            order by name
            """
        )
        return [
            Player1(
                id=r["id"],
                name=r["name"],
            )
            for r in rows
        ]
