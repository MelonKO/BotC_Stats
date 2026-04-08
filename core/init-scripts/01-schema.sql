-- ============================================================
--  Blood on the Clocktower — PostgreSQL schema (localized)
-- ============================================================

-- ============================================================
--  Section 1: Enumerated types
-- ============================================================

-- Standard BotC role types (official English values)
CREATE TYPE role_type AS ENUM (
    'Townsfolk',
    'Outsider',
    'Minion',
    'Demon',
    'Traveller'
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

-- Roles catalog: English names, alignment constraint, type enumeration
CREATE TABLE roles (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name        TEXT NOT NULL UNIQUE,
    alignment   TEXT CHECK (alignment IN ('good', 'evil', 'neutral')),
    role_type   role_type NOT NULL,
    description TEXT
);

CREATE INDEX idx_roles_alignment ON roles (alignment);
CREATE INDEX idx_roles_role_type  ON roles (role_type);

-- ============================================================
--  Section 3: Localization tables
-- ============================================================

-- Supported languages
CREATE TABLE languages (
    code        TEXT PRIMARY KEY,
    name        TEXT NOT NULL
);

-- Role name translations (one translation per role per language)
CREATE TABLE role_translations (
    role_id     UUID NOT NULL REFERENCES roles(id) ON DELETE CASCADE,
    lang_code   TEXT NOT NULL REFERENCES languages(code),
    name        TEXT NOT NULL,
    description TEXT,

    PRIMARY KEY (role_id, lang_code)
);

-- Role type translations
CREATE TABLE role_type_translations (
    role_type_en  TEXT NOT NULL,
    lang_code     TEXT NOT NULL REFERENCES languages(code),
    name          TEXT NOT NULL,

    PRIMARY KEY (role_type_en, lang_code)
);

-- Alignment translations
CREATE TABLE alignment_translations (
    alignment_en  TEXT NOT NULL,
    lang_code     TEXT NOT NULL REFERENCES languages(code),
    name          TEXT NOT NULL,

    PRIMARY KEY (alignment_en, lang_code)
);

-- ============================================================
--  Section 4: Game session tables (normalized)
-- ============================================================

-- Games metadata: one row per session
CREATE TABLE games (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    game_date       DATE NOT NULL DEFAULT CURRENT_DATE,
    scenario_name   TEXT NOT NULL,
    storyteller_id  UUID NOT NULL REFERENCES players (id),
    alignment_win   TEXT NOT NULL CHECK (alignment_win IN ('good', 'evil')),
    location        TEXT NOT NULL,
    game_number     INTEGER NOT NULL DEFAULT 1,
    duration        INTERVAL,
    notes           TEXT
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
    alignment_end   TEXT NOT NULL CHECK (alignment_end IN ('good', 'evil')),
    is_alive        BOOLEAN NOT NULL,
    seat_number     INTEGER,
    UNIQUE (game_id, player_id)
);

CREATE INDEX idx_gp_game       ON game_players (game_id);
CREATE INDEX idx_gp_player     ON game_players (player_id);
CREATE INDEX idx_gp_role_start ON game_players (role_start_id);
CREATE INDEX idx_gp_role_end   ON game_players (role_end_id);

-- ============================================================
--  Section 5: API authentication keys
-- ============================================================

-- API keys for external import clients
-- Keys are stored as SHA-256 hashes, never in plaintext
CREATE TABLE api_keys (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    key_hash        TEXT NOT NULL UNIQUE,     -- SHA-256 hash of the actual key
    owner_name      TEXT NOT NULL,             -- Human-readable owner name
    owner_contact   TEXT,                      -- Email, telegram, etc.
    active          BOOLEAN NOT NULL DEFAULT true,
    created_at      TIMESTAMPTZ NOT NULL DEFAULT now(),
    revoked_at      TIMESTAMPTZ,
    last_used_at    TIMESTAMPTZ
);

CREATE INDEX idx_api_keys_hash    ON api_keys (key_hash);
CREATE INDEX idx_api_keys_active  ON api_keys (active);

COMMENT ON TABLE api_keys IS 'API keys for external import access — store only hashes';

-- ============================================================
--  Section 6: Staging table for CSV import (denormalized, Russian input)
-- ============================================================

-- Temporary table matching Excel/CSV structure: accepts Russian values
-- Users import data here first, then call process_games_import() to distribute
CREATE TABLE games_import_staging (
    game_date           DATE NOT NULL,
    scenario_name       TEXT NOT NULL,
    storyteller_name    TEXT NOT NULL,
    alignment_win_ru    TEXT NOT NULL CHECK (alignment_win_ru IN ('добро', 'зло')),
    location            TEXT NOT NULL,
    game_number         INTEGER NOT NULL,
    duration            INTERVAL,
    notes               TEXT,
    player_name         TEXT NOT NULL,
    seat_number         INTEGER,
    role_start_name_ru  TEXT NOT NULL,
    role_end_name_ru    TEXT NOT NULL,
    alignment_end_ru    TEXT NOT NULL CHECK (alignment_end_ru IN ('добро', 'зло', 'нейтральный')),
    is_alive            BOOLEAN NOT NULL
);

-- ============================================================
--  Section 7: Import procedure with localization mapping
-- ============================================================

-- Validates all roles exist (via Russian translations), maps Russian
-- alignment → English, auto-creates missing players, distributes data
-- to normalized tables, returns import statistics
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
    -- alignment_win_ru: 'добро' → 'good', 'зло' → 'evil'
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
                format('Неизвестное значение alignment_win: %s. Допустимы: добро, зло.', rec.alignment_win_ru);
            RETURN;
        END IF;

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
            game_date, scenario_name, storyteller_id, alignment_win, location, game_number, duration, notes
        ) VALUES (
            rec.game_date, rec.scenario_name, v_storyteller_id, v_alignment_win, rec.location, rec.game_number, rec.duration, rec.notes
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

-- ============================================================
--  Section 8: Seed data — languages, translations, example roles (English names)
-- ============================================================

-- Supported languages
INSERT INTO languages (code, name) VALUES
    ('en', 'English'),
    ('ru', 'Русский');

-- Role type translations
INSERT INTO role_type_translations (role_type_en, lang_code, name) VALUES
    ('Townsfolk', 'en', 'Townsfolk'),
    ('Outsider',  'en', 'Outsider'),
    ('Minion',    'en', 'Minion'),
    ('Demon',     'en', 'Demon'),
    ('Traveller', 'en', 'Traveller'),
    ('Townsfolk', 'ru', 'Горожанин'),
    ('Outsider',  'ru', 'Изгой'),
    ('Minion',    'ru', 'Приспешник'),
    ('Demon',     'ru', 'Демон'),
    ('Traveller', 'ru', 'Странник');

-- Alignment translations
INSERT INTO alignment_translations (alignment_en, lang_code, name) VALUES
    ('good',      'en', 'good'),
    ('evil',      'en', 'evil'),
    ('neutral',   'en', 'neutral'),
    ('good',      'ru', 'добро'),
    ('evil',      'ru', 'зло'),
    ('neutral',   'ru', 'нейтральный');

-- Roles (English names)
INSERT INTO roles (name, alignment, role_type) VALUES
    ('Chambermaid',     'good',   'Outsider'),
    ('Gambler',         'good',   'Townsfolk'),
    ('Assassin',        'evil',   'Minion'),
    ('Amnesiac',        'good',   'Townsfolk'),
    ('Politician',      'good',   'Outsider'),
    ('Pukka',           'evil',   'Demon'),
    ('Washerwoman',     'good',   'Townsfolk'),
    ('Librarian',       'good',   'Townsfolk'),
    ('Investigator',    'good',   'Townsfolk'),
    ('Cook',            'good',   'Townsfolk'),
    ('Pacifist',        'good',   'Townsfolk'),
    ('Blue Executioner','evil',   'Outsider'),
    ('Red Widow',       'evil',   'Outsider'),
    ('Poisoner',        'evil',   'Minion'),
    ('Spy',             'evil',   'Minion'),
    ('Baron',           'evil',   'Minion'),
    ('Widow',           'evil',   'Minion'),
    ('Imp',             'evil',   'Demon'),
    ('Vigormortis',     'evil',   'Demon'),
    ('Leviathan',       'evil',   'Demon'),
    ('Queen',           'neutral','Traveller'),
    ('Jester',          'neutral','Traveller');

-- Role name translations (English — auto-fill from roles)
INSERT INTO role_translations (role_id, lang_code, name, description)
SELECT r.id, 'en', r.name, NULL
FROM roles r;

-- Role name translations (Russian)
INSERT INTO role_translations (role_id, lang_code, name, description) VALUES
    ((SELECT id FROM roles WHERE name = 'Chambermaid'),      'ru', 'Дамочка', NULL),
    ((SELECT id FROM roles WHERE name = 'Gambler'),          'ru', 'Азартный игрок', NULL),
    ((SELECT id FROM roles WHERE name = 'Assassin'),         'ru', 'Убийца', NULL),
    ((SELECT id FROM roles WHERE name = 'Amnesiac'),         'ru', 'Амнезиак', NULL),
    ((SELECT id FROM roles WHERE name = 'Politician'),       'ru', 'Политик', NULL),
    ((SELECT id FROM roles WHERE name = 'Pukka'),            'ru', 'Пукка', NULL),
    ((SELECT id FROM roles WHERE name = 'Washerwoman'),      'ru', 'Прачка', NULL),
    ((SELECT id FROM roles WHERE name = 'Librarian'),        'ru', 'Библиотекарь', NULL),
    ((SELECT id FROM roles WHERE name = 'Investigator'),     'ru', 'Исследователь', NULL),
    ((SELECT id FROM roles WHERE name = 'Cook'),             'ru', 'Повар', NULL),
    ((SELECT id FROM roles WHERE name = 'Pacifist'),         'ru', 'Миротворец', NULL),
    ((SELECT id FROM roles WHERE name = 'Blue Executioner'), 'ru', 'Синий Палач', NULL),
    ((SELECT id FROM roles WHERE name = 'Red Widow'),        'ru', 'Красная Вдова', NULL),
    ((SELECT id FROM roles WHERE name = 'Poisoner'),         'ru', 'Отравитель', NULL),
    ((SELECT id FROM roles WHERE name = 'Spy'),              'ru', 'Шпион', NULL),
    ((SELECT id FROM roles WHERE name = 'Baron'),            'ru', 'Барон', NULL),
    ((SELECT id FROM roles WHERE name = 'Widow'),            'ru', 'Ворон', NULL),
    ((SELECT id FROM roles WHERE name = 'Imp'),              'ru', 'Имп', NULL),
    ((SELECT id FROM roles WHERE name = 'Vigormortis'),      'ru', 'Виверна', NULL),
    ((SELECT id FROM roles WHERE name = 'Leviathan'),        'ru', 'Левиафан', NULL),
    ((SELECT id FROM roles WHERE name = 'Queen'),            'ru', 'Королева', NULL),
    ((SELECT id FROM roles WHERE name = 'Jester'),           'ru', 'Шут', NULL);

-- ============================================================
--  Section 9: Analytics views (localized)
-- ============================================================

-- Player statistics: games played, win rate, team distribution, survival
CREATE VIEW v_player_stats AS
SELECT
    p.id,
    p.name,
    COUNT(gp.id)                                                        AS games_played,
    COUNT(gp.id) FILTER (WHERE gp.alignment_end = g.alignment_win)     AS games_won,
    ROUND(
        COUNT(gp.id) FILTER (WHERE gp.alignment_end = g.alignment_win)::numeric
        / NULLIF(COUNT(gp.id), 0) * 100, 1
    )                                                                   AS win_rate_pct,
    COUNT(gp.id) FILTER (WHERE gp.alignment_end = 'good')              AS games_as_good,
    COUNT(gp.id) FILTER (WHERE gp.alignment_end = 'evil')              AS games_as_evil,
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
    r.alignment,
    rtt.name                                                            AS role_type_ru,
    at.name                                                             AS alignment_ru,
    COALESCE(rt.name, r.name)                                           AS role_name_ru,
    COUNT(gp.id)                                                        AS times_played,
    COUNT(gp.id) FILTER (WHERE gp.alignment_end = g.alignment_win)     AS times_won,
    ROUND(
        COUNT(gp.id) FILTER (WHERE gp.alignment_end = g.alignment_win)::numeric
        / NULLIF(COUNT(gp.id), 0) * 100, 1
    )                                                                   AS win_rate_pct,
    COUNT(gp.id) FILTER (WHERE gp.is_alive)                            AS survivals,
    COUNT(gp.id) FILTER (
        WHERE gp.role_start_id <> gp.role_end_id
    )                                                                   AS role_changed_count
FROM roles r
LEFT JOIN game_players gp ON gp.role_start_id = r.id
LEFT JOIN games         g  ON g.id              = gp.game_id
LEFT JOIN role_translations rt ON rt.role_id = r.id AND rt.lang_code = 'ru'
LEFT JOIN role_type_translations rtt ON rtt.role_type_en = r.role_type::TEXT AND rtt.lang_code = 'ru'
LEFT JOIN alignment_translations at ON at.alignment_en = r.alignment AND at.lang_code = 'ru';

-- Game session summary: high-level overview per game
CREATE VIEW v_game_summary AS
SELECT
    g.id                                                                AS game_uuid,
    g.game_date,
    g.game_number,
    g.scenario_name,
    g.location,
    g.duration,
    g.notes,
    st.name                                                             AS storyteller,
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
JOIN players       st ON st.id = g.storyteller_id
LEFT JOIN game_players gp ON gp.game_id = g.id
LEFT JOIN alignment_translations at ON at.alignment_en = g.alignment_win AND at.lang_code = 'ru'
GROUP BY g.id, g.game_date, g.game_number, g.scenario_name, g.location, g.duration, g.notes, st.name, g.alignment_win, at.name;

-- Role type effectiveness: aggregated win rates by role category and team
CREATE VIEW v_role_type_stats AS
SELECT
    r.role_type,
    r.alignment,
    rtt.name                                                            AS role_type_ru,
    at.name                                                             AS alignment_ru,
    COUNT(gp.id)                                                        AS times_played,
    ROUND(
        COUNT(gp.id) FILTER (WHERE gp.alignment_end = g.alignment_win)::numeric
        / NULLIF(COUNT(gp.id), 0) * 100, 1
    )                                                                   AS win_rate_pct
FROM game_players gp
JOIN roles r ON r.id  = gp.role_start_id
JOIN games g ON g.id  = gp.game_id
LEFT JOIN role_type_translations rtt ON rtt.role_type_en = r.role_type::TEXT AND rtt.lang_code = 'ru'
LEFT JOIN alignment_translations at ON at.alignment_en = r.alignment AND at.lang_code = 'ru'
GROUP BY r.role_type, r.alignment, rtt.name, at.name
ORDER BY r.alignment, r.role_type;
