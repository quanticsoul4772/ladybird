#!/usr/bin/env bash
# Run Ladybird with AddressSanitizer and comprehensive detection options

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
BUILD_DIR="$REPO_ROOT/Build/sanitizers"
BINARY="Ladybird"
LOG_FILE="asan_$(date +%Y%m%d_%H%M%S).log"
HALT_ON_ERROR=0
DETECT_LEAKS=1
EXTRA_ARGS=()

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] [BINARY] [-- BINARY_ARGS]

Run Ladybird (or other binaries) with AddressSanitizer

OPTIONS:
    -b, --build-dir DIR     Build directory (default: Build/sanitizers)
    -l, --log-file FILE     Log file for ASAN output (default: asan_TIMESTAMP.log)
    -s, --halt-on-error     Stop on first error (default: continue)
    -n, --no-leak-detect    Disable leak detection
    -h, --help              Show this help

BINARY:
    Binary to run (default: Ladybird)
    Examples: Ladybird, WebContent, RequestServer, TestIPC

BINARY_ARGS:
    Arguments to pass to the binary

EXAMPLES:
    # Run Ladybird browser
    $0

    # Run WebContent process
    $0 WebContent

    # Run specific test with halt on error
    $0 -s TestIPC

    # Run with custom log file
    $0 -l my_asan.log Ladybird

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
        -s|--halt-on-error)
            HALT_ON_ERROR=1
            shift
            ;;
        -n|--no-leak-detect)
            DETECT_LEAKS=0
            shift
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

# Check if build directory exists
if [[ ! -d "$BUILD_DIR" ]]; then
    echo -e "${RED}Build directory not found: $BUILD_DIR${NC}"
    echo -e "${YELLOW}Run: cmake --preset Sanitizers && cmake --build Build/sanitizers${NC}"
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

# Check if binary has ASAN
if ! ldd "$BINARY_PATH" | grep -q "libasan"; then
    echo -e "${YELLOW}Warning: Binary may not be built with ASAN${NC}"
    echo -e "${YELLOW}Run: cmake --preset Sanitizers && cmake --build Build/sanitizers${NC}"
fi

# Configure ASAN options
export ASAN_OPTIONS="\
detect_leaks=${DETECT_LEAKS}:\
strict_string_checks=1:\
detect_stack_use_after_return=1:\
check_initialization_order=1:\
strict_init_order=1:\
detect_invalid_pointer_pairs=2:\
print_stacktrace=1:\
symbolize=1:\
halt_on_error=${HALT_ON_ERROR}:\
abort_on_error=0:\
log_path=${LOG_FILE}"

# Configure UBSAN options (usually enabled with ASAN in Sanitizers preset)
export UBSAN_OPTIONS="\
print_stacktrace=1:\
halt_on_error=${HALT_ON_ERROR}"

# Configure LSAN options
if [[ $DETECT_LEAKS -eq 1 ]]; then
    export LSAN_OPTIONS="\
print_suppressions=0"

    # Use suppressions file if it exists
    if [[ -f "$REPO_ROOT/lsan_suppressions.txt" ]]; then
        export LSAN_OPTIONS="${LSAN_OPTIONS}:suppressions=$REPO_ROOT/lsan_suppressions.txt"
    fi
fi

# Set symbolizer path
if command -v llvm-symbolizer &> /dev/null; then
    export ASAN_SYMBOLIZER_PATH="$(command -v llvm-symbolizer)"
fi

# Print configuration
echo -e "${GREEN}=== ASAN Configuration ===${NC}"
echo "Binary:      $BINARY_PATH"
echo "Log file:    $LOG_FILE"
echo "Leak detect: $DETECT_LEAKS"
echo "Halt on err: $HALT_ON_ERROR"
echo "Extra args:  ${EXTRA_ARGS[*]:-none}"
echo ""

# Enable core dumps
ulimit -c unlimited 2>/dev/null || true

# Run the binary
echo -e "${GREEN}=== Running $BINARY ===${NC}"
set +e  # Don't exit on error

"$BINARY_PATH" "${EXTRA_ARGS[@]}"
EXIT_CODE=$?

set -e

# Check results
echo ""
echo -e "${GREEN}=== Results ===${NC}"
echo "Exit code: $EXIT_CODE"

# Check if ASAN logs were created
if ls ${LOG_FILE}* &>/dev/null; then
    echo -e "${YELLOW}ASAN logs created:${NC}"
    ls -lh ${LOG_FILE}*

    # Count errors
    ERROR_COUNT=$(grep -c "ERROR: AddressSanitizer" ${LOG_FILE}* 2>/dev/null || echo "0")
    LEAK_COUNT=$(grep -c "ERROR: LeakSanitizer" ${LOG_FILE}* 2>/dev/null || echo "0")

    if [[ $ERROR_COUNT -gt 0 ]]; then
        echo -e "${RED}Found $ERROR_COUNT ASAN errors${NC}"
    fi

    if [[ $LEAK_COUNT -gt 0 ]]; then
        echo -e "${RED}Found $LEAK_COUNT memory leaks${NC}"
    fi

    # Show summary
    echo ""
    echo -e "${YELLOW}Error summary:${NC}"
    grep "ERROR: AddressSanitizer" ${LOG_FILE}* 2>/dev/null | head -20 || true
    grep "ERROR: LeakSanitizer" ${LOG_FILE}* 2>/dev/null | head -10 || true

    echo ""
    echo -e "${GREEN}View full logs: cat ${LOG_FILE}*${NC}"
else
    if [[ $EXIT_CODE -eq 0 ]]; then
        echo -e "${GREEN}No errors detected!${NC}"
    else
        echo -e "${YELLOW}Binary exited with code $EXIT_CODE, but no ASAN logs created${NC}"
    fi
fi

exit $EXIT_CODE
