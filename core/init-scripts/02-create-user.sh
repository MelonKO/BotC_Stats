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

# Create Metabase users and metadata database
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" <<EOF
CREATE USER metabase_meta_user WITH PASSWORD '${METABASE_META_PASSWORD}';
CREATE USER metabase_readonly WITH PASSWORD '${METABASE_READONLY_PASSWORD}';
CREATE DATABASE metabase OWNER metabase_meta_user;
EOF

echo "Users botc_user, api_service, metabase_meta_user and metabase_readonly created successfully"
