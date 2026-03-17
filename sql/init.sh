#!/bin/bash
set -e

echo "=== WaitingTheLongest Database Initialization ==="
echo "Running core migrations..."

# Core migrations in order
for f in /docker-entrypoint-initdb.d/01-core/*.sql; do
    if [ -f "$f" ]; then
        echo "  Running: $(basename $f)"
        psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f "$f"
    fi
done

echo "Running module migrations..."

MODULE_DIR="/docker-entrypoint-initdb.d/02-modules"

# Run sponsorship_schema first (adoption_support depends on it)
if [ -f "$MODULE_DIR/sponsorship_schema.sql" ]; then
    echo "  Running: sponsorship_schema.sql (dependency)"
    psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f "$MODULE_DIR/sponsorship_schema.sql"
fi

# Run remaining module migrations alphabetically, skipping sponsorship
for f in $MODULE_DIR/*.sql; do
    if [ -f "$f" ] && [ "$(basename $f)" != "sponsorship_schema.sql" ]; then
        echo "  Running: $(basename $f)"
        psql -v ON_ERROR_STOP=1 --username "$POSTGRES_USER" --dbname "$POSTGRES_DB" -f "$f"
    fi
done

echo "=== Database initialization complete ==="
