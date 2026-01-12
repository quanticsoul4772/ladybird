#!/usr/bin/env bash
# Run specific Web Platform Test subsets with commonly used configurations

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LADYBIRD_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    cat <<EOF
Usage: $0 <category> [options]

Run Web Platform Tests for specific categories with sensible defaults.

Categories:
    dom         - DOM API tests
    html        - HTML parsing and APIs
    css         - CSS rendering and computed styles
    fetch       - Fetch API and network
    websockets  - WebSocket tests
    workers     - Web Workers
    all         - Run all WPT tests

Options:
    --baseline         Save results as baseline for future comparison
    --compare FILE     Compare against baseline file
    --parallel N       Run with N parallel instances (0=auto, default=1)
    --show-window      Show browser window (not headless)
    --debug            Attach debugger to WebContent process
    --help             Show this help

Examples:
    # Run DOM tests and save baseline
    $0 dom --baseline

    # Run CSS tests comparing against baseline
    $0 css --compare baseline-css.log

    # Run HTML tests with auto-parallelism
    $0 html --parallel 0

    # Run fetch tests with visible browser
    $0 fetch --show-window

    # Run all tests with 4 parallel instances
    $0 all --parallel 4
EOF
    exit 1
}

# Default values
CATEGORY=""
BASELINE_MODE=false
COMPARE_FILE=""
PARALLEL_INSTANCES=1
EXTRA_ARGS=()

# Parse arguments
if [ $# -eq 0 ]; then
    usage
fi

CATEGORY="$1"
shift

while [[ $# -gt 0 ]]; do
    case "$1" in
        --baseline)
            BASELINE_MODE=true
            shift
            ;;
        --compare)
            COMPARE_FILE="$2"
            shift 2
            ;;
        --parallel)
            PARALLEL_INSTANCES="$2"
            shift 2
            ;;
        --show-window)
            EXTRA_ARGS+=("--show-window")
            shift
            ;;
        --debug)
            EXTRA_ARGS+=("--debug-process" "WebContent")
            shift
            ;;
        --help)
            usage
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            usage
            ;;
    esac
done

# Map category to WPT path
case "$CATEGORY" in
    dom)
        TEST_PATH="dom"
        ;;
    html)
        TEST_PATH="html"
        ;;
    css)
        TEST_PATH="css"
        ;;
    fetch)
        TEST_PATH="fetch"
        ;;
    websockets)
        TEST_PATH="websockets"
        ;;
    workers)
        TEST_PATH="workers"
        ;;
    all)
        TEST_PATH=""
        ;;
    *)
        echo -e "${RED}Error: Unknown category '$CATEGORY'${NC}"
        usage
        ;;
esac

# Change to Ladybird root
cd "$LADYBIRD_ROOT"

# Ensure WPT is set up
if [ ! -d "Tests/LibWeb/WPT/wpt" ]; then
    echo -e "${YELLOW}WPT repository not found. Updating...${NC}"
    ./Meta/WPT.sh update
fi

# Generate log filename
TIMESTAMP=$(date +"%Y%m%d-%H%M%S")
LOG_FILE="${LADYBIRD_ROOT}/wpt-${CATEGORY}-${TIMESTAMP}.log"

if [ "$BASELINE_MODE" = true ]; then
    LOG_FILE="${LADYBIRD_ROOT}/baseline-${CATEGORY}-${TIMESTAMP}.log"
fi

# Build command
WPT_CMD=("./Meta/WPT.sh")

if [ -n "$COMPARE_FILE" ]; then
    echo -e "${GREEN}Running WPT comparison mode${NC}"
    echo -e "Category: ${YELLOW}${CATEGORY}${NC}"
    echo -e "Baseline: ${YELLOW}${COMPARE_FILE}${NC}"
    echo -e "Results:  ${YELLOW}${LOG_FILE}${NC}"
    echo -e "Parallel: ${YELLOW}${PARALLEL_INSTANCES}${NC}"
    echo ""

    WPT_CMD+=("compare" "$COMPARE_FILE")
else
    echo -e "${GREEN}Running WPT tests${NC}"
    echo -e "Category: ${YELLOW}${CATEGORY}${NC}"
    echo -e "Log file: ${YELLOW}${LOG_FILE}${NC}"
    echo -e "Parallel: ${YELLOW}${PARALLEL_INSTANCES}${NC}"
    if [ "$BASELINE_MODE" = true ]; then
        echo -e "Mode:     ${YELLOW}Baseline${NC}"
    fi
    echo ""

    WPT_CMD+=("run")
fi

WPT_CMD+=("--log" "$LOG_FILE")
WPT_CMD+=("--parallel-instances" "$PARALLEL_INSTANCES")
WPT_CMD+=("${EXTRA_ARGS[@]}")

if [ -n "$TEST_PATH" ]; then
    WPT_CMD+=("$TEST_PATH")
fi

# Run WPT
echo -e "${GREEN}Command: ${WPT_CMD[*]}${NC}"
echo ""

"${WPT_CMD[@]}"

# Summary
echo ""
echo -e "${GREEN}WPT run completed${NC}"
echo -e "Results saved to: ${YELLOW}${LOG_FILE}${NC}"

if [ "$BASELINE_MODE" = true ]; then
    echo ""
    echo -e "${GREEN}To compare future runs against this baseline:${NC}"
    echo -e "  $0 ${CATEGORY} --compare ${LOG_FILE}"
fi
