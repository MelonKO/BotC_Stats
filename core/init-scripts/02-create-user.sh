#!/bin/bash
# ============================================================
#  BotC PostgreSQL — Create application users
# ============================================================

set -e

# Create application user with limited privileges
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<EOF
CREATE USER botc_user WITH PASSWORD '${BOTC_USER_PASSWORD}';
CREATE ROLE api_service WITH LOGIN PASSWORD '${API_SERVICE_PASSWORD}';
EOF

echo "Users botc_user and api_service created successfully"
