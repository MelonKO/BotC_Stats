# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Blood on the Clocktower (BotC) Stats — a monorepo for tracking board game session statistics. Components: PostgreSQL 16 database, FastAPI REST API, Nginx reverse proxy (all in Docker), and a Python CLI importer.

## Commands

### Core stack (Docker)
```bash
cd core
cp .env.example .env
bash scripts/generate-cert.sh       # Required before first start — Nginx fails without cert
docker-compose up -d
docker-compose logs -f
```

### API key management
```bash
cd core
bash scripts/create-api-key.sh create "Your Name" "email@example.com"
# Output: sk-<32-hex-chars> — store this; only shown once
```

### Uploader (Python CLI)
```bash
cd uploader
python -m venv venv
venv\Scripts\activate               # Windows
# source venv/bin/activate          # Linux/macOS
pip install -r requirements.txt
cp .env.example .env                # Set API_KEY and API_URL

python uploader.py path/to/games.csv          # Import game records
python uploader.py --roles path/to/roles.csv  # Import role definitions
```

### Tests
```bash
# Uploader
cd uploader && venv\Scripts\activate
python -m pytest tests/ -v

# Core API
cd core/api && venv\Scripts\activate
python -m pytest app/tests/ -v
```

### OpenAPI code generation
```bash
cd openapi
npm run bundle             # Compile openapi.yaml → dist/
npm run generate:python    # Regenerate Python models → python_gen/
npm run generate:cpp       # Regenerate C++ client → cpp_gen/
```

## Architecture

```
CSV files → uploader.py → POST /api/games/import or /api/roles/import
                                    ↓
                            FastAPI (core/api/)
                                    ↓
                           asyncpg → PostgreSQL 16
                                    ↑
                           Nginx (SSL termination, port 443)
```

**`core/`** — Docker stack. `docker-compose.yml` defines three services: `db` (PostgreSQL), `api` (FastAPI/Uvicorn), `nginx` (reverse proxy). Init scripts in `core/init-scripts/` run once on first `docker-compose up`: schema creation, user creation, privilege grants.

**`core/api/app/`** — FastAPI application:
- `main.py` — routes and app factory
- `models.py` — Pydantic v2 schemas (auto-generated from OpenAPI spec; regenerate via `npm run generate:python`)
- `auth.py` — `X-API-Key` validation; keys stored as SHA-256 hashes in `api_keys` table
- `db.py` — asyncpg connection pool; use the `get_db()` async context manager
- `import_logic.py` — business logic for game/role imports

**`uploader/uploader.py`** — argparse CLI. Reads CSV, groups rows into game records, calls the REST API. `SSL_VERIFY=false` is default in `.env.example` for self-signed cert.

**`openapi/api/openapi.yaml`** — source of truth for the API contract. Edit this first, then regenerate `models.py` and the C++ client.

## Critical Conventions

**Language split — this is the most common source of bugs:**
- Gameplay data in CSVs uses **Russian**: `alignment_win`/`alignment_end` values are `добро`/`зло`; role names are Russian.
- System/schema data uses **English**: `alignment` column in `roles` table is `good`/`evil`; `role_type` is `Townsfolk`, `Outsider`, `Minion`, `Demon`, `Traveller`.
- Code and comments: English only.

**Database access:**
- Port 5432 is **not exposed** outside Docker. The only admin path is SSH tunnel: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>`.
- When adding tables, grant explicit permissions to both `botc_user` (CRUD) and `api_service` (only what the API needs). Upsert operations need both INSERT and UPDATE — don't just grant SELECT.

**Versioning:** Single semver tag for the entire monorepo. Bump PATCH/MINOR/MAJOR applies to all components simultaneously.

**Test fixtures:**
- Uploader tests: temp CSV files via `conftest.py`
- Core API tests: mocked asyncpg pool via `conftest.py` — tests do not hit a real database

**Git workflow:** git-flow; all commit messages in English.

## Database Schema Quick Reference

Key tables: `players`, `roles`, `games`, `game_players`, `api_keys`, `games_import_staging`  
Localization: `languages`, `role_translations`, `role_type_translations`, `alignment_translations`  
Analytical views: `v_player_stats`, `v_role_stats`, `v_game_summary`, `v_role_type_stats`

See `core/init-scripts/01-schema.sql` for full schema and `core/docs/API-ACCESS.md` for endpoint documentation.
