#!/bin/bash

################################################################################
# WaitingTheLongest Database Initialization Script
#
# This script initializes the PostgreSQL database for WaitingTheLongest by:
# - Creating the database if it doesn't exist
# - Running all core migrations in order
# - Running all module migrations
# - Optionally resetting (dropping) the database first
#
# Usage:
#   ./scripts/init-db.sh              # Normal initialization
#   ./scripts/init-db.sh --reset      # Drop and recreate database
#   ./scripts/init-db.sh --help       # Show this help message
#
# Prerequisites:
#   - PostgreSQL client tools installed (psql)
#   - Database credentials set in environment or defaults
#   - SQL migration files in sql/core/ and sql/modules/
#
################################################################################

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration from environment or defaults
DB_HOST="${DB_HOST:-localhost}"
DB_PORT="${DB_PORT:-5432}"
DB_NAME="${DB_NAME:-waitingthelongest}"
DB_USER="${DB_USER:-postgres}"
DB_PASSWORD="${DB_PASSWORD:-}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SQL_CORE_DIR="${PROJECT_ROOT}/sql/core"
SQL_MODULES_DIR="${PROJECT_ROOT}/sql/modules"

# Flags
RESET_DB=false

################################################################################
# Functions
################################################################################

print_header() {
    echo -e "${BLUE}================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

show_help() {
    sed -n '1,/^################################################################################$/p' "$0" | grep "^#" | sed 's/^#//' | sed 's/^ //'
}

check_prerequisites() {
    print_header "Checking Prerequisites"

    if ! command -v psql &> /dev/null; then
        print_error "psql not found. Please install PostgreSQL client tools."
        exit 1
    fi
    print_success "psql is available"

    if [ ! -d "$SQL_CORE_DIR" ]; then
        print_error "SQL core directory not found: $SQL_CORE_DIR"
        exit 1
    fi
    print_success "SQL core directory found: $SQL_CORE_DIR"

    if [ ! -d "$SQL_MODULES_DIR" ]; then
        print_error "SQL modules directory not found: $SQL_MODULES_DIR"
        exit 1
    fi
    print_success "SQL modules directory found: $SQL_MODULES_DIR"

    echo ""
}

set_psql_connection_string() {
    # Build psql connection string
    PSQL_OPTS="-h $DB_HOST -p $DB_PORT -U $DB_USER"

    if [ -z "$DB_PASSWORD" ]; then
        # If no password, assume trust authentication or .pgpass
        export PGPASSWORD=""
    else
        export PGPASSWORD="$DB_PASSWORD"
    fi
}

check_database_connection() {
    print_info "Checking database connection..."

    if ! psql $PSQL_OPTS -d "postgres" -c "SELECT version();" > /dev/null 2>&1; then
        print_error "Cannot connect to PostgreSQL at $DB_HOST:$DB_PORT as $DB_USER"
        exit 1
    fi

    print_success "Database connection successful"
    echo ""
}

reset_database() {
    if [ "$RESET_DB" != true ]; then
        return
    fi

    print_header "Resetting Database"
    print_warning "Dropping database '$DB_NAME' (if it exists)..."

    # Terminate existing connections
    psql $PSQL_OPTS -d "postgres" <<EOF
SELECT pg_terminate_backend(pg_stat_activity.pid)
FROM pg_stat_activity
WHERE pg_stat_activity.datname = '$DB_NAME'
  AND pid <> pg_backend_pid();
EOF

    # Drop database if it exists
    psql $PSQL_OPTS -d "postgres" -c "DROP DATABASE IF EXISTS \"$DB_NAME\";" 2>/dev/null || true

    print_success "Database dropped"
    echo ""
}

create_database() {
    print_header "Creating Database"

    if psql $PSQL_OPTS -d "postgres" -tAc "SELECT 1 FROM pg_database WHERE datname = '$DB_NAME'" | grep -q 1; then
        print_info "Database '$DB_NAME' already exists"
    else
        print_info "Creating database '$DB_NAME'..."
        psql $PSQL_OPTS -d "postgres" -c "CREATE DATABASE \"$DB_NAME\" ENCODING 'UTF8' LC_COLLATE='C' LC_CTYPE='C';"
        print_success "Database created: $DB_NAME"
    fi

    echo ""
}

run_migrations() {
    local migration_dir=$1
    local migration_type=$2

    if [ ! -d "$migration_dir" ]; then
        print_warning "Migration directory not found: $migration_dir"
        return 0
    fi

    # Count migrations
    local migration_count=$(find "$migration_dir" -maxdepth 1 -name "*.sql" -type f | wc -l)

    if [ $migration_count -eq 0 ]; then
        print_info "No $migration_type migrations found"
        return 0
    fi

    print_header "Running $migration_type Migrations ($migration_count found)"

    local success_count=0
    local failure_count=0

    # Run migrations in sorted order (for core migrations, ensures numeric order)
    while IFS= read -r migration_file; do
        local migration_name=$(basename "$migration_file")

        if psql $PSQL_OPTS -d "$DB_NAME" -f "$migration_file" > /dev/null 2>&1; then
            print_success "Applied: $migration_name"
            ((success_count++))
        else
            print_error "Failed: $migration_name"
            ((failure_count++))
            # Continue with other migrations but track failure
        fi
    done < <(find "$migration_dir" -maxdepth 1 -name "*.sql" -type f | sort)

    echo ""
    print_info "$migration_type migrations: $success_count succeeded, $failure_count failed"

    if [ $failure_count -gt 0 ]; then
        print_error "Some $migration_type migrations failed"
        return 1
    fi

    return 0
}

print_summary() {
    local exit_code=$1

    echo ""
    print_header "Initialization Summary"

    if [ $exit_code -eq 0 ]; then
        print_success "Database initialization completed successfully!"
        echo ""
        print_info "Database: $DB_NAME"
        print_info "Host: $DB_HOST:$DB_PORT"
        print_info "User: $DB_USER"
        echo ""
        print_info "Next steps:"
        echo "  1. Verify data in database: psql -h $DB_HOST -p $DB_PORT -U $DB_USER -d $DB_NAME"
        echo "  2. Start the application"
        echo "  3. Run tests: npm test"
    else
        print_error "Database initialization failed"
        echo ""
        print_warning "Please check the errors above and try again"
    fi

    echo ""
}

################################################################################
# Main Script
################################################################################

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --reset)
            RESET_DB=true
            shift
            ;;
        --help|-h)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Run initialization
print_header "WaitingTheLongest Database Initialization"
echo ""

check_prerequisites
set_psql_connection_string
check_database_connection
reset_database
create_database

# Run migrations
run_migrations "$SQL_CORE_DIR" "Core"
CORE_RESULT=$?

run_migrations "$SQL_MODULES_DIR" "Module"
MODULE_RESULT=$?

# Print summary and exit with appropriate code
if [ $CORE_RESULT -eq 0 ] && [ $MODULE_RESULT -eq 0 ]; then
    print_summary 0
    exit 0
else
    print_summary 1
    exit 1
fi
