#!/bin/bash
# ============================================================
#  BotC PostgreSQL — Create application user
# ============================================================

set -e

# Create application user with limited privileges
# Password comes from BOTC_USER_PASSWORD environment variable
psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" <<EOF
CREATE USER botc_user WITH PASSWORD '${BOTC_USER_PASSWORD}';
EOF

echo "User botc_user created successfully"
