#!/bin/bash
# WaitingTheLongest.com - Deployment Validation Script
# Run before deploying to production to check configuration

set -euo pipefail

PASS=0
FAIL=0
WARN=0

check_pass() { echo "  [PASS] $1"; ((PASS++)); }
check_fail() { echo "  [FAIL] $1"; ((FAIL++)); }
check_warn() { echo "  [WARN] $1"; ((WARN++)); }

echo "=========================================="
echo " WaitingTheLongest Deployment Validator"
echo "=========================================="
echo ""

# 1. Check .env exists
echo "--- Environment ---"
if [ -f .env ]; then
    check_pass ".env file exists"
else
    check_fail ".env file missing"
fi

# 2. Check DB password is not default
if [ -f .env ]; then
    DB_PASS=$(grep -E '^DB_PASSWORD=' .env | cut -d= -f2-)
    if [ "${DB_PASS}" = "postgres" ] || [ -z "${DB_PASS}" ]; then
        check_fail "DB_PASSWORD is default or empty - change it!"
    else
        check_pass "DB_PASSWORD is set to non-default value"
    fi
fi

# 3. Check JWT secret
if [ -f .env ]; then
    JWT=$(grep -E '^JWT_SECRET=' .env | cut -d= -f2-)
    if [ -z "${JWT}" ] || [ "${JWT}" = "CHANGE_ME_IN_PRODUCTION_USE_64_CHAR_RANDOM_STRING" ]; then
        check_fail "JWT_SECRET is not configured"
    elif [ ${#JWT} -lt 32 ]; then
        check_warn "JWT_SECRET is shorter than 32 chars"
    else
        check_pass "JWT_SECRET is configured (${#JWT} chars)"
    fi
fi

# 4. Check Redis password
if [ -f .env ]; then
    REDIS_PASS=$(grep -E '^REDIS_PASSWORD=' .env | cut -d= -f2-)
    if [ -z "${REDIS_PASS}" ] || [ "${REDIS_PASS}" = "changeme" ]; then
        check_fail "REDIS_PASSWORD is not configured"
    else
        check_pass "REDIS_PASSWORD is configured"
    fi
fi

# 5. Check Docker is available
echo ""
echo "--- Docker ---"
if command -v docker &> /dev/null; then
    check_pass "Docker is installed"
else
    check_fail "Docker is not installed"
fi

if command -v docker-compose &> /dev/null || docker compose version &> /dev/null 2>&1; then
    check_pass "Docker Compose is available"
else
    check_fail "Docker Compose is not available"
fi

# 6. Check required files exist
echo ""
echo "--- Required Files ---"
for f in docker-compose.yml Dockerfile nginx/nginx.conf nginx/conf.d/default.conf sql/init.sh config/config.json; do
    if [ -f "$f" ]; then
        check_pass "$f exists"
    else
        check_fail "$f is missing"
    fi
done

# 7. Check SSL config
echo ""
echo "--- SSL/TLS ---"
if [ -f nginx/conf.d/default.conf.production ]; then
    check_pass "Production SSL nginx config exists"
else
    check_warn "No production SSL nginx config found"
fi

if [ -d certbot/conf/live ]; then
    check_pass "SSL certificates directory exists"
else
    check_warn "No SSL certificates found (run certbot after DNS setup)"
fi

# 8. Check disk space
echo ""
echo "--- System ---"
DISK_AVAIL=$(df -h . | tail -1 | awk '{print $4}')
check_pass "Available disk space: ${DISK_AVAIL}"

# Summary
echo ""
echo "=========================================="
echo " Results: ${PASS} passed, ${FAIL} failed, ${WARN} warnings"
echo "=========================================="

if [ ${FAIL} -gt 0 ]; then
    echo ""
    echo "FIX THE FAILURES BEFORE DEPLOYING!"
    exit 1
else
    echo ""
    echo "Ready for deployment."
    exit 0
fi
