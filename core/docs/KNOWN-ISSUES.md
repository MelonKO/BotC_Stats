# Known issues and open questions

Working list of defects, contract inconsistencies and unresolved decisions for
the whole monorepo. Items are collected from code audits; each one names the
place in the code so it can be verified, and the decision or next step it needs.

Status values: `open` — confirmed, not fixed; `needs decision` — blocked on a
product/domain choice; `deferred` — consciously postponed; `fixed` — resolved
(keep the entry until the release notes mention it).

Items `I2`, `I3`, `I4`, `C1`, `C4` and `T1`–`T2` were first reported in the
2026-10 improvement list; `I1` and `Q2` from that list are already fixed.

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

### Q2. Keep the historical name `alignment` or rename it to `side` — `fixed`

Decision (2026-10-23): keep `alignment`. It is treated as the project's term for
the side of the conflict (`core/docs/SIDES.md`), so no schema, API or client
renaming happens. Only the documentation explains the meaning; `neutral` and
`draw` remain valid only in the contexts listed there.

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

### I1. `нейтральный` is accepted for a player's end side but rejected by the database — `fixed`

Fixed on 2026-10-23, a player's end side is now `добро`/`зло` everywhere:

- `games_import_staging.alignment_end_ru` is restricted in
  `core/init-scripts/01-schema.sql`, and
  `core/init-scripts/98-migrate-alignment-end.sql` applies the same change to
  existing databases (staged rows with a neutral side are discarded).
- The OpenAPI contract narrows `alignment_end` to `^(добро|зло)$` and explains
  why, and `core/api/app/models.py` was regenerated (`npm run bundle`,
  `generate.py`, `sync.py`).
- The unused alignment regexes were removed from
  `DBConnect/src/api/BotCApiClient.cpp`; the CSV parser already allowed only
  `добро`/`зло`.
- `core/api/app/tests/test_schemas.py` now asserts that `добро`/`зло` pass and
  `нейтральный` is rejected, plus the whole suite stays green (93 tests).

Original report, kept for context:

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

Resolution: the import now fails at the API boundary with 422 instead of an
obscure 500 from the database, because the staging check, the contract and the
client all allow only `добро`/`зло`.

### I2. Duplicate game is reported as a successful import — `open`

`process_games_import()` skips a game whose key already exists
(`01-schema.sql:252-260`), then truncates staging and returns empty `errors`
(`:333-336`). The API treats that as success and returns the id of the existing
game (`core/api/app/import_logic.py:120-142`), so DBConnect shows "game imported,
players created: 0" (`DBConnect/src/ui/ImportGamesWidget.cpp:384-397`) while the
new data is silently dropped.

Next step: return a distinct result for an existing game (a separate status such
as `already_exists` together with the existing id) or compare the submitted
payload with the stored game and report the differing fields as a conflict.
DBConnect should show the game as skipped, not as imported.

### I3. Any database exception other than a duplicate aborts the whole batch — `open`

`core/api/app/import_logic.py:104-106` re-raises, and the batch loop
(`:177-179`) has no per-game handler, so the documented "a failure in one game
does not roll back the others" (`main.py:115`, `import_logic.py:170-171`) is not
true: a check/FK/connection error returns HTTP 500 and the remaining games are
not imported.

Next step: handle failures per game — wrap each import in `try/except`, collect
the failed games with their error text in the batch response, and let the
remaining games continue; align the docstrings and the OpenAPI response schema
with that behaviour.

### I4. `import_roles` is not transactional — `open`

`core/api/app/import_logic.py:261-311` commits each `execute` separately and
writes translations after the role, so a failure in the middle leaves partially
imported roles while the client is told the import failed. The
`UniqueViolationError` branch (`:306-307`) is practically unreachable because the
statement uses `ON CONFLICT DO UPDATE`.

Next step: run the whole role import inside a single transaction (with an
advisory lock, as the games import does), so a failure leaves neither roles nor
translations behind.

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

### C1. TLS verification cannot be re-enabled — `open`

`BotCApiClient::setSslVerify()` only disables verification, and the settings
checkbox promises a toggle (`DBConnect/src/api/BotCApiClient.cpp:58-71`). The
connection test ignores the setting as well (`SettingsWidget.cpp:89` is
commented out), so a user cannot tell which certificate policy is in effect.

Next step: make the toggle two-way — restore the default verification
configuration when it is enabled, never switch verification off for the whole
application — and apply the setting in the connection test.

### C2. Config path is relative to the working directory — `open`

`DBConnect/src/config/ConfigManager.h:55` opens `config.ini` from the CWD, so
starting the executable from another folder silently falls back to
`https://localhost:443`.

Next step: resolve the file next to the executable or through `QStandardPaths`
and show a warning instead of silently using defaults.

### C3. Role cache race — `open`

Roles and players are fetched asynchronously while the cache is read
synchronously when a file is opened (`ImportGamesWidget.cpp:55-57`, `:79-97`),
which produces false "role not in database" errors; `DBCache.cpp:30,46` clears
the cache without a message. `DBCache.h:13-29` declares a signal without
`Q_OBJECT`.

Next step: wait for the initial cache load before validating a file, surface
fetch errors in the UI, and add the missing `Q_OBJECT`.

### C4. Import is enabled despite validation errors — `open`

`ImportGamesWidget.cpp:212-221` keeps the import button active while rows have
validation errors, and the API validates the batch as a whole, so one bad game
yields HTTP 422 shown as a "network error".

Next step: keep the button disabled until validation passes and list the
offending rows and fields next to the error messages.

### C5. Durations above 24 hours are lost — `open`

`GamesCsvParser.cpp:117` parses with `hh:mm:ss`, while the API supports longer
games (`core/api/app/models.py:101`); the preview shows "-".

Next step: parse the same `[days ]hh:mm:ss` form the API accepts.

### C6. CSV parser details — `open`

The error message drops `errorString()` (`CsvParser.cpp:18-20`), a trailing
empty field is discarded (`:82-85`), multi-line quoted fields are not supported
(`:38-41`), and `notes` are copied to every player of a game
(`GamesCsvParser.cpp:119-128`).

Next step: fix the message, keep trailing fields, support quoted multi-line
values, and store notes once per game.

### C7. Cancel button is inert — `open`

The games import progress dialog ignores its cancel button
(`ImportGamesWidget.cpp:107-112`, unlike roles at `:90-96`).

Next step: check `wasCanceled()` after each game, mirroring the roles dialog.

## Security

### S1. API key and server IP are still in git history — `open`

The file is untracked now and `config.ini.example` is tracked, but rotation of
the exposed key was never confirmed.

Next step: rotate the key, confirm the old one is rejected, and keep the new one
only in the local `config.ini`.

### S2. SQL injection in the key-issuing script — `open`

`core/scripts/create-api-key.sh:66-67` interpolates `owner_name` and
`owner_contact` into SQL inside quotes.

Next step: pass the values as `psql` variables (`-v`) or build the statement with
`format()` and quote literals.

### S3. `SECURITY DEFINER` without a fixed `search_path` — `open`

`01-schema.sql:338`, `99-migrate-storyteller-draw.sql:212`.

Next step: add `SET search_path = public, pg_temp` to both functions.

### S4. Weak defaults and exposed surfaces — `open`

`core/docker-compose.yml:14-18` falls back to `change_me` without `.env`;
Swagger UI and `/openapi.json` are public (`core/nginx/nginx.conf:104-112`).

Next step: fail fast when a password is unset and restrict the docs endpoints.

### S5. No import audit — `open`

`validate_api_key` returns the key owner (`core/api/app/auth.py:44-48`) but the
import functions ignore it, so the database does not record who imported a game.

Next step: pass the key owner into the import and store it with the game.

## Tests and CI

### T1. Import paths have no tests against a real database — `open`

No tests for the batch import path (`POST /api/games/import_batch`), for
`get_all_roles(lang)`, for `process_games_import()`, staging or the views; all
existing tests mock asyncpg (`core/api/app/tests/conftest.py:91-96`).

Next step: add PostgreSQL-backed integration tests for the import scenarios —
duplicate game, neutral end side, per-game failure isolation, role import
rollback — plus the analytical views, and run them in CI with a service
container.

### T2. A test asserts behaviour the database does not have — `open`

Repeated import is expected to raise `UniqueViolationError`
(`core/api/app/tests/test_import_logic.py:156-171`), while the SQL function
skips the game and returns empty `errors`. Whatever `I2` decides, this test has
to follow the real behaviour.

### T3. CI does not build DBConnect — `open`

CI builds and tests only `core/api` (`.github/workflows/ci.yml:9-36`); DBConnect
is never compiled.

Next step: build the GUI on a Windows runner with the shared CMake presets.
