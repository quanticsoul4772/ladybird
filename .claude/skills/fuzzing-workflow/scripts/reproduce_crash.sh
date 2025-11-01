#!/usr/bin/env bash
# Reproduce crash with debugging and generate minimal reproducer

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
NC='\033[0m' # No Color

# Default values
CRASH_FILE=""
TARGET_BINARY=""
USE_DEBUGGER=0
DEBUGGER="gdb"
MINIMIZE=0
OUTPUT_DIR=""
VERBOSE=0
RUN_COUNT=1

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] CRASH_FILE TARGET_BINARY

Reproduce crash with debugging and generate minimal reproducer

OPTIONS:
    -d, --debugger [TYPE]   Attach debugger (gdb or lldb, default: gdb)
    -m, --minimize          Generate minimal reproducer
    -o, --output DIR        Output directory for reproducer (default: reproducers/)
    -n, --runs N            Number of times to reproduce (default: 1)
    -v, --verbose           Verbose output
    -h, --help              Show this help

ARGUMENTS:
    CRASH_FILE              Path to crash input file
    TARGET_BINARY           Path to fuzz target binary

DESCRIPTION:
    This tool helps reproduce crashes found during fuzzing. It can:
    - Run the target with the crash input
    - Attach a debugger for interactive analysis
    - Generate stack traces and crash information
    - Minimize the crash input to smallest reproducer
    - Verify crash reproducibility

EXAMPLES:
    # Basic crash reproduction
    $0 crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

    # Reproduce with GDB debugger
    $0 -d crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

    # Reproduce and minimize
    $0 -m crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

    # Test reproducibility (run 10 times)
    $0 -n 10 crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

    # Use LLDB instead of GDB
    $0 -d lldb crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

EOF
    exit 1
}

log_info() {
    echo -e "${BLUE}==>${NC} $1"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1"
}

log_error() {
    echo -e "${RED}✗${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

log_debug() {
    [[ $VERBOSE -eq 1 ]] && echo -e "${MAGENTA}[DEBUG]${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debugger)
            USE_DEBUGGER=1
            if [[ $# -gt 1 ]] && [[ "$2" != -* ]]; then
                DEBUGGER="$2"
                shift 2
            else
                shift
            fi
            ;;
        -m|--minimize)
            MINIMIZE=1
            shift
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -n|--runs)
            RUN_COUNT="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            usage
            ;;
        *)
            if [[ -z "$CRASH_FILE" ]]; then
                CRASH_FILE="$1"
            elif [[ -z "$TARGET_BINARY" ]]; then
                TARGET_BINARY="$1"
            else
                log_error "Too many arguments"
                usage
            fi
            shift
            ;;
    esac
done

# Validate arguments
if [[ -z "$CRASH_FILE" ]] || [[ -z "$TARGET_BINARY" ]]; then
    log_error "Missing required arguments"
    usage
fi

if [[ ! -f "$CRASH_FILE" ]]; then
    log_error "Crash file not found: $CRASH_FILE"
    exit 1
fi

if [[ ! -f "$TARGET_BINARY" ]]; then
    log_error "Target binary not found: $TARGET_BINARY"
    exit 1
fi

# Set default output directory
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="reproducers"
fi

mkdir -p "$OUTPUT_DIR"

# Get absolute paths
CRASH_FILE=$(realpath "$CRASH_FILE")
TARGET_BINARY=$(realpath "$TARGET_BINARY")

CRASH_NAME=$(basename "$CRASH_FILE")

echo "============================================"
echo "Crash Reproduction"
echo "============================================"
echo ""
log_info "Configuration:"
echo "  Crash file:  $CRASH_FILE"
echo "  Target:      $TARGET_BINARY"
echo "  Debugger:    ${USE_DEBUGGER}"
echo "  Minimize:    ${MINIMIZE}"
echo "  Runs:        $RUN_COUNT"
echo "  Output:      $OUTPUT_DIR"
echo ""

# Check crash file size
CRASH_SIZE=$(wc -c < "$CRASH_FILE")
log_info "Crash file size: $CRASH_SIZE bytes"
echo ""

# Function to reproduce crash once
reproduce_once() {
    local attempt=$1
    local output_file="$OUTPUT_DIR/${CRASH_NAME}.trace.$attempt"

    log_debug "Attempt $attempt: Running $TARGET_BINARY with crash input..."

    # Configure ASAN for detailed output
    export ASAN_OPTIONS="halt_on_error=1:symbolize=1:print_stacktrace=1:print_summary=1:check_initialization_order=1"
    export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"

    # Set symbolizer if available
    if command -v llvm-symbolizer &> /dev/null; then
        export ASAN_SYMBOLIZER_PATH="$(command -v llvm-symbolizer)"
    fi

    # Run target with crash input
    set +e
    "$TARGET_BINARY" "$CRASH_FILE" > "$output_file" 2>&1
    local exit_code=$?
    set -e

    log_debug "Exit code: $exit_code"

    # Check if crash was reproduced
    if [[ $exit_code -ne 0 ]] || grep -q "ERROR: AddressSanitizer\|ERROR: UndefinedBehaviorSanitizer" "$output_file" 2>/dev/null; then
        return 0  # Crash reproduced
    else
        return 1  # No crash
    fi
}

# Reproduce crash multiple times
log_info "Reproducing crash..."
REPRODUCED_COUNT=0

for ((i=1; i<=RUN_COUNT; i++)); do
    if reproduce_once "$i"; then
        REPRODUCED_COUNT=$((REPRODUCED_COUNT + 1))
        if [[ $RUN_COUNT -eq 1 ]]; then
            log_success "Crash reproduced!"
        else
            echo -ne "\r  Reproduced: $REPRODUCED_COUNT/$i"
        fi
    else
        if [[ $RUN_COUNT -eq 1 ]]; then
            log_warn "Crash did not reproduce"
        fi
    fi
done

if [[ $RUN_COUNT -gt 1 ]]; then
    echo ""
    if [[ $REPRODUCED_COUNT -eq $RUN_COUNT ]]; then
        log_success "Crash reproduced $REPRODUCED_COUNT/$RUN_COUNT times (100%)"
    elif [[ $REPRODUCED_COUNT -gt 0 ]]; then
        log_warn "Crash reproduced $REPRODUCED_COUNT/$RUN_COUNT times ($((REPRODUCED_COUNT * 100 / RUN_COUNT))%) - Non-deterministic"
    else
        log_error "Crash did not reproduce in any attempts"
        exit 1
    fi
fi

# Analyze crash
TRACE_FILE="$OUTPUT_DIR/${CRASH_NAME}.trace.1"
log_info "Analyzing crash..."

# Extract crash type
CRASH_TYPE="unknown"
if grep -q "heap-use-after-free" "$TRACE_FILE"; then
    CRASH_TYPE="heap-use-after-free"
elif grep -q "heap-buffer-overflow" "$TRACE_FILE"; then
    CRASH_TYPE="heap-buffer-overflow"
elif grep -q "stack-buffer-overflow" "$TRACE_FILE"; then
    CRASH_TYPE="stack-buffer-overflow"
elif grep -q "SEGV on unknown address 0x0" "$TRACE_FILE"; then
    CRASH_TYPE="null-pointer-dereference"
elif grep -q "SEGV" "$TRACE_FILE"; then
    CRASH_TYPE="segmentation-fault"
elif grep -q "timeout" "$TRACE_FILE"; then
    CRASH_TYPE="timeout"
elif grep -q "undefined-behavior" "$TRACE_FILE"; then
    CRASH_TYPE="undefined-behavior"
fi

echo ""
echo -e "${BLUE}Crash Information:${NC}"
echo "  Type:        $CRASH_TYPE"

# Extract crash location
if grep -A 5 "ERROR: AddressSanitizer" "$TRACE_FILE" | grep -q "^    #0"; then
    CRASH_LOCATION=$(grep -A 5 "ERROR: AddressSanitizer" "$TRACE_FILE" | grep "^    #0" | head -1)
    echo "  Location:    $CRASH_LOCATION"
fi

# Display stack trace
echo ""
echo -e "${BLUE}Stack Trace:${NC}"
grep -A 15 "^    #" "$TRACE_FILE" | head -20 || echo "  (no stack trace available)"

# Save summary
SUMMARY_FILE="$OUTPUT_DIR/${CRASH_NAME}.summary"
cat > "$SUMMARY_FILE" <<EOF
Crash Reproduction Summary
==========================
Date: $(date)
Crash File: $CRASH_FILE
Target Binary: $TARGET_BINARY
File Size: $CRASH_SIZE bytes
Crash Type: $CRASH_TYPE
Reproducibility: $REPRODUCED_COUNT/$RUN_COUNT ($((REPRODUCED_COUNT * 100 / RUN_COUNT))%)

Stack Trace:
$(grep -A 15 "^    #" "$TRACE_FILE" | head -20)

Full trace: $TRACE_FILE
EOF

log_success "Summary saved to: $SUMMARY_FILE"

# Minimize crash if requested
if [[ $MINIMIZE -eq 1 ]]; then
    echo ""
    log_info "Minimizing crash input..."

    MINIMIZED_FILE="$OUTPUT_DIR/${CRASH_NAME}.minimized"

    # Use LibFuzzer's built-in minimization
    log_info "Running minimization (timeout: 300s)..."

    "$TARGET_BINARY" \
        -minimize_crash=1 \
        -max_total_time=300 \
        -exact_artifact_path="$MINIMIZED_FILE" \
        "$CRASH_FILE" > "$OUTPUT_DIR/${CRASH_NAME}.minimize.log" 2>&1 || true

    if [[ -f "$MINIMIZED_FILE" ]]; then
        MINIMIZED_SIZE=$(wc -c < "$MINIMIZED_FILE")
        REDUCTION=$((100 - (MINIMIZED_SIZE * 100 / CRASH_SIZE)))

        log_success "Minimized crash input:"
        echo "  Original:  $CRASH_SIZE bytes"
        echo "  Minimized: $MINIMIZED_SIZE bytes"
        echo "  Reduction: $REDUCTION%"
        echo "  Saved to:  $MINIMIZED_FILE"

        # Verify minimized crash still reproduces
        log_info "Verifying minimized crash..."
        if reproduce_once "minimized"; then
            log_success "Minimized crash reproduces successfully"
        else
            log_warn "Minimized crash may not reproduce reliably"
        fi
    else
        log_warn "Minimization did not produce output"
    fi
fi

# Launch debugger if requested
if [[ $USE_DEBUGGER -eq 1 ]]; then
    echo ""
    log_info "Launching debugger: $DEBUGGER"

    case "$DEBUGGER" in
        gdb)
            if ! command -v gdb &> /dev/null; then
                log_error "GDB not found. Install with: sudo apt install gdb"
                exit 1
            fi

            # Create GDB commands
            GDB_COMMANDS="$OUTPUT_DIR/${CRASH_NAME}.gdb"
            cat > "$GDB_COMMANDS" <<EOF
set args $CRASH_FILE
run
bt full
info registers
x/20i \$pc
EOF

            log_info "GDB will run with crash input"
            echo "Useful commands:"
            echo "  bt       - backtrace"
            echo "  bt full  - backtrace with locals"
            echo "  info reg - show registers"
            echo "  x/20i \$pc - disassemble at crash"
            echo ""

            gdb -x "$GDB_COMMANDS" "$TARGET_BINARY"
            ;;

        lldb)
            if ! command -v lldb &> /dev/null; then
                log_error "LLDB not found. Install with: sudo apt install lldb"
                exit 1
            fi

            log_info "LLDB will run with crash input"
            echo "Useful commands:"
            echo "  bt       - backtrace"
            echo "  frame info - frame details"
            echo "  register read - show registers"
            echo ""

            lldb -o "run $CRASH_FILE" -o "bt" "$TARGET_BINARY"
            ;;

        *)
            log_error "Unknown debugger: $DEBUGGER"
            log_error "Supported debuggers: gdb, lldb"
            exit 1
            ;;
    esac
fi

echo ""
echo "============================================"
echo "Reproduction Complete"
echo "============================================"
echo ""
log_info "Next steps:"
echo "  1. Review crash summary: $SUMMARY_FILE"
echo "  2. Review stack trace: $TRACE_FILE"
[[ $MINIMIZE -eq 1 ]] && [[ -f "$MINIMIZED_FILE" ]] && echo "  3. Use minimized input: $MINIMIZED_FILE"
echo "  4. Write unit test to prevent regression"
echo "  5. Fix the bug and verify with: $TARGET_BINARY $CRASH_FILE"
echo ""

# Generate unit test template
TEST_TEMPLATE="$OUTPUT_DIR/${CRASH_NAME}.test_template.cpp"
cat > "$TEST_TEMPLATE" <<EOF
// Unit test template for crash: $CRASH_NAME
// Crash type: $CRASH_TYPE

#include <LibTest/TestCase.h>

TEST_CASE(crash_${CRASH_NAME}_regression)
{
    // Load crash input
    auto file = MUST(Core::File::open("$CRASH_FILE", Core::File::OpenMode::Read));
    auto input = MUST(file->read_until_eof());

    // TODO: Add appropriate test for your component
    // This should NOT crash after the fix

    // Example for IPC:
    // IPC::Decoder decoder(input.data(), input.size());
    // auto result = handle_message(decoder);
    // EXPECT(result.is_error()); // Should return error, not crash

    // Example for parser:
    // auto result = parse_input(input);
    // EXPECT(result.is_error() || result.value() != nullptr);
}
EOF

log_info "Unit test template: $TEST_TEMPLATE"
