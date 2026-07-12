-- ============================================================
--  BotC PostgreSQL — Grant privileges to application user
-- ============================================================

-- Grant CONNECT privilege on the database
GRANT CONNECT ON DATABASE botc_stats TO botc_user;

-- Grant USAGE on public schema
GRANT USAGE ON SCHEMA public TO botc_user;

-- Grant CRUD permissions on all tables
GRANT SELECT, INSERT, UPDATE, DELETE ON ALL TABLES IN SCHEMA public TO botc_user;

-- Grant USAGE on all sequences (for auto-increment IDs)
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO botc_user;

-- Set default privileges for future tables
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT, INSERT, UPDATE, DELETE ON TABLES TO botc_user;

ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT USAGE ON SEQUENCES TO botc_user;

-- Revoke unnecessary privileges from public
REVOKE CREATE ON SCHEMA public FROM PUBLIC;
REVOKE CREATE ON DATABASE botc_stats FROM PUBLIC;

-- ============================================================
--  API service role — limited to importing games via API
--  (Role api_service is created in 02-create-user.sh
--   with password from API_SERVICE_PASSWORD env variable)
-- ============================================================

GRANT CONNECT ON DATABASE botc_stats TO api_service;
GRANT USAGE ON SCHEMA public TO api_service;

-- API service needs full write access for the import process:
-- 1. Write to staging table (INSERT, DELETE for TRUNCATE)
GRANT INSERT, SELECT, DELETE ON games_import_staging TO api_service;

-- 2. Read and manage roles (add new roles, read for validation)
GRANT SELECT, INSERT, UPDATE ON roles TO api_service;

-- 2b. Read/write translation tables (for role import with translations)
GRANT INSERT, UPDATE, SELECT ON role_translations TO api_service;
GRANT SELECT ON role_type_translations TO api_service;
GRANT SELECT ON alignment_translations TO api_service;
GRANT SELECT ON languages TO api_service;

-- 3. Read reference data, create players if missing
GRANT SELECT, INSERT ON players TO api_service;

-- 4. Write to game tables (function inserts games + game_players + game_storytellers)
GRANT INSERT, SELECT ON games TO api_service;
GRANT INSERT, SELECT ON game_players TO api_service;
GRANT INSERT, SELECT ON game_storytellers TO api_service;

-- 5. Read API keys (for authentication)
GRANT SELECT, UPDATE (last_used_at) ON api_keys TO api_service;

-- 5. Sequences for auto-generated UUIDs
GRANT USAGE ON ALL SEQUENCES IN SCHEMA public TO api_service;

-- 6. Execute the import function (SECURITY DEFINER, runs as postgres)
GRANT EXECUTE ON FUNCTION process_games_import() TO api_service;

-- 7. Read-only access to analytics views
GRANT SELECT ON v_player_stats TO api_service;
GRANT SELECT ON v_role_stats TO api_service;
GRANT SELECT ON v_game_summary TO api_service;
GRANT SELECT ON v_role_type_stats TO api_service;

-- 8. Default privileges for future tables (API service gets read-only)
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO api_service;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT USAGE ON SEQUENCES TO api_service;
