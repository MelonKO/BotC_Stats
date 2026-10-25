# ============================================================
#  BotC Stats — PostgreSQL image for the released stack
# ============================================================
#
# Published by the release workflow as
# ghcr.io/<owner>/botc-stats-db, so a server can initialise a fresh database
# without the checkout.
#
# The image carries init-scripts/ only. PostgreSQL executes
# /docker-entrypoint-initdb.d exactly once, when it creates an empty data
# directory, so only scripts that build a database from scratch belong here.
#
# Schema changes for an already-deployed database are migrations. They live in
# core/migrations/, are NOT part of this image and are applied explicitly with
# the db-migrate compose service (db/migrate.sh below), which runs them once
# each and records them in public.schema_migrations.
#
# Build from the core/ directory:
#   docker build -t botc-stats-db:local -f db/Dockerfile .
FROM postgres:16

# Fresh-database initialisation: schema, application users, grants.
COPY init-scripts/ /docker-entrypoint-initdb.d/

# One-shot migration runner used by the db-migrate compose service.
COPY db/migrate.sh /usr/local/bin/botc-migrate
RUN chmod 755 /usr/local/bin/botc-migrate
