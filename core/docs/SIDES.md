# Sides of the conflict ("alignment" fields)

The column name `alignment` is historical. In this project it means **side of the
conflict** (сторона), not "выравнивание". Four different things are called a
"side" and they do **not** share the same set of allowed values; most confusion
around the import API comes from mixing them up. Each context is defined below.

## 1. Role side — `roles.alignment`

Values: `good | evil | neutral`.

- `good` / `evil` — the role belongs to that team.
- `neutral` — the role does not fix the side. Used by roles such as Traveller
  ("Странник", `role_type = 'Traveller'`): the storyteller decides which side
  that player joins at the start of the game.

Russian names come from `alignment_translations`: `добро`, `зло`, `нейтральный`.

## 2. Winning side — `games.alignment_win`

Values: `good | evil | draw`.

- `draw` (`ничья`) is an exceptional outcome, not a regular result.
- `neutral` never wins a game.

## 3. Player side at the end — `game_players.alignment_end`

Values: `good | evil`. Nothing else.

- It is the side the player actually played for by the end of the game.
- A Traveller whose role is `neutral` still ends as `good` or `evil`: the role's
  `neutral` describes "decided by the storyteller", not a third team.
- `neutral` and `draw` are never valid here.

## 4. Player side at the start

Not stored as a column. The current role side can be looked up through
`game_players.role_start_id -> roles.alignment`, but it is not a historical
snapshot of the player's side at the start of that game:

- `good` and `evil` describe the role's current catalog side, which may have
  changed since the game was imported;
- `neutral` does not identify the player's side — only the storyteller knows
  which side that player took, and the import format does not carry it;
- `roles.alignment` is nullable in the database schema, so a direct database
  write can also leave the role side unknown. The API requires a non-null value.

If start-side statistics are ever needed, add an explicit `alignment_start`
column with the same `CHECK (alignment_start IN ('good', 'evil'))` constraint
instead of guessing it from a `neutral` role.

## Invariants

| Field | Allowed values |
|---|---|
| `roles.alignment` | `good`, `evil`, `neutral` (or `NULL` in the database) |
| `games.alignment_win` | `good`, `evil`, `draw` |
| `game_players.alignment_end` | `good`, `evil` |
| `games_import_staging.alignment_win_ru` | `добро`, `зло`, `ничья` |
| `games_import_staging.alignment_end_ru` | `добро`, `зло` |

Russian and English values are linked through `alignment_translations`
(`добро`/`good`, `зло`/`evil`, `ничья`/`draw`, `нейтральный`/`neutral`). The
dictionary is shared, so containing `neutral` there does not mean every field
accepts it — the per-field constraints above are the source of truth.

## Where the model lives

- Schema: `core/init-scripts/01-schema.sql` — `roles` (:39), `games` (:94),
  `game_players` (:123), `games_import_staging` (:166, :175), mapping notes (:230).
- Import function: `process_games_import()` in the same file.
- API contract: `openapi/api/openapi.yaml` (`alignment`, `alignment_win`,
  `alignment_end`) and the generated `core/api/app/models.py`.
- DBConnect: CSV validation in
  `DBConnect/src/utils/games/parser/GamesCsvParser.cpp` (`VALID_ALIGNMENTS` =
  `добро`, `зло`; `VALID_ALIGNMENT_WINS` = `добро`, `зло`, `ничья`).

## Known inconsistencies

The alignment fields now follow the model above. Open questions around it are
tracked in [`KNOWN-ISSUES.md`](KNOWN-ISSUES.md): `Q1` (the missing
`alignment_start` field, deferred) and `Q3`/`Q4` (how draws and Travellers are
counted in the statistics). Keep that list up to date instead of repeating the
details here.

## Rules for new fields and endpoints

- Name fields after their context: `alignment_role`, `alignment_win`,
  `alignment_start`, `alignment_end`. Never add a bare `alignment` to a new
  table or endpoint.
- Keep the value set as small as the domain allows. `neutral` belongs only where
  the side is genuinely not fixed by the role; `draw` belongs only to game
  outcomes.
- Use Russian values only at the CSV/staging boundary, English values everywhere
  inside the database and the API.
