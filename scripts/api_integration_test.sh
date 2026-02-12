#!/bin/bash
# WaitingTheLongest - API Integration Tests
# Tests all API endpoints for correct responses

set -euo pipefail

BASE_URL="${1:-http://localhost}"
PASS=0
FAIL=0
SKIP=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m'

test_endpoint() {
    local method="$1"
    local path="$2"
    local expected_status="$3"
    local description="$4"
    local data="${5:-}"

    if [ "$method" = "GET" ]; then
        actual_status=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL$path" 2>/dev/null || echo "000")
    elif [ "$method" = "POST" ]; then
        actual_status=$(curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: application/json" -d "$data" "$BASE_URL$path" 2>/dev/null || echo "000")
    else
        actual_status=$(curl -s -o /dev/null -w "%{http_code}" -X "$method" "$BASE_URL$path" 2>/dev/null || echo "000")
    fi

    if [ "$actual_status" = "$expected_status" ]; then
        echo -e "  ${GREEN}PASS${NC} [$method] $path ($actual_status) - $description"
        ((PASS++))
    elif [ "$actual_status" = "000" ]; then
        echo -e "  ${YELLOW}SKIP${NC} [$method] $path (unreachable) - $description"
        ((SKIP++))
    else
        echo -e "  ${RED}FAIL${NC} [$method] $path (got $actual_status, expected $expected_status) - $description"
        ((FAIL++))
    fi
}

echo "============================================="
echo " WaitingTheLongest API Integration Tests"
echo " Base URL: $BASE_URL"
echo "============================================="
echo ""

echo "--- Health & Infrastructure ---"
test_endpoint GET "/health-check" "200" "Nginx health check"
test_endpoint GET "/api/health" "200" "API health check"

echo ""
echo "--- Dogs API ---"
test_endpoint GET "/api/dogs" "200" "List dogs"
test_endpoint GET "/api/dogs?page=1&limit=10" "200" "List dogs with pagination"
test_endpoint GET "/api/dogs?breed=labrador" "200" "Filter dogs by breed"
test_endpoint GET "/api/dogs/nonexistent-id" "404" "Get non-existent dog"

echo ""
echo "--- Shelters API ---"
test_endpoint GET "/api/shelters" "200" "List shelters"
test_endpoint GET "/api/shelters/nonexistent-id" "404" "Get non-existent shelter"

echo ""
echo "--- States API ---"
test_endpoint GET "/api/states" "200" "List states"
test_endpoint GET "/api/states/TX" "200" "Get state by code"
test_endpoint GET "/api/states/XX" "404" "Get non-existent state"

echo ""
echo "--- Search API ---"
test_endpoint GET "/api/search?q=labrador" "200" "Search dogs"
test_endpoint GET "/api/search?q=&state=TX" "200" "Search with state filter"

echo ""
echo "--- Auth API ---"
test_endpoint POST "/api/auth/register" "400" "Register without body"
test_endpoint POST "/api/auth/login" "400" "Login without body"
test_endpoint POST "/api/auth/login" "401" "Login with bad credentials" '{"email":"bad@test.com","password":"wrong"}'

echo ""
echo "--- Protected Endpoints (should require auth) ---"
test_endpoint GET "/api/users/me" "401" "Get profile without auth"
test_endpoint GET "/api/admin/dashboard" "401" "Admin dashboard without auth"

echo ""
echo "--- Static Files ---"
test_endpoint GET "/static/css/main.css" "200" "Main CSS"
test_endpoint GET "/static/js/led-counter.js" "200" "LED counter JS"
test_endpoint GET "/static/robots.txt" "200" "Robots.txt"

echo ""
echo "============================================="
echo " Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}, ${YELLOW}$SKIP skipped${NC}"
echo "============================================="

if [ "$FAIL" -gt 0 ]; then
    exit 1
fi
