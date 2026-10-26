-- Allow the API service to rename players: PATCH /api/players/{playerId}.
--
-- Column-scoped on purpose. The import service only ever changes a name; the
-- contact fields (telegram, vk, phone, email, notes) and DELETE stay out of its
-- reach. Same statement as core/init-scripts/03-grant-privileges.sql, which only
-- runs on a fresh data volume.
GRANT UPDATE (name) ON players TO api_service;
