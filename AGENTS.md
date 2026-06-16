# BotC Stats — Agent Instructions

## Monorepo Structure

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| `core/` | Docker stack: PostgreSQL 16 + FastAPI + Nginx | `docker-compose.yml`, `api/` |
| `DBConnect/` | Qt6 GUI for CSV→API import (games, roles, players) | `src/main.cpp`, `CMakeLists.txt`, `config.ini` |
| `uploader/` | *(legacy)* Python CLI importer — superseded by DBConnect | `uploader.py`, `requirements.txt` |

## Developer Commands

### Start local environment (core)
```bash
cd core
cp .env.example .env
bash scripts/generate-cert.sh
docker-compose up -d
```

### Create API key (core)
```bash
cd core
bash scripts/create-api-key.sh create "Your Name" "contact@example.com"
# Output contains: sk-<32-hex-chars> — save it securely
```

### Build DBConnect (Qt6 GUI importer)
```bash
cd DBConnect
cmake --preset debug          # configure
cmake --build build_debug     # build

# Windows deploy (exe + Qt DLLs → dist/):
cmake --preset release && cmake --build build_release --target deploy
```

Open the built executable. Configure API URL and key on the **Settings** tab, then import via **Import Games** or **Import Roles** tabs.

### Run tests
```bash
# Core API tests
cd core/api && venv\Scripts\activate  # Windows
cd core/api && source venv/bin/activate  # Linux/macOS
python -m pytest app/tests/ -v
```

## Critical Conventions

1. **CSV format for games**: `test_sample.csv` has the exact schema. Each row = one player's role in a game. Multiple rows per game are grouped by `(game_date, scenario_name, storyteller_name, alignment_win, location, game_number)`.

2. **Language**: Russian for gameplay data (`alignment_win`, `alignment_end`, role names). English for system data (`name`, `alignment`, `role_type` in roles).

3. **API authentication**: `X-API-Key: sk-<key>` header. Keys stored as SHA-256 hashes in `api_keys` table.

4. **SSL**: Local dev uses self-signed cert. `DBConnect/config.ini` has `SSL_VERIFY = false` — required for self-signed cert. For production, replace `core/nginx/ssl/` certs.

5. **Database access**: PostgreSQL port 5432 is **not exposed externally**. Admin access only via SSH tunnel: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>`.

6. **Versioning**: Single semver for entire monorepo. One tag = guaranteed compatibility across `core/` and `DBConnect/`.

7. **Test fixtures**: 
   - Uploader: temp CSV files in `uploader/tests/test_*.py` (see `conftest.py`)
   - Core API: mocked asyncpg pool in `core/api/app/tests/test_*.py` (see `conftest.py`)

## Common Pitfalls

- ❌ Forgetting to run `generate-cert.sh` → Nginx fails to start
- ❌ `SSL_VERIFY = false` not set in `DBConnect/config.ini` → DBConnect fails TLS verification against self-signed cert
- ❌ Using `alignment_win: "нейтральный"` → only `"добро"` or `"зло"` allowed
- ❌ Mixing English and Russian in gameplay fields → CSV parser expects Russian for alignment and role names
- ❌ Assuming PostgreSQL is reachable on host port → only SSH tunnel works for admin access

## Project Links

- Core API docs: `core/docs/API-ACCESS.md`
- SSH setup: `core/docs/SSH-ACCESS.md`
- Role CSV format: see `test_roles.csv` + `botc_character_list.csv` schema

## Database Schema Overview

### Tables

| Table | Purpose |
|-------|---------|
| `players` | Player registry (unique name, contacts) |
| `roles` | Role dictionary (name, alignment, type in English) |
| `games` | Game metadata (date, scenario, storyteller, winning alignment, location, game_number) |
| `game_players` | Game setup (player-role link, alignment, survival) |
| `api_keys` | API key hashes for authentication (SHA-256) |
| `games_import_staging` | Temporary table for CSV import (Russian → English values) |
| `languages` | Supported languages (en, ru) |
| `role_translations` | Role name translations |
| `role_type_translations` | Role type translations |
| `alignment_translations` | Alignment translations |

### Analytical Views

| View | Data |
|------|------|
| `v_player_stats` | Player stats: games, wins, winrate, survival |
| `v_role_stats` | Role efficiency: winrate, survival, role change frequency |
| `v_game_summary` | Game summary: roster, winner, survivors count |
| `v_role_type_stats` | Aggregated stats by role types |

### Database Users

| User | Permissions | Purpose |
|------|-------------|---------|
| `postgres` | Superuser | Administration |
| `botc_user` | CRUD (SELECT, INSERT, UPDATE, DELETE) | Application |
| `api_service` | INSERT on games/players/roles/staging, SELECT on views | API import service |

## SQL Analytics Examples

```sql
-- Top 5 players by winrate (min 5 games)
SELECT name, games_played, win_rate_pct
FROM v_player_stats
WHERE games_played >= 5
ORDER BY win_rate_pct DESC
LIMIT 5;

-- Top 3 most common roles by winrate
SELECT role_name, times_played, win_rate_pct
FROM v_role_stats
WHERE times_played >= 3
ORDER BY win_rate_pct DESC
LIMIT 3;

-- All games in the last month
SELECT game_date, scenario_name, storyteller, alignment_win, location, game_number
FROM v_game_summary
WHERE game_date >= CURRENT_DATE - INTERVAL '1 month'
ORDER BY game_date DESC;

-- Roles most often changed during games
SELECT role_name, role_changed_count, times_played,
       ROUND(100.0 * role_changed_count / times_played, 1) AS change_rate_pct
FROM v_role_stats
WHERE times_played >= 5
ORDER BY change_rate_pct DESC;
```

## Backup & Recovery

```bash
# Create backup
docker-compose exec db pg_dump -U postgres botc_stats > backup.sql

# Restore
docker-compose exec db psql -U postgres botc_stats < backup.sql
```

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Container not starting | `docker-compose logs db` → check errors |
| Database not initializing | `docker-compose down -v && docker-compose up -d` |
| API connection error | Check `docker-compose ps`, `api` container status |
| `Invalid API key` | Check `config.ini` key, create new one |
| SSL error in DBConnect | Set `SSL_VERIFY = false` in `DBConnect/config.ini` |
| Missing roles | Add roles to `roles` table before import |
| PostgreSQL port 5432 not accessible | Use SSH tunnel only: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>` |

## Critical Conventions

1. **CSV format for games**: `test_sample.csv` has the exact schema. Each row = one player's role in a game. Multiple rows per game are grouped by `(game_date, scenario_name, storyteller_name, alignment_win, location, game_number)`.

2. **Language**: Russian for gameplay data (`alignment_win`, `alignment_end`, role names). English for system data (`name`, `alignment`, `role_type` in roles).

3. **API authentication**: `X-API-Key: sk-<key>` header. Keys stored as SHA-256 hashes in `api_keys` table.

4. **SSL**: Local dev uses self-signed cert. `DBConnect/config.ini` has `SSL_VERIFY = false` — required for self-signed cert. For production, replace `core/nginx/ssl/` certs.

5. **Database access**: PostgreSQL port 5432 is **not exposed externally**. Admin access only via SSH tunnel: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>`.

6. **Versioning**: Single semver for entire monorepo. One tag = guaranteed compatibility across `core/` and `DBConnect/`.

7. **Test fixtures**: 
   - Core API tests mock asyncpg pool (see `core/api/app/tests/conftest.py`)

8. **Git workflow**: Work by git-flow. All commit messages in English.

9. **Code style**: Comments/documentation in English only.

10. **Database permissions**: When adding new tables, always check `api_service` (and `botc_user`) permissions. Define exact operations needed (SELECT, INSERT, UPDATE, DELETE), not just read access. Example: `role_translations` needs INSERT+UPDATE for upsert via API, not just SELECT.
