-- ============================================================
--  Manual migration for an already-deployed database
-- ============================================================
--
-- init-scripts/*.sql only run automatically on a FRESH Docker volume
-- (first `docker-compose up`). This file is NOT executed automatically —
-- it must be run by hand against the live database, e.g. via the SSH
-- tunnel documented in CLAUDE.md:
--   ssh -L 5432:localhost:5432 botc-ssh@<SERVER_IP>
--   psql -h localhost -U <admin_user> -d botc_stats -f 99-migrate-storyteller-draw.sql
--
-- It brings an existing database created from the pre-multi-storyteller
-- / pre-draw schema up to date with 01-schema.sql. Review before running.

BEGIN;

-- 1. Multiple storytellers: new junction table + backfill from games.storyteller_id
CREATE TABLE IF NOT EXISTS game_storytellers (
    game_id     UUID NOT NULL REFERENCES games (id) ON DELETE CASCADE,
    player_id   UUID NOT NULL REFERENCES players (id),
    PRIMARY KEY (game_id, player_id)
);

CREATE INDEX IF NOT EXISTS idx_gst_game   ON game_storytellers (game_id);
CREATE INDEX IF NOT EXISTS idx_gst_player ON game_storytellers (player_id);

INSERT INTO game_storytellers (game_id, player_id)
SELECT id, storyteller_id FROM games
ON CONFLICT DO NOTHING;

-- Drop the old single-storyteller column/index/unique constraint
DROP INDEX IF EXISTS idx_games_storyteller;
DROP INDEX IF EXISTS uq_games_date_scenario_storyteller_num;
ALTER TABLE games DROP COLUMN IF EXISTS storyteller_id;

CREATE UNIQUE INDEX IF NOT EXISTS uq_games_date_scenario_num
    ON games (game_date, scenario_name, game_number);

-- 2. Draw ("ничья") support
ALTER TABLE games DROP CONSTRAINT IF EXISTS games_alignment_win_check;
ALTER TABLE games ADD CONSTRAINT games_alignment_win_check
    CHECK (alignment_win IN ('good', 'evil', 'draw'));

ALTER TABLE games_import_staging DROP CONSTRAINT IF EXISTS games_import_staging_alignment_win_ru_check;
ALTER TABLE games_import_staging ADD CONSTRAINT games_import_staging_alignment_win_ru_check
    CHECK (alignment_win_ru IN ('добро', 'зло', 'ничья'));

INSERT INTO alignment_translations (alignment_en, lang_code, name) VALUES
    ('draw', 'en', 'draw'),
    ('draw', 'ru', 'ничья')
ON CONFLICT (alignment_en, lang_code) DO NOTHING;

-- 3. Re-create process_games_import() and v_game_summary with the new logic.
--    Re-run the CREATE OR REPLACE FUNCTION process_games_import() and
--    CREATE OR REPLACE VIEW v_game_summary statements from the current
--    01-schema.sql after this migration (both use CREATE OR REPLACE, safe to
--    re-run against live data).

-- 4. Grants for the new table
GRANT SELECT, INSERT, UPDATE, DELETE ON game_storytellers TO botc_user;
GRANT INSERT, SELECT ON game_storytellers TO api_service;

COMMIT;
