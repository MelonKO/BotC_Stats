-- ============================================================
--  Manual migration for an already-deployed database
-- ============================================================
--
-- init-scripts/*.sql only run automatically on a FRESH Docker volume
-- (first `docker-compose up`). This file is NOT executed automatically —
-- it must be run by hand against the live database, e.g.:
--   docker exec -i botc-postgres psql -U postgres -d botc_stats \
--     -v ON_ERROR_STOP=1 < 99-migrate-storyteller-draw.sql
--
-- It brings an existing database created from the pre-multi-storyteller
-- / pre-draw schema up to date with 01-schema.sql: creates the
-- game_storytellers junction table (backfilled from games.storyteller_id),
-- drops the old column, widens alignment_win to allow 'draw'/'ничья',
-- and re-creates process_games_import() and v_game_summary.
-- The whole migration runs in a single transaction. Review before running.

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

-- Drop the view that depends on games.storyteller_id (re-created below)
DROP VIEW IF EXISTS v_game_summary;

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

-- 3. Re-create process_games_import() (same definition as 01-schema.sql)
CREATE OR REPLACE FUNCTION process_games_import()
RETURNS TABLE (
    games_created INTEGER,
    players_created INTEGER,
    errors TEXT
) AS $$
DECLARE
    v_game_id UUID;
    v_player_id UUID;
    v_storyteller_id UUID;
    v_storyteller_name TEXT;
    v_role_start_id UUID;
    v_role_end_id UUID;
    v_alignment_win TEXT;
    v_alignment_end TEXT;
    rec RECORD;
    v_games_count INTEGER := 0;
    v_players_count INTEGER := 0;
    v_missing_roles TEXT[];
    v_new_player_count INTEGER;
BEGIN
    -- Step 1: Validate Russian role names exist in role_translations
    SELECT ARRAY_AGG(DISTINCT missing_role) INTO v_missing_roles
    FROM (
        SELECT role_start_name_ru AS missing_role
        FROM games_import_staging
        WHERE role_start_name_ru NOT IN (SELECT name FROM role_translations WHERE lang_code = 'ru')
        UNION
        SELECT role_end_name_ru AS missing_role
        FROM games_import_staging
        WHERE role_end_name_ru NOT IN (SELECT name FROM role_translations WHERE lang_code = 'ru')
    ) AS missing_roles_subquery;

    -- If any roles are missing, abort and return error message
    IF v_missing_roles IS NOT NULL THEN
        RETURN QUERY SELECT
            0,
            0,
            format('Отсутствуют роли в справочнике переводов: %s. Добавьте их в таблицу role_translations перед импортом.',
                   array_to_string(v_missing_roles, ', '));
        RETURN;
    END IF;

    -- Step 2: Map Russian alignment → English (validate + convert)
    -- alignment_win_ru: 'добро' → 'good', 'зло' → 'evil', 'ничья' → 'draw'
    -- alignment_end_ru: 'добро' → 'good', 'зло' → 'evil', 'нейтральный' → 'neutral'

    -- Step 3: Process each unique game session from staging
    FOR rec IN
        SELECT DISTINCT
            game_date, scenario_name, storyteller_name, alignment_win_ru,
            location, game_number, duration, notes
        FROM games_import_staging
    LOOP
        -- Map alignment_win_ru → English
        SELECT alignment_en INTO v_alignment_win
        FROM alignment_translations
        WHERE name = rec.alignment_win_ru AND lang_code = 'ru';

        IF v_alignment_win IS NULL THEN
            RETURN QUERY SELECT
                0, 0,
                format('Неизвестное значение alignment_win: %s. Допустимы: добро, зло, ничья.', rec.alignment_win_ru);
            RETURN;
        END IF;

        -- Skip if this game session already exists
        IF EXISTS (
            SELECT 1 FROM games
            WHERE game_date = rec.game_date
              AND scenario_name = rec.scenario_name
              AND game_number = rec.game_number
        ) THEN
            CONTINUE;
        END IF;

        -- Create the game session record
        INSERT INTO games (
            game_date, scenario_name, alignment_win, location, game_number, duration, notes
        ) VALUES (
            rec.game_date, rec.scenario_name, v_alignment_win, rec.location, rec.game_number, rec.duration, rec.notes
        ) RETURNING id INTO v_game_id;

        v_games_count := v_games_count + 1;

        -- Resolve or auto-create each storyteller in the comma-separated list, link to the game
        FOR v_storyteller_name IN
            SELECT DISTINCT TRIM(name) FROM UNNEST(STRING_TO_ARRAY(rec.storyteller_name, ',')) AS name
            WHERE TRIM(name) <> ''
        LOOP
            SELECT id INTO v_storyteller_id FROM players WHERE name = v_storyteller_name;
            IF v_storyteller_id IS NULL THEN
                INSERT INTO players (name) VALUES (v_storyteller_name) RETURNING id INTO v_storyteller_id;
                v_players_count := v_players_count + 1;
            END IF;

            INSERT INTO game_storytellers (game_id, player_id) VALUES (v_game_id, v_storyteller_id)
            ON CONFLICT DO NOTHING;
        END LOOP;

        -- Insert all player participations for this game
        -- First, create missing players (excluding storyteller which is already handled)
        INSERT INTO players (name)
        SELECT DISTINCT s.player_name
        FROM games_import_staging s
        WHERE
            s.game_date = rec.game_date AND
            s.scenario_name = rec.scenario_name AND
            s.storyteller_name = rec.storyteller_name AND
            s.alignment_win_ru = rec.alignment_win_ru AND
            s.location = rec.location AND
            s.game_number = rec.game_number AND
            s.player_name NOT IN (SELECT name FROM players);

        -- Count newly created players for this game
        GET DIAGNOSTICS v_new_player_count = ROW_COUNT;
        v_players_count := v_players_count + v_new_player_count;

        -- Then insert game players with resolved player IDs and mapped English role names
        INSERT INTO game_players (
            game_id, player_id, role_start_id, role_end_id, alignment_end, is_alive, seat_number
        )
        SELECT
            v_game_id,
            p.id,
            (SELECT r.id FROM roles r
             JOIN role_translations rt ON rt.role_id = r.id
             WHERE rt.name = s.role_start_name_ru AND rt.lang_code = 'ru'),
            (SELECT r.id FROM roles r
             JOIN role_translations rt ON rt.role_id = r.id
             WHERE rt.name = s.role_end_name_ru AND rt.lang_code = 'ru'),
            (SELECT alignment_en FROM alignment_translations
             WHERE name = s.alignment_end_ru AND lang_code = 'ru'),
            s.is_alive,
            s.seat_number
        FROM games_import_staging s
        JOIN players p ON p.name = s.player_name
        WHERE
            s.game_date = rec.game_date AND
            s.scenario_name = rec.scenario_name AND
            s.storyteller_name = rec.storyteller_name AND
            s.alignment_win_ru = rec.alignment_win_ru AND
            s.location = rec.location AND
            s.game_number = rec.game_number;
    END LOOP;

    -- Step 4: Clear staging table after successful processing
    TRUNCATE TABLE games_import_staging;

    -- Return import statistics
    RETURN QUERY SELECT v_games_count, v_players_count, ''::TEXT;
END;
$$ LANGUAGE plpgsql SECURITY DEFINER;

-- 4. Re-create v_game_summary (same definition as 01-schema.sql)
CREATE VIEW v_game_summary AS
SELECT
    g.id                                                                AS game_uuid,
    g.game_date,
    g.game_number,
    g.scenario_name,
    g.location,
    g.duration,
    g.notes,
    st.storyteller,
    g.alignment_win,
    at.name                                                             AS alignment_win_ru,
    COUNT(gp.id)                                                        AS players,
    COUNT(gp.id) FILTER (WHERE gp.alignment_end = 'good')              AS good_players,
    COUNT(gp.id) FILTER (WHERE gp.alignment_end = 'evil')              AS evil_players,
    COUNT(gp.id) FILTER (WHERE gp.is_alive)                            AS survivors,
    COUNT(gp.id) FILTER (
        WHERE gp.role_start_id <> gp.role_end_id
    )                                                                   AS role_changes
FROM games g
LEFT JOIN LATERAL (
    SELECT STRING_AGG(p.name, ', ' ORDER BY p.name) AS storyteller
    FROM game_storytellers gst
    JOIN players p ON p.id = gst.player_id
    WHERE gst.game_id = g.id
) st ON true
LEFT JOIN game_players gp ON gp.game_id = g.id
LEFT JOIN alignment_translations at ON at.alignment_en = g.alignment_win AND at.lang_code = 'ru'
GROUP BY g.id, g.game_date, g.game_number, g.scenario_name, g.location, g.duration, g.notes, st.storyteller, g.alignment_win, at.name;

-- 5. Grants for the new table and the re-created view
GRANT SELECT, INSERT, UPDATE, DELETE ON game_storytellers TO botc_user;
GRANT INSERT, SELECT ON game_storytellers TO api_service;
GRANT SELECT ON v_game_summary TO api_service;
GRANT SELECT ON game_storytellers, v_game_summary TO metabase_readonly;

COMMIT;
