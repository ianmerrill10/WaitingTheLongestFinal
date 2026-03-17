#!/bin/bash
# WaitingTheLongest - Basic Load Test
# Requires: curl, bash
# For serious load testing, use k6, ab, or wrk

set -euo pipefail

BASE_URL="${1:-http://localhost}"
CONCURRENT="${2:-10}"
TOTAL="${3:-100}"

echo "============================================="
echo " WaitingTheLongest Load Test"
echo " URL: $BASE_URL"
echo " Concurrent: $CONCURRENT"
echo " Total requests: $TOTAL"
echo "============================================="

ENDPOINTS=(
    "/api/health"
    "/api/dogs?page=1&limit=10"
    "/api/states"
    "/health-check"
)

for endpoint in "${ENDPOINTS[@]}"; do
    echo ""
    echo "Testing: $endpoint"
    echo "---"

    start_time=$(date +%s%N)
    success=0
    fail=0

    for i in $(seq 1 $TOTAL); do
        status=$(curl -s -o /dev/null -w "%{http_code}" "$BASE_URL$endpoint" 2>/dev/null || echo "000")
        if [ "$status" = "200" ]; then
            ((success++))
        else
            ((fail++))
        fi
    done

    end_time=$(date +%s%N)
    elapsed=$(( (end_time - start_time) / 1000000 ))
    rps=$(( TOTAL * 1000 / (elapsed + 1) ))

    echo "  Requests: $TOTAL | Success: $success | Failed: $fail"
    echo "  Time: ${elapsed}ms | RPS: ~$rps"
done

echo ""
echo "Load test complete."
