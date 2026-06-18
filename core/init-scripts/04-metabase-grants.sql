-- ============================================================
--  BotC PostgreSQL — Metabase read-only access
--  metabase_readonly has SELECT on all tables and views in botc_stats.
-- ============================================================

GRANT CONNECT ON DATABASE botc_stats TO metabase_readonly;
GRANT USAGE ON SCHEMA public TO metabase_readonly;

-- Full read access to all existing tables and views
GRANT SELECT ON ALL TABLES IN SCHEMA public TO metabase_readonly;

-- Read access to future tables created after this script runs
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO metabase_readonly;
