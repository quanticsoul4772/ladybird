#!/usr/bin/env bash
# Run Ladybird with Valgrind Memcheck for detailed memory error detection

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_DIR="$REPO_ROOT/Build/release"
BINARY="Ladybird"
LOG_FILE="valgrind_$(date +%Y%m%d_%H%M%S).log"
TOOL="memcheck"
TRACK_ORIGINS="yes"
SHOW_LEAK_KINDS="all"
EXTRA_ARGS=()

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] [BINARY] [-- BINARY_ARGS]

Run Ladybird with Valgrind for memory error detection

OPTIONS:
    -b, --build-dir DIR     Build directory (default: Build/release)
    -l, --log-file FILE     Log file for Valgrind output (default: valgrind_TIMESTAMP.log)
    -t, --tool TOOL         Valgrind tool (memcheck|callgrind|massif) (default: memcheck)
    -n, --no-track-origins  Don't track origins of uninitialized values
    -k, --leak-kinds KINDS  Leak kinds to show (all|definite|...) (default: all)
    -h, --help              Show this help

BINARY:
    Binary to run (default: Ladybird)
    Examples: Ladybird, WebContent, RequestServer, TestIPC

BINARY_ARGS:
    Arguments to pass to the binary

EXAMPLES:
    # Run Ladybird browser with memcheck
    $0

    # Run with callgrind profiler
    $0 -t callgrind

    # Run with massif heap profiler
    $0 -t massif

    # Run specific test
    $0 TestIPC

    # Run with binary arguments
    $0 Ladybird -- --debug-web-content

EOF
    exit 1
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -l|--log-file)
            LOG_FILE="$2"
            shift 2
            ;;
        -t|--tool)
            TOOL="$2"
            shift 2
            ;;
        -n|--no-track-origins)
            TRACK_ORIGINS="no"
            shift
            ;;
        -k|--leak-kinds)
            SHOW_LEAK_KINDS="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        --)
            shift
            EXTRA_ARGS=("$@")
            break
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
        *)
            BINARY="$1"
            shift
            ;;
    esac
done

# Check if valgrind is installed
if ! command -v valgrind &> /dev/null; then
    echo -e "${RED}Valgrind not found${NC}"
    echo -e "${YELLOW}Install: sudo apt-get install valgrind${NC}"
    exit 1
fi

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}Build directory not found: $BUILD_DIR${NC}"
    echo -e "${YELLOW}Run: cmake --preset Release && cmake --build Build/release${NC}"
    exit 1
fi

# Find binary
BINARY_PATH="$BUILD_DIR/bin/$BINARY"
if [[ ! -f "$BINARY_PATH" ]]; then
    echo -e "${RED}Binary not found: $BINARY_PATH${NC}"
    echo -e "${YELLOW}Available binaries:${NC}"
    ls -1 "$BUILD_DIR/bin/" | head -20
    exit 1
fi

# Check if binary has ASAN (conflicts with Valgrind)
if ldd "$BINARY_PATH" 2>/dev/null | grep -q "libasan"; then
    echo -e "${RED}Warning: Binary is built with ASAN${NC}"
    echo -e "${YELLOW}ASAN and Valgrind conflict. Use Release build:${NC}"
    echo -e "${YELLOW}cmake --preset Release && cmake --build Build/release${NC}"
    echo ""
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Tool-specific options
VALGRIND_OPTS=()

case "$TOOL" in
    memcheck)
        VALGRIND_OPTS=(
            --tool=memcheck
            --leak-check=full
            --show-leak-kinds="$SHOW_LEAK_KINDS"
            --track-origins="$TRACK_ORIGINS"
            --verbose
            --num-callers=20
            --error-limit=no
        )

        # Use suppressions file if it exists
        if [[ -f "$REPO_ROOT/valgrind.supp" ]]; then
            VALGRIND_OPTS+=(--suppressions="$REPO_ROOT/valgrind.supp")
        fi
        ;;

    callgrind)
        LOG_FILE="callgrind.out.$(date +%Y%m%d_%H%M%S)"
        VALGRIND_OPTS=(
            --tool=callgrind
            --dump-instr=yes
            --collect-jumps=yes
            --callgrind-out-file="$LOG_FILE"
        )
        ;;

    massif)
        LOG_FILE="massif.out.$(date +%Y%m%d_%H%M%S)"
        VALGRIND_OPTS=(
            --tool=massif
            --massif-out-file="$LOG_FILE"
        )
        ;;

    *)
        echo -e "${RED}Unknown tool: $TOOL${NC}"
        echo "Valid tools: memcheck, callgrind, massif"
        exit 1
        ;;
esac

# Add log file for memcheck
if [[ "$TOOL" == "memcheck" ]]; then
    VALGRIND_OPTS+=(--log-file="$LOG_FILE")
fi

# Print configuration
echo -e "${GREEN}=== Valgrind Configuration ===${NC}"
echo "Tool:        $TOOL"
echo "Binary:      $BINARY_PATH"
echo "Log file:    $LOG_FILE"
echo "Extra args:  ${EXTRA_ARGS[*]:-none}"
echo ""

# Enable core dumps
ulimit -c unlimited 2>/dev/null || true

# Run with valgrind
echo -e "${GREEN}=== Running $BINARY with Valgrind ===${NC}"
echo -e "${YELLOW}Note: This will be SLOW (10-50x slower than normal)${NC}"
echo ""

set +e  # Don't exit on error

valgrind "${VALGRIND_OPTS[@]}" "$BINARY_PATH" "${EXTRA_ARGS[@]}"
EXIT_CODE=$?

set -e

# Check results
echo ""
echo -e "${GREEN}=== Results ===${NC}"
echo "Exit code: $EXIT_CODE"

if [[ "$TOOL" == "memcheck" ]]; then
    if [[ -f "$LOG_FILE" ]]; then
        echo -e "${YELLOW}Valgrind log created: $LOG_FILE${NC}"

        # Count errors
        ERROR_COUNT=$(grep -c "ERROR SUMMARY" "$LOG_FILE" 2>/dev/null || echo "0")
        LEAK_COUNT=$(grep -c "definitely lost" "$LOG_FILE" 2>/dev/null || echo "0")

        # Show summary
        echo ""
        echo -e "${YELLOW}Error summary:${NC}"
        grep "ERROR SUMMARY" "$LOG_FILE" || true

        echo ""
        echo -e "${YELLOW}Leak summary:${NC}"
        grep -A 5 "LEAK SUMMARY" "$LOG_FILE" || true

        # Show first few errors
        echo ""
        echo -e "${YELLOW}First errors (if any):${NC}"
        grep -A 10 "^==[0-9]*== Invalid" "$LOG_FILE" | head -50 || echo "No errors found"

        echo ""
        echo -e "${GREEN}View full log: cat $LOG_FILE${NC}"

        # Suggest fixes
        if [[ $LEAK_COUNT -gt 0 ]]; then
            echo ""
            echo -e "${YELLOW}Memory leaks detected. To investigate:${NC}"
            echo "1. Search for 'definitely lost' in the log"
            echo "2. Look at the allocation stack traces"
            echo "3. Ensure proper cleanup with OwnPtr/RefPtr"
        fi
    fi

elif [[ "$TOOL" == "callgrind" ]]; then
    if [[ -f "$LOG_FILE" ]]; then
        echo -e "${GREEN}Callgrind output created: $LOG_FILE${NC}"
        echo -e "${YELLOW}Visualize with: kcachegrind $LOG_FILE${NC}"
    fi

elif [[ "$TOOL" == "massif" ]]; then
    if [[ -f "$LOG_FILE" ]]; then
        echo -e "${GREEN}Massif output created: $LOG_FILE${NC}"
        echo -e "${YELLOW}Visualize with: ms_print $LOG_FILE${NC}"

        # Generate text report
        TEXT_REPORT="${LOG_FILE}.txt"
        ms_print "$LOG_FILE" > "$TEXT_REPORT" 2>/dev/null || true
        if [[ -f "$TEXT_REPORT" ]]; then
            echo -e "${GREEN}Text report: $TEXT_REPORT${NC}"
        fi
    fi
fi

exit $EXIT_CODE
