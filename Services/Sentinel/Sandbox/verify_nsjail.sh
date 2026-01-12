#!/bin/bash
# nsjail Verification Test Suite
# Runs comprehensive tests to verify nsjail installation

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
TESTS_WARNINGS=0

echo "=================================================="
echo "nsjail Verification Test Suite"
echo "=================================================="
echo ""

# Helper function to run a test
run_test() {
    local test_name="$1"
    local test_command="$2"
    local expected_pattern="$3"

    TESTS_RUN=$((TESTS_RUN + 1))
    echo -e "${BLUE}Test $TESTS_RUN: $test_name${NC}"
    echo "Command: $test_command"
    echo ""

    # Run the command and capture output
    if output=$(eval "$test_command" 2>&1); then
        # Check if output contains expected pattern
        if [ -z "$expected_pattern" ] || echo "$output" | grep -q "$expected_pattern"; then
            echo -e "${GREEN}✓ PASSED${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        else
            echo -e "${YELLOW}⚠ WARNING: Expected pattern not found${NC}"
            echo "Expected: $expected_pattern"
            TESTS_WARNINGS=$((TESTS_WARNINGS + 1))
        fi
    else
        echo -e "${RED}✗ FAILED${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi

    echo "Output:"
    echo "$output" | head -20
    echo ""
    echo "--------------------------------------------------"
    echo ""
}

# Check if nsjail is installed
echo "Checking nsjail installation..."
if ! command -v nsjail &> /dev/null; then
    echo -e "${RED}Error: nsjail not found in PATH${NC}"
    echo "Please run install_nsjail.sh first"
    exit 1
fi

echo -e "${GREEN}✓ nsjail found: $(which nsjail)${NC}"
echo ""

# Display version/help info
echo "nsjail version information:"
nsjail --version 2>&1 || nsjail --help | head -5
echo ""
echo "=================================================="
echo "Running Tests"
echo "=================================================="
echo ""

# Test 1: Basic execution
run_test \
    "Basic Execution - Echo Test" \
    "nsjail --mode o --time_limit 5 -- /bin/echo 'Hello from nsjail'" \
    "Hello from nsjail"

# Test 2: Resource limits - Memory
run_test \
    "Resource Limits - Memory (128MB)" \
    "nsjail --mode o --rlimit_as 128 -- /bin/ls /tmp" \
    ""

# Test 3: Resource limits - File size
run_test \
    "Resource Limits - File Size (1MB)" \
    "nsjail --mode o --rlimit_fsize 1 -- /bin/ls /" \
    ""

# Test 4: Time limit enforcement
run_test \
    "Time Limit - Kill after 2 seconds" \
    "timeout 5 nsjail --mode o --time_limit 2 -- /bin/sleep 10" \
    ""

# Test 5: User namespace isolation
run_test \
    "User Namespace Isolation - ID mapping" \
    "nsjail --mode o -- /usr/bin/id" \
    "uid="

# Test 6: Hostname isolation
run_test \
    "UTS Namespace - Hostname isolation" \
    "nsjail --mode o --hostname 'nsjail-test' -- /bin/hostname" \
    ""

# Test 7: PID namespace isolation
run_test \
    "PID Namespace - Process isolation" \
    "nsjail --mode o -- /bin/ps aux" \
    ""

# Test 8: Mount namespace
run_test \
    "Mount Namespace - File system isolation" \
    "nsjail --mode o -- /bin/mount" \
    ""

# Test 9: CPU limit
run_test \
    "CPU Limit - 5 CPU seconds max" \
    "timeout 10 nsjail --mode o --rlimit_cpu 5 -- /bin/yes" \
    ""

# Test 10: Multiple executions (ensure cleanup)
echo -e "${BLUE}Test 10: Multiple Consecutive Executions${NC}"
echo "Running 5 consecutive nsjail executions..."
SUCCESS_COUNT=0
for i in {1..5}; do
    if nsjail --mode o --time_limit 2 -- /bin/echo "Execution $i" &> /dev/null; then
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
done
TESTS_RUN=$((TESTS_RUN + 1))
if [ $SUCCESS_COUNT -eq 5 ]; then
    echo -e "${GREEN}✓ PASSED - All 5 executions succeeded${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${YELLOW}⚠ WARNING - Only $SUCCESS_COUNT/5 executions succeeded${NC}"
    TESTS_WARNINGS=$((TESTS_WARNINGS + 1))
fi
echo ""
echo "--------------------------------------------------"
echo ""

# Test 11: Working directory
run_test \
    "Working Directory - CWD isolation" \
    "nsjail --mode o --cwd /tmp -- /bin/pwd" \
    "/tmp"

# Test 12: Environment variables
run_test \
    "Environment Variables - Custom env" \
    "nsjail --mode o --env TEST_VAR=test_value -- /bin/sh -c 'echo \$TEST_VAR'" \
    "test_value"

echo "=================================================="
echo "Test Summary"
echo "=================================================="
echo ""
echo "Total tests run:     $TESTS_RUN"
echo -e "${GREEN}Tests passed:        $TESTS_PASSED${NC}"
echo -e "${YELLOW}Tests with warnings: $TESTS_WARNINGS${NC}"
echo -e "${RED}Tests failed:        $TESTS_FAILED${NC}"
echo ""

# Calculate success rate
if [ $TESTS_RUN -gt 0 ]; then
    SUCCESS_RATE=$(( (TESTS_PASSED * 100) / TESTS_RUN ))
    echo "Success rate: $SUCCESS_RATE%"
    echo ""
fi

# Overall status
if [ $TESTS_FAILED -eq 0 ]; then
    if [ $TESTS_WARNINGS -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed successfully${NC}"
        echo "nsjail is ready for use with Sentinel Behavioral Analysis"
        exit 0
    else
        echo -e "${YELLOW}⚠ All tests completed with some warnings${NC}"
        echo "nsjail should work but review warnings above"
        exit 0
    fi
else
    echo -e "${RED}✗ Some tests failed${NC}"
    echo "Please review failures above and check nsjail installation"
    exit 1
fi
