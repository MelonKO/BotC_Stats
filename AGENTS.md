# BotC Stats — Agent Instructions

## Monorepo Structure

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| `core/` | Docker stack: PostgreSQL 16 + FastAPI + Nginx + Metabase | `docker-compose.yml`, `api/` |
| `DBConnect/` | Qt6 GUI: CSV→API import (games and roles; players are created by game imports), the players list and player renaming | `src/main.cpp`, `CMakeLists.txt`, `config.ini.example` |
| `openapi/` | API contract and client generation | `api/openapi.yaml`, `generate.py`, `sync.py` |
| `scripts/` | Repository-wide task runner | `project.py`, `project.ini.example` |

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

### Routine tasks (`scripts/project.py`)

`scripts/project.py` is the single entry point for the everyday work. It regenerates the API clients from the spec, copies them into the live code, builds DBConnect and runs the tests:

```powershell
python scripts/project.py doctor              # tools, npm state and whether the live code is fresh
python scripts/project.py setup               # npm install + generator requirements
python scripts/project.py generate            # spec → Pydantic models + C++ client → live code
python scripts/project.py generate --check    # fail if the live code does not match the spec
python scripts/project.py sync [--dry-run]    # copy the generated files only
python scripts/project.py build [options]     # forwarded to DBConnect/scripts/build.py
python scripts/project.py deploy [options]    # Release + build_release/dist/
python scripts/project.py test [-k EXPR]      # core API pytest suite
python scripts/project.py verify              # generate --check, then test
```

Everything after `build` or `deploy` is forwarded to `DBConnect/scripts/build.py`, so the CMake/vcpkg logic has a single implementation. Tool paths can be pinned in `scripts/project.ini` (git-ignored, template `project.ini.example`); precedence is command line > config file > environment (`BOTC_PYTHON`, `BOTC_NPM`) > auto-detection.

### Build DBConnect (Qt6 GUI importer)

Prerequisites: CMake 3.25+, Ninja, Visual Studio C++ build tools, and vcpkg. Set `VCPKG_ROOT` to the vcpkg checkout path (for example, `C:\vcpkg`). On Windows, use a **Developer PowerShell for Visual Studio** so `cl.exe` is available to Ninja.

`DBConnect/scripts/build.py` prepares everything itself (imports the Visual Studio environment, forces `VCPKG_ROOT` to the standalone vcpkg checkout, keeps a single CMake) and is the recommended way to build outside an IDE:

```powershell
python DBConnect/scripts/build.py doctor              # toolchain, vcpkg and ABI diagnostics
python DBConnect/scripts/build.py                     # configure + build Debug
python DBConnect/scripts/build.py build --preset release
python DBConnect/scripts/build.py deploy              # Release + deploy (build_release/dist/)
python DBConnect/scripts/build.py clean --preset debug  # remove build_debug, keeps vcpkg_installed
```

Options: `--run` starts the built executable, `--jobs N` limits parallelism, `--dry-run` prints the CMake command, `--cmake PATH` and `--vcpkg-root PATH` override auto-detection.

Machine-specific tool paths live in `DBConnect/scripts/build.ini` (git-ignored, created from the tracked `build.ini.example`): `vs_install`, `cmake`, `vcpkg_root`, `ninja` in `[toolchain]`, plus `preset` and `jobs` in `[build]`. Every value is optional and the precedence is command line > config file > environment (`VCPKG_ROOT`) > auto-detection; `build.py doctor` prints where each tool came from. `build.py --help` lists all options.

Same steps by hand, from a Developer PowerShell:

```powershell
cd DBConnect
cmake --list-presets
cmake --preset debug                    # configure
cmake --build --preset build-debug      # build

# Or configure + build in one command:
cmake --workflow --preset debug

# Windows deploy (exe + Qt DLLs → build_release/dist/):
cmake --workflow --preset deploy
```

The shared presets are defined in `DBConnect/CMakePresets.json`. Local paths such as `VCPKG_ROOT` belong in the machine environment, not in the repository; open a new terminal after changing environment variables.

**Keep the toolchain stable.** vcpkg hashes the compiler, the triplet and the CMake version it builds ports with into every port ABI. Updating CMake (or alternating between the CMake from Visual Studio, MSYS2 and CLion) marks all packages as stale and recompiles Qt from source. Never delete `vcpkg_installed` to "clean" a build: use `build.py clean`, which removes only `build_debug`/`build_release`, and let an interrupted vcpkg install finish.

Open the built executable. Configure API URL and key on the **Settings** tab, then import via **Import Games** or **Import Roles** tabs.

### Run tests
```bash
# Via the project task runner (recommended)
python scripts/project.py test

# Core API tests directly
cd core/api && venv\Scripts\activate  # Windows
cd core/api && source venv/bin/activate  # Linux/macOS
python -m pytest app/tests/ -v
```

## Critical Conventions

1. **CSV format for games**: `test_sample.csv` has the exact schema. Each row = one player's role in a game. Multiple rows per game are grouped by `(game_date, scenario_name, storyteller_name, alignment_win, location, game_number)`.

2. **Language**: Russian for gameplay data (`alignment_win`, `alignment_end`, role names). English for system data (`name`, `alignment`, `role_type` in roles). **All code text must be English: comments, docstrings, and `Field(description=...)` values in models.** `models.py` and `DBConnect/generated_api/` are auto-generated — fix issues in the OpenAPI spec (`openapi/api/openapi.yaml`) and run `python scripts/project.py generate` (bundles the spec, regenerates both clients, and syncs them into the live code). `python scripts/project.py verify` checks that the committed generated code matches the spec.

3. **API authentication**: `X-API-Key: sk-<key>` header. Keys stored as SHA-256 hashes in `api_keys` table.

4. **SSL**: Local dev uses self-signed cert. Local `DBConnect/config.ini` (created from the tracked `config.ini.example`) has `SSL_VERIFY = false` — required for self-signed cert. For production, replace `core/nginx/ssl/` certs.

5. **Database access**: PostgreSQL port 5432 is **not exposed externally**. Admin access only via SSH tunnel: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>`.

6. **Versioning**: Single semver for entire monorepo. One tag = guaranteed compatibility across `core/` and `DBConnect/`.

7. **Test fixtures**: 
   - Core API tests mock asyncpg pool (see `core/api/app/tests/conftest.py`)

8. **Git workflow**: Work by git-flow. All commit messages in English. Do not push commits or other changes to a remote until the work is complete and verified, you have reported what changed and how it was checked, and the user has had an opportunity to review it and explicitly approved the push. Never push automatically as part of implementation or after committing locally.

9. **Code style**: Comments/documentation in English only.

10. **Database permissions**: When adding new tables, always check `api_service` (and `botc_user`) permissions. Define exact operations needed (SELECT, INSERT, UPDATE, DELETE), not just read access. Example: `role_translations` needs INSERT+UPDATE for upsert via API, not just SELECT.

11. **Sides of the conflict**: `alignment` in the schema means the *side* (сторона), not "выравнивание". A role's side allows `good|evil|neutral` (neutral = decided by the storyteller, e.g. Traveller), the winning side allows `good|evil|draw`, and a player's end side allows only `good|evil`. Read `core/docs/SIDES.md` before adding or changing an alignment field.

## Common Pitfalls

- ❌ Committing `DBConnect/config.ini` → it holds a real API key; the file is git-ignored, commit only `config.ini.example`
- ❌ Forgetting to run `generate-cert.sh` → Nginx fails to start
- ❌ `SSL_VERIFY = false` not set in local `DBConnect/config.ini` → DBConnect fails TLS verification against self-signed cert
- ❌ Using `alignment_win: "нейтральный"` → only `"добро"`, `"зло"` or `"ничья"` allowed
- ❌ Sending `alignment_end: "нейтральный"` → a player's end side is only `"добро"`/`"зло"`; `neutral` is a role side (Traveller), decided by the storyteller
- ❌ Mixing English and Russian in gameplay fields → CSV parser expects Russian for alignment and role names
- ❌ Assuming PostgreSQL is reachable on host port → only SSH tunnel works for admin access

## Project Links

- Core API docs: `core/docs/API-ACCESS.md`
- SSH setup: `core/docs/SSH-ACCESS.md`
- Sides of the conflict (`alignment` fields): `core/docs/SIDES.md`
- Known issues and open questions: `core/docs/KNOWN-ISSUES.md`
- Role CSV format: see `test_roles.csv` + `botc_character_list.csv` schema

## Database Schema Overview

### Tables

| Table | Purpose |
|-------|---------|
| `players` | Player registry (unique name, contacts) |
| `roles` | Role dictionary (name, alignment, type in English) |
| `games` | Game metadata (date, scenario, winning alignment, location, game_number) |
| `game_storytellers` | Game-storyteller link (a game may have several storytellers) |
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
| `api_service` | Scoped import permissions on staging, roles/translations, players and game tables; SELECT on analytics views | API import service |

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
| `Invalid API key` | Check the key on the Settings tab (stored in local `config.ini`), create new one |
| SSL error in DBConnect | Set `SSL_VERIFY = false` in local `DBConnect/config.ini` |
| Missing roles | Add roles to `roles` table before import |
| PostgreSQL port 5432 not accessible | Use SSH tunnel only: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>` |
