-- ============================================================
--  BotC PostgreSQL — Metabase read-only access
--  metabase_readonly connects to botc_stats and reads only
--  the analytics views — no access to raw tables.
-- ============================================================

GRANT CONNECT ON DATABASE botc_stats TO metabase_readonly;
GRANT USAGE ON SCHEMA public TO metabase_readonly;

-- Analytics views only — Metabase cannot see raw game/player tables
GRANT SELECT ON v_player_stats    TO metabase_readonly;
GRANT SELECT ON v_role_stats      TO metabase_readonly;
GRANT SELECT ON v_game_summary    TO metabase_readonly;
GRANT SELECT ON v_role_type_stats TO metabase_readonly;
