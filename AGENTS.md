# BotC Stats — Agent Instructions

## Monorepo Structure

| Directory | Purpose | Key Files |
|-----------|---------|-----------|
| `core/` | Docker stack: PostgreSQL 16 + FastAPI + Nginx | `docker-compose.yml`, `api/` |
| `uploader/` | CLI tool for CSV→API import | `uploader.py`, `requirements.txt` |

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

### Run uploader (import games)
```bash
cd uploader
python -m venv venv
venv\Scripts\activate  # Windows
# or: source venv/bin/activate  # Linux/macOS
pip install -r requirements.txt

# Copy .env.example and set API_KEY
python uploader.py path/to/games.csv
```

### Run uploader (import roles)
```bash
cd uploader
venv\Scripts\activate  # Windows
# or: source venv/bin/activate  # Linux/macOS
python uploader.py --roles path/to/roles.csv
```

### Run tests
```bash
# Uploader tests
cd uploader && venv\Scripts\activate  # Windows
cd uploader && source venv/bin/activate  # Linux/macOS
pytest tests/ -v

# Core API tests
cd core/api && pytest app/tests/ -v
```

## Critical Conventions

1. **CSV format for games**: `test_sample.csv` has the exact schema. Each row = one player's role in a game. Multiple rows per game are grouped by `(game_date, scenario_name, storyteller_name, alignment_win, location, game_number)`.

2. **Language**: Russian for gameplay data (`alignment_win`, `alignment_end`, role names). English for system data (`name`, `alignment`, `role_type` in roles).

3. **API authentication**: `X-API-Key: sk-<key>` header. Keys stored as SHA-256 hashes in `api_keys` table.

4. **SSL**: Local dev uses self-signed cert. `uploader/.env` defaults `SSL_VERIFY=false`. For production, replace `core/nginx/ssl/` certs.

5. **Database access**: PostgreSQL port 5432 is **not exposed externally**. Admin access only via SSH tunnel: `ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>`.

6. **Versioning**: Single semver for entire monorepo. One tag = guaranteed compatibility across `core/` and `uploader/`.

7. **Test fixtures**: 
   - Uploader tests use temp CSV files (see `uploader/tests/conftest.py`)
   - Core API tests mock asyncpg pool (see `core/api/app/tests/conftest.py`)

## Common Pitfalls

- ❌ Forgetting to run `generate-cert.sh` → Nginx fails to start
- ❌ Not copying `.env` and setting `API_KEY` → uploader exits with "API_KEY not specified"
- ❌ Using `alignment_win: "нейтральный"` → only `"добро"` or `"зло"` allowed
- ❌ Mixing English and Russian in gameplay fields → CSV parser expects Russian for alignment and role names
- ❌ Assuming PostgreSQL is reachable on host port → only SSH tunnel works for admin access

## Project Links

- Core API docs: `core/docs/API-ACCESS.md`
- SSH setup: `core/docs/SSH-ACCESS.md`
- Role CSV format: see `test_roles.csv` + `botc_character_list.csv` schema
