-- Allow the API service to delete a role translation through
-- PATCH /api/roles/{roleId}. Deleting roles themselves remains prohibited.
GRANT DELETE ON role_translations TO api_service;
