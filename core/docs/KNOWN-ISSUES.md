# Known issues and open questions

Working list of defects, contract inconsistencies and unresolved decisions for
the whole monorepo. Items are collected from code audits; each one names the
place in the code so it can be verified, and the decision or next step it needs.

Status values: `open` — confirmed, not fixed; `needs decision` — blocked on a
product/domain choice; `deferred` — consciously postponed; `fixed` — resolved
(keep the entry until the release notes mention it).

## Open questions

### Q1. `alignment_start` for players — `deferred`

The start side of a player is not stored anywhere in the import pipeline: the
CSV (`test_sample.csv`) has no such column, `GamesCsvParser::PlayerRecord`
(`DBConnect/src/utils/games/parser/GamesCsvParser.h:9-16`) has only
`alignmentEnd`, the API payload and `games_import_staging` carry only
`alignment_end`, and `game_players` has only `alignment_end`
(`core/init-scripts/01-schema.sql:123`).

Today the start side exists only implicitly as `game_players.role_start_id ->
roles.alignment`, which is the **role's** side with three values
(`good`, `evil`, `neutral`). It is used for analytics by role
(`v_role_type_stats`, `01-schema.sql:471-477`) and for role-change detection
(`:417`, `:445`), never as a player attribute.

Why the derivation is not equivalent to a stored field:

- a `neutral` role (Traveller) has no side until the storyteller picks one, so
  the value is unrecoverable from the role dictionary;
- the side can change without a role change, so "started good, ended evil" is
  invisible today — only the end side is kept;
- `roles.alignment` is mutable (role import upserts it,
  `core/api/app/import_logic.py:271-279`), so a derived "historical" value would
  change retroactively.

Decision (2026-10-23): do not add the field now. Revisit when start-vs-end
analytics is actually needed. The change would touch, in order: CSV column and
`test_sample.csv`; `PlayerRecord.alignmentStart` plus validation;
`GamePlayerItem` in `openapi/api/openapi.yaml` (then `npm run generate`);
`games_import_staging.alignment_start_ru` and the mapping in
`process_games_import()`; `game_players.alignment_start` with
`CHECK (alignment_start IN ('good', 'evil'))`; the insert; tests. Backfill for
existing rows can only come from roles with `good`/`evil` sides — rows with a
`neutral` start role stay `NULL`, which forces a decision about `NULL` vs
`NOT NULL`.

### Q2. Keep the historical name `alignment` or rename it to `side` — `needs decision`

`alignment` means the side of the conflict, not "выравнивание"
(`core/docs/SIDES.md`). The name is kept in the schema, the API and both
clients. Renaming would touch `01-schema.sql`, the analytical views, the OpenAPI
contract and the generated clients, so it is only worth doing together with
another breaking change to the API.

### Q3. How draws affect win rate — `needs decision`

Views count a win as `alignment_end = alignment_win`
(`01-schema.sql:386-392`, `:410-412`). When the winning side is `draw`, nobody
wins, so every player gets a loss-less "not won" result while the denominator
still grows. Confirm that a draw is meant to count as a non-win for all players,
or exclude such games from win-rate calculations.

### Q4. Travellers in role statistics — `needs decision`

`v_role_type_stats` groups by the role's side, so every Traveller is reported
under `neutral` (`01-schema.sql:471-477`) although the players who ran it played
as `good` or `evil`. Decide whether Traveller statistics should be split by the
actual side (`gp.alignment_end`) or stay reported per role.

## Contract inconsistencies

### I1. `нейтральный` is accepted for a player's end side but rejected by the database — `open`

- `games_import_staging.alignment_end_ru` allows `нейтральный`
  (`core/init-scripts/01-schema.sql:175`) and the mapping comment promises
  `нейтральный -> neutral` (`:231`), while
  `game_players.alignment_end` allows only `good`/`evil` (`:123`).
- The OpenAPI contract and the generated model allow `нейтральный` for
  `alignment_end` (`openapi/api/openapi.yaml`, `core/api/app/models.py:68-70`).
- `DBConnect/src/api/BotCApiClient.cpp:12-13` declares regexes that allow it;
  they are unused dead code, while `GamesCsvParser.cpp:9-11` correctly allows
  only `добро`/`зло`.

Impact: a caller outside the GUI passes validation, then hits a constraint
violation on insert. FastAPI validates the whole batch, so one such player
aborts the entire request with HTTP 500 and an error that does not name the
field. The GUI cannot trigger it today, which is why tests do not catch it.

Next step: restrict the staging check and the mapping to `добро`/`зло`, narrow
the `alignment_end` pattern in `openapi/api/openapi.yaml`, regenerate the
clients, remove the dead constants, and add a test that a neutral end side is
rejected with a clear message.

### I2. Duplicate game is reported as a successful import — `open`

`process_games_import()` skips a game whose key already exists
(`01-schema.sql:252-260`), then truncates staging and returns empty `errors`
(`:333-336`). The API treats that as success and returns the id of the existing
game (`core/api/app/import_logic.py:120-142`), so DBConnect shows "game imported,
players created: 0" (`DBConnect/src/ui/ImportGamesWidget.cpp:384-397`) while the
new data is silently dropped.

### I3. Any database exception other than a duplicate aborts the whole batch — `open`

`core/api/app/import_logic.py:104-106` re-raises, and the batch loop
(`:177-179`) has no per-game handler, so the documented "a failure in one game
does not roll back the others" (`main.py:115`, `import_logic.py:170-171`) is not
true: a check/FK/connection error returns HTTP 500 and the remaining games are
not imported.

### I4. `import_roles` is not transactional — `open`

`core/api/app/import_logic.py:261-311` commits each `execute` separately and
writes translations after the role, so a failure in the middle leaves partially
imported roles while the client is told the import failed. The
`UniqueViolationError` branch (`:306-307`) is practically unreachable because the
statement uses `ON CONFLICT DO UPDATE`.

### I5. Staging table is global and can keep stale rows — `open`

`games_import_staging` is a single shared table; the early `RETURN`s
(`01-schema.sql:220-227`, `:245-250`) happen before `TRUNCATE`, so rows survive a
failed import and are picked up by the next request. Isolating staging per import
(temporary table or an `import_id`) would remove the class of bugs.

### I6. Misleading "Duplicate" error for a repeated player — `open`

`core/api/app/import_logic.py:99-103` maps every unique violation to
`Duplicate: ...`, including `UNIQUE (game_id, player_id)` — that is the same
player listed twice in one game, not a duplicate game.

## DBConnect client

- **TLS verification cannot be re-enabled** — `BotCApiClient::setSslVerify()`
  only disables verification, and the settings checkbox promises a toggle
  (`DBConnect/src/api/BotCApiClient.cpp:58-71`). The connection test ignores the
  setting as well (`SettingsWidget.cpp:89` is commented out). — `open`
- **Config path is relative to the working directory** —
  `DBConnect/src/config/ConfigManager.h:55` opens `config.ini` from the CWD, so
  starting the executable from another folder silently falls back to
  `https://localhost:443`. — `open`
- **Role cache race** — roles and players are fetched asynchronously while the
  cache is read synchronously when a file is opened
  (`ImportGamesWidget.cpp:55-57`, `:79-97`), which produces false "role not in
  database" errors; `DBCache.cpp:30,46` clears the cache without a message.
  `DBCache.h:13-29` declares a signal without `Q_OBJECT`. — `open`
- **Import is enabled despite validation errors** — `ImportGamesWidget.cpp:212-221`
  keeps the button active, and the API validates the batch as a whole, so one
  bad game yields HTTP 422 shown as "network error". — `open`
- **Durations above 24 hours are lost** — `GamesCsvParser.cpp:117` parses with
  `hh:mm:ss`, while the API supports longer games
  (`core/api/app/models.py:101`); the preview shows "-". — `open`
- **CSV parser details** — the error message drops `errorString()`
  (`CsvParser.cpp:18-20`), a trailing empty field is discarded (`:82-85`),
  multi-line quoted fields are not supported (`:38-41`), and `notes` are copied
  to every player of a game (`GamesCsvParser.cpp:119-128`). — `open`
- **Cancel button is inert** in the games import progress dialog
  (`ImportGamesWidget.cpp:107-112`, unlike roles at `:90-96`). — `open`

## Security

- **API key and server IP are still in git history** — the file is untracked now
  and `config.ini.example` is tracked, but rotation of the exposed key was never
  confirmed. — `open`
- **SQL injection in the key-issuing script** —
  `core/scripts/create-api-key.sh:66-67` interpolates `owner_name` and
  `owner_contact` into SQL inside quotes. — `open`
- **`SECURITY DEFINER` without a fixed `search_path`** —
  `01-schema.sql:338`, `99-migrate-storyteller-draw.sql:212`. — `open`
- **Weak defaults and exposed surfaces** — `core/docker-compose.yml:14-18`
  falls back to `change_me` without `.env`; Swagger UI and `/openapi.json` are
  public (`core/nginx/nginx.conf:104-112`). — `open`
- **No import audit** — `validate_api_key` returns the key owner
  (`core/api/app/auth.py:44-48`) but the import functions ignore it, so the
  database does not record who imported a game. — `open`

## Tests and CI

- No tests for the batch import path (`POST /api/games/import_batch`), for
  `get_all_roles(lang)`, for `process_games_import()`, staging or the views; all
  existing tests mock asyncpg (`core/api/app/tests/conftest.py:91-96`). — `open`
- A test asserts behaviour that the database does not have: repeated import is
  expected to raise `UniqueViolationError`
  (`core/api/app/tests/test_import_logic.py:156-171`), while the SQL function
  skips the game and returns empty `errors`. — `open`
- CI builds and tests only `core/api` (`.github/workflows/ci.yml:9-36`);
  DBConnect is never compiled. — `open`
