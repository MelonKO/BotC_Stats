#!/bin/bash
# ============================================================
#  BotC Stats — apply SQL migrations to the BotC database
# ============================================================
#
#  Runs inside the PostgreSQL image through the opt-in db-migrate compose
#  service:
#
#    docker compose -f docker-compose.release.yml run --rm db-migrate
#
#  Every file from $BOTC_MIGRATIONS_DIR (default /migrations) is applied in
#  lexicographic order and at most once: applied files are recorded in
#  public.schema_migrations, so a re-run only picks up files added later.
#
#  Connection settings come from the standard libpq variables:
#  PGHOST, PGPORT, PGUSER, PGPASSWORD, PGDATABASE.
#
#  Exit codes: 0 — migrations applied or nothing to do, non-zero (psql's own
#  code) — a migration failed or the database is unreachable.
# ============================================================

set -euo pipefail

MIGRATIONS_DIR="${BOTC_MIGRATIONS_DIR:-/migrations}"
DB_NAME="${PGDATABASE:-botc_stats}"
LEDGER="public.schema_migrations"

log() { echo "[migrate] $*"; }

# The ledger is created on first use and lives next to the data tables.
psql -v ON_ERROR_STOP=1 -q --dbname "$DB_NAME" <<'SQL'
CREATE TABLE IF NOT EXISTS public.schema_migrations (
    version    TEXT PRIMARY KEY,
    applied_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
SQL

# Queries are read from stdin on purpose: psql substitutes :'version' only in
# input it parses itself, not in the command string passed with -c.
is_applied() {
    psql -tA -v ON_ERROR_STOP=1 -v version="$1" --dbname "$DB_NAME" <<'SQL'
SELECT 1 FROM public.schema_migrations WHERE version = :'version';
SQL
}

mark_applied() {
    psql -v ON_ERROR_STOP=1 -q -v version="$1" --dbname "$DB_NAME" <<'SQL'
INSERT INTO public.schema_migrations (version) VALUES (:'version')
ON CONFLICT (version) DO NOTHING;
SQL
}

shopt -s nullglob
migrations=("$MIGRATIONS_DIR"/*.sql)

if [ "${#migrations[@]}" -eq 0 ]; then
    log "no migration files in $MIGRATIONS_DIR, nothing to do"
    exit 0
fi

applied=0
skipped=0

for file in "${migrations[@]}"; do
    version="$(basename "$file")"

    if ! recorded="$(is_applied "$version")"; then
        log "cannot read $LEDGER, aborting"
        exit 1
    fi

    if [ "$recorded" = "1" ]; then
        log "skip  $version (already applied)"
        skipped=$((skipped + 1))
        continue
    fi

    log "apply $version"
    # Each migration carries its own transaction; ON_ERROR_STOP makes psql exit
    # non-zero on the first failing statement, so a failure never reaches the
    # ledger and the next run retries the file.
    psql -v ON_ERROR_STOP=1 --dbname "$DB_NAME" -f "$file"

    mark_applied "$version"
    applied=$((applied + 1))
done

log "done: $applied applied, $skipped skipped"
