#!/usr/bin/env bash
# Compare two WPT log files and show differences

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
    cat <<EOF
Usage: $0 <baseline.log> <current.log>

Compare two WPT log files and show:
- Tests that now pass (regressions fixed)
- Tests that now fail (new regressions)
- Overall statistics

Arguments:
    baseline.log    Baseline WPT results
    current.log     Current WPT results to compare

Example:
    # Run baseline
    ./Meta/WPT.sh run --log baseline.log css

    # Make changes
    git checkout my-feature-branch

    # Run comparison
    ./Meta/WPT.sh compare --log current.log baseline.log css

    # Analyze differences
    $0 baseline.log current.log
EOF
    exit 1
}

if [ $# -ne 2 ]; then
    usage
fi

BASELINE="$1"
CURRENT="$2"

if [ ! -f "$BASELINE" ]; then
    echo -e "${RED}Error: Baseline file not found: $BASELINE${NC}"
    exit 1
fi

if [ ! -f "$CURRENT" ]; then
    echo -e "${RED}Error: Current file not found: $CURRENT${NC}"
    exit 1
fi

# Extract test results
extract_results() {
    local log_file="$1"

    # Extract PASS/FAIL lines
    grep -E "^(PASS|FAIL|ERROR|TIMEOUT|CRASH)" "$log_file" | sort || true
}

# Parse summary statistics
extract_stats() {
    local log_file="$1"

    # Find summary section
    grep -A 10 "Ran .* tests finished in" "$log_file" | \
        grep -E "(Ran [0-9]+ tests|ran as expected|tests skipped|tests.*errors|tests.*unexpected)" || true
}

echo -e "${BLUE}Comparing WPT results${NC}"
echo -e "Baseline: ${YELLOW}${BASELINE}${NC}"
echo -e "Current:  ${YELLOW}${CURRENT}${NC}"
echo ""

# Extract results
echo -e "${BLUE}Extracting test results...${NC}"
BASELINE_RESULTS=$(mktemp)
CURRENT_RESULTS=$(mktemp)

extract_results "$BASELINE" > "$BASELINE_RESULTS"
extract_results "$CURRENT" > "$CURRENT_RESULTS"

# Find differences
IMPROVEMENTS=$(mktemp)
REGRESSIONS=$(mktemp)

# Tests that now pass (were FAIL, now PASS)
grep "^FAIL" "$BASELINE_RESULTS" | cut -d' ' -f2- | sort > /tmp/baseline_fails
grep "^PASS" "$CURRENT_RESULTS" | cut -d' ' -f2- | sort > /tmp/current_passes
comm -12 /tmp/baseline_fails /tmp/current_passes > "$IMPROVEMENTS"

# Tests that now fail (were PASS, now FAIL/ERROR/TIMEOUT/CRASH)
grep "^PASS" "$BASELINE_RESULTS" | cut -d' ' -f2- | sort > /tmp/baseline_passes
grep -E "^(FAIL|ERROR|TIMEOUT|CRASH)" "$CURRENT_RESULTS" | cut -d' ' -f2- | sort > /tmp/current_fails
comm -12 /tmp/baseline_passes /tmp/current_fails > "$REGRESSIONS"

# Count differences
IMPROVEMENT_COUNT=$(wc -l < "$IMPROVEMENTS")
REGRESSION_COUNT=$(wc -l < "$REGRESSIONS")

# Display improvements
echo -e "${GREEN}Tests that now pass: ${IMPROVEMENT_COUNT}${NC}"
if [ "$IMPROVEMENT_COUNT" -gt 0 ]; then
    head -20 "$IMPROVEMENTS" | while read -r test; do
        echo -e "  ${GREEN}✓${NC} $test"
    done
    if [ "$IMPROVEMENT_COUNT" -gt 20 ]; then
        echo -e "  ${GREEN}... and $((IMPROVEMENT_COUNT - 20)) more${NC}"
    fi
fi
echo ""

# Display regressions
echo -e "${RED}Tests that now fail: ${REGRESSION_COUNT}${NC}"
if [ "$REGRESSION_COUNT" -gt 0 ]; then
    head -20 "$REGRESSIONS" | while read -r test; do
        echo -e "  ${RED}✗${NC} $test"
    done
    if [ "$REGRESSION_COUNT" -gt 20 ]; then
        echo -e "  ${RED}... and $((REGRESSION_COUNT - 20)) more${NC}"
    fi
fi
echo ""

# Display summary statistics
echo -e "${BLUE}Baseline summary:${NC}"
extract_stats "$BASELINE" | sed 's/^/  /'
echo ""

echo -e "${BLUE}Current summary:${NC}"
extract_stats "$CURRENT" | sed 's/^/  /'
echo ""

# Overall verdict
if [ "$REGRESSION_COUNT" -eq 0 ] && [ "$IMPROVEMENT_COUNT" -gt 0 ]; then
    echo -e "${GREEN}✓ No regressions, ${IMPROVEMENT_COUNT} improvements!${NC}"
    EXIT_CODE=0
elif [ "$REGRESSION_COUNT" -eq 0 ] && [ "$IMPROVEMENT_COUNT" -eq 0 ]; then
    echo -e "${YELLOW}No changes in test results${NC}"
    EXIT_CODE=0
elif [ "$REGRESSION_COUNT" -gt 0 ] && [ "$IMPROVEMENT_COUNT" -gt 0 ]; then
    echo -e "${YELLOW}Mixed results: ${IMPROVEMENT_COUNT} improvements, ${REGRESSION_COUNT} regressions${NC}"
    EXIT_CODE=1
else
    echo -e "${RED}✗ ${REGRESSION_COUNT} regressions detected${NC}"
    EXIT_CODE=1
fi

# Cleanup
rm -f "$BASELINE_RESULTS" "$CURRENT_RESULTS" "$IMPROVEMENTS" "$REGRESSIONS"
rm -f /tmp/baseline_fails /tmp/current_passes /tmp/baseline_passes /tmp/current_fails

exit $EXIT_CODE
