-- ============================================================
--  Blood on the Clocktower — PostgreSQL schema (final)
-- ============================================================

-- ============================================================
--  Section 1: Enumerated types
-- ============================================================

-- Standard BotC role types (user-facing values in Russian)
CREATE TYPE role_type AS ENUM (
    'Горожанин',
    'Изгой',
    'Приспешник',
    'Демон',
    'Странник'
);

-- ============================================================
--  Section 2: Core reference tables
-- ============================================================

-- Players registry: unique by name, with optional contact fields
CREATE TABLE players (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name            TEXT NOT NULL UNIQUE,
    telegram        TEXT,
    vk              TEXT,
    phone           TEXT,
    email           TEXT,
    notes           TEXT
);

CREATE INDEX idx_players_name ON players (name);

-- Roles catalog: name uniqueness, color constraint, type enumeration
CREATE TABLE roles (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name        TEXT NOT NULL UNIQUE,
    color       TEXT CHECK (color IN ('синий', 'красный', 'нейтральный')),
    role_type   role_type NOT NULL
);

CREATE INDEX idx_roles_color     ON roles (color);
CREATE INDEX idx_roles_role_type ON roles (role_type);

-- ============================================================
--  Section 3: Game session tables (normalized)
-- ============================================================

-- Games metadata: one row per session
CREATE TABLE games (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    game_date       DATE NOT NULL DEFAULT CURRENT_DATE,
    scenario_name   TEXT NOT NULL,
    storyteller_id  UUID NOT NULL REFERENCES players (id),
    color_win       TEXT NOT NULL CHECK (color_win IN ('синий', 'красный')),
    location        TEXT NOT NULL,
    game_number     INTEGER NOT NULL DEFAULT 1
);

CREATE INDEX idx_games_date        ON games (game_date DESC);
CREATE INDEX idx_games_scenario    ON games (scenario_name);
CREATE INDEX idx_games_storyteller ON games (storyteller_id);
CREATE UNIQUE INDEX uq_games_date_scenario_storyteller_num
    ON games (game_date, scenario_name, storyteller_id, game_number);

-- Game participants: junction table linking players to games with role assignment
CREATE TABLE game_players (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    game_id         UUID NOT NULL REFERENCES games (id) ON DELETE CASCADE,
    player_id       UUID NOT NULL REFERENCES players (id),
    role_start_id   UUID NOT NULL REFERENCES roles (id),
    role_end_id     UUID NOT NULL REFERENCES roles (id),
    color_end       TEXT NOT NULL CHECK (color_end IN ('синий', 'красный')),
    is_alive        BOOLEAN NOT NULL,
    UNIQUE (game_id, player_id)
);

CREATE INDEX idx_gp_game       ON game_players (game_id);
CREATE INDEX idx_gp_player     ON game_players (player_id);
CREATE INDEX idx_gp_role_start ON game_players (role_start_id);
CREATE INDEX idx_gp_role_end   ON game_players (role_end_id);

-- ============================================================
--  Section 4: Staging table for CSV import (denormalized for user entry)
-- ============================================================

-- Temporary table matching Excel/CSV structure: one row per player per game
-- Users import data here first, then call process_games_import() to distribute
CREATE TABLE games_import_staging (
    game_date       DATE NOT NULL,
    scenario_name   TEXT NOT NULL,
    storyteller_name TEXT NOT NULL,
    color_win       TEXT NOT NULL CHECK (color_win IN ('синий', 'красный')),
    location        TEXT NOT NULL,
    game_number     INTEGER NOT NULL,
    player_name     TEXT NOT NULL,
    role_start_name TEXT NOT NULL,
    role_end_name   TEXT NOT NULL,
    color_end       TEXT NOT NULL CHECK (color_end IN ('синий', 'красный')),
    is_alive        BOOLEAN NOT NULL
);

-- ============================================================
--  Section 5: Import procedure with strict role validation
-- ============================================================

-- Validates all roles exist before import, auto-creates missing players,
-- distributes data to normalized tables, returns import statistics
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
    v_role_start_id UUID;
    v_role_end_id UUID;
    rec RECORD;
    v_games_count INTEGER := 0;
    v_players_count INTEGER := 0;
    v_missing_roles TEXT[];
    v_new_player_count INTEGER;
BEGIN
    -- Step 1: Strict validation — collect all role names from staging that don't exist in roles table
    SELECT ARRAY_AGG(DISTINCT missing_role) INTO v_missing_roles
    FROM (
        SELECT role_start_name AS missing_role 
        FROM games_import_staging
        WHERE role_start_name NOT IN (SELECT name FROM roles)
        UNION
        SELECT role_end_name AS missing_role 
        FROM games_import_staging
        WHERE role_end_name NOT IN (SELECT name FROM roles)
    ) AS missing_roles_subquery;

    -- If any roles are missing, abort and return error message
    IF v_missing_roles IS NOT NULL THEN
        RETURN QUERY SELECT 
            0, 
            0, 
            format('Отсутствуют роли в справочнике: %s. Добавьте их в таблицу roles перед импортом.', 
                   array_to_string(v_missing_roles, ', '));
        RETURN;
    END IF;

    -- Step 2: Process each unique game session from staging
    FOR rec IN
        SELECT DISTINCT
            game_date, scenario_name, storyteller_name, color_win, location, game_number
        FROM games_import_staging
    LOOP
        -- Resolve or auto-create storyteller player record
        SELECT id INTO v_storyteller_id FROM players WHERE name = rec.storyteller_name;
        IF v_storyteller_id IS NULL THEN
            INSERT INTO players (name) VALUES (rec.storyteller_name) RETURNING id INTO v_storyteller_id;
            v_players_count := v_players_count + 1;
        END IF;

        -- Skip if this game session already exists
        IF EXISTS (
            SELECT 1 FROM games
            WHERE game_date = rec.game_date
              AND scenario_name = rec.scenario_name
              AND storyteller_id = v_storyteller_id
              AND game_number = rec.game_number
        ) THEN
            CONTINUE;
        END IF;

        -- Create the game session record
        INSERT INTO games (
            game_date, scenario_name, storyteller_id, color_win, location, game_number
        ) VALUES (
            rec.game_date, rec.scenario_name, v_storyteller_id, rec.color_win, rec.location, rec.game_number
        ) RETURNING id INTO v_game_id;

        v_games_count := v_games_count + 1;

        -- Insert all player participations for this game
        -- First, create missing players (excluding storyteller which is already handled)
        INSERT INTO players (name)
        SELECT DISTINCT s.player_name
        FROM games_import_staging s
        WHERE
            s.game_date = rec.game_date AND
            s.scenario_name = rec.scenario_name AND
            s.storyteller_name = rec.storyteller_name AND
            s.color_win = rec.color_win AND
            s.location = rec.location AND
            s.game_number = rec.game_number AND
            s.player_name NOT IN (SELECT name FROM players);

        -- Count newly created players for this game
        GET DIAGNOSTICS v_new_player_count = ROW_COUNT;
        v_players_count := v_players_count + v_new_player_count;

        -- Then insert game players with resolved player IDs
        INSERT INTO game_players (
            game_id, player_id, role_start_id, role_end_id, color_end, is_alive
        )
        SELECT
            v_game_id,
            p.id,
            (SELECT id FROM roles WHERE name = s.role_start_name),
            (SELECT id FROM roles WHERE name = s.role_end_name),
            s.color_end,
            s.is_alive
        FROM games_import_staging s
        JOIN players p ON p.name = s.player_name
        WHERE
            s.game_date = rec.game_date AND
            s.scenario_name = rec.scenario_name AND
            s.storyteller_name = rec.storyteller_name AND
            s.color_win = rec.color_win AND
            s.location = rec.location AND
            s.game_number = rec.game_number;
    END LOOP;

    -- Step 3: Clear staging table after successful processing
    TRUNCATE TABLE games_import_staging;

    -- Return import statistics
    RETURN QUERY SELECT v_games_count, v_players_count, ''::TEXT;
END;
$$ LANGUAGE plpgsql;

-- ============================================================
--  Section 6: Seed data — example roles (Russian names)
-- ============================================================

INSERT INTO roles (name, color, role_type) VALUES
    ('Дамочка',         'синий',    'Изгой'),
    ('Азартный игрок',  'синий',    'Горожанин'),
    ('Убийца',          'красный',  'Приспешник'),
    ('Амнезиак',        'синий',    'Горожанин'),
    ('Политик',         'синий',    'Изгой'),
    ('Пукка',           'красный',  'Демон');

-- ============================================================
--  Section 7: Analytics views (adapted to normalized schema)
-- ============================================================

-- Player statistics: games played, win rate, team distribution, survival
CREATE VIEW v_player_stats AS
SELECT
    p.id,
    p.name,
    COUNT(gp.id)                                                        AS games_played,
    COUNT(gp.id) FILTER (WHERE gp.color_end = g.color_win)             AS games_won,
    ROUND(
        COUNT(gp.id) FILTER (WHERE gp.color_end = g.color_win)::numeric
        / NULLIF(COUNT(gp.id), 0) * 100, 1
    )                                                                   AS win_rate_pct,
    COUNT(gp.id) FILTER (WHERE gp.color_end = 'синий')                 AS games_as_blue,
    COUNT(gp.id) FILTER (WHERE gp.color_end = 'красный')               AS games_as_red,
    COUNT(gp.id) FILTER (WHERE gp.is_alive)                            AS survived,
    COUNT(gp.id) FILTER (WHERE NOT gp.is_alive)                        AS died
FROM players p
LEFT JOIN game_players gp ON gp.player_id = p.id
LEFT JOIN games         g  ON g.id         = gp.game_id
GROUP BY p.id, p.name;

-- Role effectiveness: win rate and survival by starting role
CREATE VIEW v_role_stats AS
SELECT
    r.name                                                              AS role_name,
    r.role_type,
    r.color,
    COUNT(gp.id)                                                        AS times_played,
    COUNT(gp.id) FILTER (WHERE gp.color_end = g.color_win)             AS times_won,
    ROUND(
        COUNT(gp.id) FILTER (WHERE gp.color_end = g.color_win)::numeric
        / NULLIF(COUNT(gp.id), 0) * 100, 1
    )                                                                   AS win_rate_pct,
    COUNT(gp.id) FILTER (WHERE gp.is_alive)                            AS survivals,
    COUNT(gp.id) FILTER (
        WHERE gp.role_start_id <> gp.role_end_id
    )                                                                   AS role_changed_count
FROM roles r
LEFT JOIN game_players gp ON gp.role_start_id = r.id
LEFT JOIN games         g  ON g.id              = gp.game_id
GROUP BY r.id, r.name, r.role_type, r.color;

-- Game session summary: high-level overview per game
CREATE VIEW v_game_summary AS
SELECT
    g.id                                                                AS game_uuid,
    g.game_date,
    g.game_number,
    g.scenario_name,
    g.location,
    st.name                                                             AS storyteller,
    g.color_win,
    COUNT(gp.id)                                                        AS players,
    COUNT(gp.id) FILTER (WHERE gp.color_end = 'синий')                 AS blue_players,
    COUNT(gp.id) FILTER (WHERE gp.color_end = 'красный')               AS red_players,
    COUNT(gp.id) FILTER (WHERE gp.is_alive)                            AS survivors,
    COUNT(gp.id) FILTER (
        WHERE gp.role_start_id <> gp.role_end_id
    )                                                                   AS role_changes
FROM games g
JOIN players       st ON st.id = g.storyteller_id
LEFT JOIN game_players gp ON gp.game_id = g.id
GROUP BY g.id, g.game_date, g.game_number, g.scenario_name, g.location, st.name, g.color_win;

-- Role type effectiveness: aggregated win rates by role category and team
CREATE VIEW v_role_type_stats AS
SELECT
    r.role_type,
    r.color,
    COUNT(gp.id)                                                        AS times_played,
    ROUND(
        COUNT(gp.id) FILTER (WHERE gp.color_end = g.color_win)::numeric
        / NULLIF(COUNT(gp.id), 0) * 100, 1
    )                                                                   AS win_rate_pct
FROM game_players gp
JOIN roles r ON r.id  = gp.role_start_id
JOIN games g ON g.id  = gp.game_id
GROUP BY r.role_type, r.color
ORDER BY r.color, r.role_type;