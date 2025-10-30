#!/bin/bash
# Day 29 Security Fixes Validation Script
# Validates that all security fixes are properly implemented

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo "=========================================="
echo " Day 29 Security Fixes Validation"
echo " Date: $(date +%Y-%m-%d)"
echo "=========================================="
echo ""

PASS_COUNT=0
FAIL_COUNT=0
TOTAL_COUNT=0

# Function to check if a function exists in a file
check_function_exists() {
    local file="$1"
    local function_name="$2"
    local description="$3"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    if grep -q "^static.*${function_name}" "$file" || grep -q "${function_name}(" "$file"; then
        echo -e "${GREEN}✓${NC} ${description}: ${function_name} exists in $(basename $file)"
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    else
        echo -e "${RED}✗${NC} ${description}: ${function_name} NOT FOUND in $(basename $file)"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi
}

# Function to check if a function is called in another function
check_function_called() {
    local file="$1"
    local caller_function="$2"
    local called_function="$3"
    local description="$4"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    # Extract the caller function body and check if it calls the validation function
    if awk "/^.*${caller_function}\(/,/^}/" "$file" | grep -q "${called_function}"; then
        echo -e "${GREEN}✓${NC} ${description}: ${caller_function}() calls ${called_function}()"
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    else
        echo -e "${RED}✗${NC} ${description}: ${caller_function}() does NOT call ${called_function}()"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi
}

# Function to check if ESCAPE clause is used in SQL
check_sql_escape() {
    local file="$1"
    local description="$2"

    TOTAL_COUNT=$((TOTAL_COUNT + 1))

    if grep -q "ESCAPE" "$file"; then
        echo -e "${GREEN}✓${NC} ${description}: ESCAPE clause found in SQL queries"
        PASS_COUNT=$((PASS_COUNT + 1))
        return 0
    else
        echo -e "${RED}✗${NC} ${description}: ESCAPE clause NOT FOUND"
        FAIL_COUNT=$((FAIL_COUNT + 1))
        return 1
    fi
}

echo -e "${BLUE}=== Task 1: SQL Injection Fix in PolicyGraph ===${NC}"
echo ""

POLICY_GRAPH="/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.cpp"

check_function_exists "$POLICY_GRAPH" "is_safe_url_pattern" "Task 1.1"
check_function_exists "$POLICY_GRAPH" "validate_policy_inputs" "Task 1.2"
check_function_called "$POLICY_GRAPH" "create_policy" "validate_policy_inputs" "Task 1.3"
check_function_called "$POLICY_GRAPH" "update_policy" "validate_policy_inputs" "Task 1.4"
check_sql_escape "$POLICY_GRAPH" "Task 1.5"

echo ""
echo -e "${BLUE}=== Task 2: Arbitrary File Read Fix in SentinelServer ===${NC}"
echo ""

SENTINEL_SERVER="/home/rbsmith4/ladybird/Services/Sentinel/SentinelServer.cpp"

check_function_exists "$SENTINEL_SERVER" "validate_scan_path" "Task 2.1"
check_function_called "$SENTINEL_SERVER" "scan_file" "validate_scan_path" "Task 2.2"

# Check for required headers
TOTAL_COUNT=$((TOTAL_COUNT + 1))
if grep -q "#include.*sys/stat" "$SENTINEL_SERVER"; then
    echo -e "${GREEN}✓${NC} Task 2.3: sys/stat.h included for lstat()"
    PASS_COUNT=$((PASS_COUNT + 1))
else
    echo -e "${RED}✗${NC} Task 2.3: sys/stat.h NOT included"
    FAIL_COUNT=$((FAIL_COUNT + 1))
fi

echo ""
echo -e "${BLUE}=== Task 3: Path Traversal Fix in Quarantine ===${NC}"
echo ""

QUARANTINE="/home/rbsmith4/ladybird/Services/RequestServer/Quarantine.cpp"

check_function_exists "$QUARANTINE" "validate_restore_destination" "Task 3.1"
check_function_exists "$QUARANTINE" "sanitize_filename" "Task 3.2"
check_function_called "$QUARANTINE" "restore_file" "validate_restore_destination" "Task 3.3"
check_function_called "$QUARANTINE" "restore_file" "sanitize_filename" "Task 3.4"

echo ""
echo -e "${BLUE}=== Task 4: Quarantine ID Validation ===${NC}"
echo ""

check_function_exists "$QUARANTINE" "is_valid_quarantine_id" "Task 4.1"
check_function_called "$QUARANTINE" "read_metadata" "is_valid_quarantine_id" "Task 4.2"
check_function_called "$QUARANTINE" "restore_file" "is_valid_quarantine_id" "Task 4.3"
check_function_called "$QUARANTINE" "delete_file" "is_valid_quarantine_id" "Task 4.4"

echo ""
echo "=========================================="
echo " Validation Summary"
echo "=========================================="
echo -e "Total checks: ${TOTAL_COUNT}"
echo -e "Passed: ${GREEN}${PASS_COUNT}${NC}"
echo -e "Failed: ${RED}${FAIL_COUNT}${NC}"
echo ""

if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "${GREEN}✓ All security fixes validated successfully!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some security fixes are missing or incomplete${NC}"
    exit 1
fi
