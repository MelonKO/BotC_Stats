import logging
from datetime import timedelta
from typing import Optional

from asyncpg.exceptions import UniqueViolationError

from app.db import get_connection
from app.models import (
    GameImportRequest,
    RolesImportRequest,
    RoleItem,
    RoleTranslation,
    GameImportStatusResponse,
    GamesImportStatusResponse,
    PlayerItem,
)

_IMPORT_ADVISORY_LOCK_KEY = 7391823
logger = logging.getLogger(__name__)


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


async def _import_single_game_on_conn(conn, data: GameImportRequest) -> GameImportStatusResponse:
    """
    Imports a single game using an already-open connection.
    Must be called inside an advisory lock. Each call runs its own transaction
    so failures are isolated when processing a batch.
    """
    game_label = f"{data.scenario_name} {data.game_date} №{data.game_number}"
    logger.info("Importing game: %s", game_label)
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
            logger.warning("Game %s: missing roles: %s", game_label, sorted(missing_roles))
            return GameImportStatusResponse(
                status="error",
                errors=[f"Unknown roles: {', '.join(sorted(missing_roles))}"],
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
                ", ".join(s.root for s in data.storyteller_names),
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
            logger.debug("Calling process_games_import() for game: %s", game_label)
            row = await conn.fetchrow("SELECT * FROM process_games_import()")
        except UniqueViolationError:
            logger.warning("Game %s: duplicate — already exists in DB", game_label)
            return GameImportStatusResponse(
                status="error",
                errors=[f"Duplicate: '{data.scenario_name}' {data.game_date} #{data.game_number}"],
                players_created=0,
            )
        except Exception:
            logger.exception("process_games_import() failed for game: %s", game_label)
            raise

        players_created = row["players_created"]
        errors = row["errors"]

        if errors:
            logger.warning("Game %s: DB function returned errors: %s", game_label, errors)
            return GameImportStatusResponse(
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

        logger.info("Game %s imported successfully: id=%s, players_created=%d",
                    game_label, game_row["id"] if game_row else None, players_created)
        return GameImportStatusResponse(
            status="ok",
            game_id=game_row["id"] if game_row else None,
            players_created=players_created,
            errors=[],
        )


async def import_game(data: GameImportRequest, owner: dict) -> GameImportStatusResponse:
    """
    Import a single game via the API.

    Uses a PostgreSQL advisory lock for safe concurrent access across processes and workers.
    Each call runs inside its own transaction so staging rows are rolled back on any error.
    """
    game_label = f"{data.scenario_name} {data.game_date} №{data.game_number}"
    async with get_connection() as conn:
        logger.debug("Acquiring advisory lock for game: %s", game_label)
        await conn.execute("SELECT pg_advisory_lock($1)", _IMPORT_ADVISORY_LOCK_KEY)
        logger.debug("Advisory lock acquired for game: %s", game_label)
        try:
            result = await _import_single_game_on_conn(conn, data)
        finally:
            await conn.execute("SELECT pg_advisory_unlock($1)", _IMPORT_ADVISORY_LOCK_KEY)
            logger.debug("Advisory lock released for game: %s", game_label)

    return result


async def import_games_batch(games: list[GameImportRequest], owner: dict) -> GamesImportStatusResponse:
    """
    Import multiple games in a single request.

    Each game is processed independently: a failure in one does not roll back the others.
    All games are processed sequentially under a single advisory lock.
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


async def get_all_roles(lang: Optional[str] = None) -> list[RoleItem]:
    """Return all available roles, optionally with a translation for the given language code."""
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
                RoleItem(
                    id=r["id"],
                    name=r["name"],
                    alignment=r["alignment"],
                    role_type=r["role_type"],
                    description=r["description"],
                    translation=RoleTranslation(
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
                RoleItem(
                    id=r["id"],
                    name=r["name"],
                    alignment=r["alignment"],
                    role_type=r["role_type"],
                    description=r["description"],
                )
                for r in rows
            ]


async def check_db_connection() -> bool:
    """Return True if the database is reachable, False otherwise."""
    try:
        async with get_connection() as conn:
            await conn.fetchval("SELECT 1")
        return True
    except Exception:
        return False


async def import_roles(data: RolesImportRequest) -> dict:
    """
    Upsert a list of roles into the database.

    INSERT ... ON CONFLICT (name) DO UPDATE — existing roles are updated.
    If a role has translations, each language is upserted into role_translations.
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

            except UniqueViolationError:
                errors.append(f"'{role.name}': already exists")
            except Exception as e:
                logger.exception("Role import failed: %s", role.name)
                errors.append(f"'{role.name}': save failed")

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


async def get_all_players() -> list[PlayerItem]:
    async with get_connection() as conn:
        rows = await conn.fetch(
            """
            select id, name
            from players
            order by name
            """
        )
        return [
            PlayerItem(
                id=r["id"],
                name=r["name"],
            )
            for r in rows
        ]
