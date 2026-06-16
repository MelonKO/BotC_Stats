from datetime import timedelta
from typing import Optional

from asyncpg.exceptions import UniqueViolationError

from app.db import get_connection
from app.models import (
    GameImportRequest,
    Game,
    RolesImportRequest,
    Role1,
    Translation,
    GameImportStatusResponse,
    Game1,
    GamesImportStatusResponse,
    Player2,
)

_IMPORT_ADVISORY_LOCK_KEY = 7391823


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


async def _import_single_game_on_conn(conn, data: GameImportRequest | Game) -> Game1:
    """
    Imports a single game using an already-open connection.
    Must be called inside an advisory lock. Each call runs its own transaction
    so failures are isolated when processing a batch.
    """
    async with conn.transaction():
        # Step 1: Validate roles via Russian translations
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
            return Game1(
                status="error",
                errors=[
                    f"Отсутствуют роли: {', '.join(sorted(missing_roles))}.",
                    f"Обратитесь к администратору для добавления ролей."
                ],
                players_created=0,
            )

        # Step 2: Insert into staging
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

        # Step 3: Call process_games_import()
        try:
            row = await conn.fetchrow("SELECT * FROM process_games_import()")
        except UniqueViolationError:
            return Game1(
                status="error",
                errors=[
                    f"Партия {data.scenario_name} ({data.game_date}, №{data.game_number}) ",
                    f"рассказчик {data.storyteller_name} уже существует в базе данных."
                ],
                players_created=0,
            )

        players_created = row["players_created"]
        errors = row["errors"]

        if errors:
            return Game1(
                status="error",
                errors=[errors],
                players_created=players_created,
            )

        # Step 4: Fetch created game ID
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

        return Game1(
            status="ok",
            game_id=game_row["id"] if game_row else None,
            players_created=players_created,
            errors=[],
        )


async def import_game(data: GameImportRequest, owner: dict) -> GameImportStatusResponse:
    """
    Импортирует одну партию через API.

    Использует PostgreSQL advisory lock для безопасного конкурентного доступа
    (работает между процессами и воркерами). Каждый вызов оборачивается в
    транзакцию, чтобы staging-строки откатывались при любой ошибке.
    """
    async with get_connection() as conn:
        await conn.execute("SELECT pg_advisory_lock($1)", _IMPORT_ADVISORY_LOCK_KEY)
        try:
            result = await _import_single_game_on_conn(conn, data)
        finally:
            await conn.execute("SELECT pg_advisory_unlock($1)", _IMPORT_ADVISORY_LOCK_KEY)

    return GameImportStatusResponse(
        status=result.status,
        game_id=result.game_id,
        players_created=result.players_created,
        errors=result.errors,
    )


async def import_games_batch(games: list[Game], owner: dict) -> GamesImportStatusResponse:
    """
    Импортирует несколько партий за один запрос.

    Каждая партия обрабатывается независимо: ошибка в одной не отменяет остальные.
    Все партии обрабатываются последовательно под одним advisory lock.
    """
    async with get_connection() as conn:
        await conn.execute("SELECT pg_advisory_lock($1)", _IMPORT_ADVISORY_LOCK_KEY)
        try:
            results = []
            for game in games:
                result = await _import_single_game_on_conn(conn, game)
                results.append(result)
        finally:
            await conn.execute("SELECT pg_advisory_unlock($1)", _IMPORT_ADVISORY_LOCK_KEY)

    return GamesImportStatusResponse(games=results)


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


async def get_all_players() -> list[Player2]:
    async with get_connection() as conn:
        rows = await conn.fetch(
            """
            select id, name
            from players
            order by name
            """
        )
        return [
            Player2(
                id=r["id"],
                name=r["name"],
            )
            for r in rows
        ]
