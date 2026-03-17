#!/bin/bash
# WaitingTheLongest.com - API Smoke Test
# Tests all known API endpoints and reports results

set -euo pipefail

BASE_URL="${1:-http://localhost:8080}"
PASS=0
FAIL=0

test_endpoint() {
    local method="$1"
    local path="$2"
    local expected_status="${3:-200}"
    local url="${BASE_URL}${path}"

    HTTP_STATUS=$(curl -s -o /dev/null -w "%{http_code}" -X "${method}" "${url}" --max-time 10 2>/dev/null || echo "000")

    if [ "${HTTP_STATUS}" = "${expected_status}" ]; then
        echo "  [PASS] ${method} ${path} -> ${HTTP_STATUS}"
        ((PASS++))
    else
        echo "  [FAIL] ${method} ${path} -> ${HTTP_STATUS} (expected ${expected_status})"
        ((FAIL++))
    fi
}

echo "=========================================="
echo " WaitingTheLongest API Smoke Test"
echo " Target: ${BASE_URL}"
echo "=========================================="
echo ""

# Health
echo "--- Health ---"
test_endpoint GET /api/health

# Core API
echo ""
echo "--- Core API ---"
test_endpoint GET /api/dogs
test_endpoint GET /api/dogs/urgent
test_endpoint GET /api/dogs/longest-waiting
test_endpoint GET /api/shelters
test_endpoint GET /api/states

# Search
echo ""
echo "--- Search ---"
test_endpoint GET /api/search/dogs
test_endpoint GET /api/search/breeds

# Urgency
echo ""
echo "--- Urgency ---"
test_endpoint GET /api/urgency/critical
test_endpoint GET /api/urgency/high

# Analytics
echo ""
echo "--- Analytics ---"
test_endpoint GET /api/analytics/dashboard
test_endpoint GET /api/analytics/realtime

# Content
echo ""
echo "--- Content ---"
test_endpoint GET /api/tiktok/analytics

# Resources
echo ""
echo "--- Resources ---"
test_endpoint GET /api/v1/resources
test_endpoint GET /api/v1/transport/requests

# Frontend Pages
echo ""
echo "--- Frontend Pages ---"
test_endpoint GET /
test_endpoint GET /dogs.html
test_endpoint GET /dog.html
test_endpoint GET /states.html
test_endpoint GET /state.html
test_endpoint GET /search.html
test_endpoint GET /quiz.html
test_endpoint GET /admin/

# Static Assets
echo ""
echo "--- Static Assets ---"
test_endpoint GET /css/main.css
test_endpoint GET /css/led-counter.css

# Error Pages
echo ""
echo "--- Error Pages ---"
test_endpoint GET /nonexistent-page-test 404

# Summary
echo ""
echo "=========================================="
echo " Results: ${PASS} passed, ${FAIL} failed"
echo "=========================================="

if [ ${FAIL} -gt 0 ]; then
    exit 1
else
    echo "All smoke tests passed!"
    exit 0
fi
