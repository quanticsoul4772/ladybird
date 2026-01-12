#!/usr/bin/env bash
# Analyze sanitizer reports and extract useful information

set -euo pipefail

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] LOG_FILE

Analyze sanitizer output logs and generate summary report

OPTIONS:
    -v, --verbose          Show detailed information
    -s, --summary-only     Only show summary (no details)
    -f, --filter TYPE      Filter by error type (use-after-free, leak, overflow, etc.)
    -h, --help             Show this help

EXAMPLES:
    # Analyze ASAN log
    $0 asan.log

    # Show only summary
    $0 -s asan.log

    # Filter for use-after-free errors
    $0 -f use-after-free asan.log

    # Verbose output
    $0 -v asan.log

EOF
    exit 1
}

# Default values
VERBOSE=0
SUMMARY_ONLY=0
FILTER=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -s|--summary-only)
            SUMMARY_ONLY=1
            shift
            ;;
        -f|--filter)
            FILTER="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        -*)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
            ;;
        *)
            LOG_FILE="$1"
            shift
            ;;
    esac
done

# Check if log file provided
if [[ -z "${LOG_FILE:-}" ]]; then
    echo -e "${RED}Error: No log file specified${NC}"
    usage
fi

# Check if log file exists
if [[ ! -f "$LOG_FILE" ]]; then
    echo -e "${RED}Error: Log file not found: $LOG_FILE${NC}"
    exit 1
fi

# Determine sanitizer type
SANITIZER_TYPE="unknown"
if grep -q "AddressSanitizer" "$LOG_FILE"; then
    SANITIZER_TYPE="asan"
elif grep -q "UndefinedBehaviorSanitizer" "$LOG_FILE"; then
    SANITIZER_TYPE="ubsan"
elif grep -q "ThreadSanitizer" "$LOG_FILE"; then
    SANITIZER_TYPE="tsan"
elif grep -q "MemorySanitizer" "$LOG_FILE"; then
    SANITIZER_TYPE="msan"
fi

echo -e "${GREEN}=== Sanitizer Report Analysis ===${NC}"
echo "File: $LOG_FILE"
echo "Type: $SANITIZER_TYPE"
echo ""

# Extract errors based on sanitizer type
if [[ "$SANITIZER_TYPE" == "asan" ]]; then
    # ASAN error types
    UAF_COUNT=$(grep -c "heap-use-after-free" "$LOG_FILE" 2>/dev/null || echo "0")
    HEAP_OVERFLOW_COUNT=$(grep -c "heap-buffer-overflow" "$LOG_FILE" 2>/dev/null || echo "0")
    STACK_OVERFLOW_COUNT=$(grep -c "stack-buffer-overflow" "$LOG_FILE" 2>/dev/null || echo "0")
    GLOBAL_OVERFLOW_COUNT=$(grep -c "global-buffer-overflow" "$LOG_FILE" 2>/dev/null || echo "0")
    DOUBLE_FREE_COUNT=$(grep -c "attempting double-free" "$LOG_FILE" 2>/dev/null || echo "0")
    LEAK_COUNT=$(grep -c "LeakSanitizer: detected memory leaks" "$LOG_FILE" 2>/dev/null || echo "0")

    TOTAL_ERRORS=$((UAF_COUNT + HEAP_OVERFLOW_COUNT + STACK_OVERFLOW_COUNT + GLOBAL_OVERFLOW_COUNT + DOUBLE_FREE_COUNT + LEAK_COUNT))

    echo -e "${YELLOW}=== ASAN Error Summary ===${NC}"
    echo "Total errors: $TOTAL_ERRORS"
    echo ""
    echo "  Use-after-free:        $UAF_COUNT"
    echo "  Heap buffer overflow:  $HEAP_OVERFLOW_COUNT"
    echo "  Stack buffer overflow: $STACK_OVERFLOW_COUNT"
    echo "  Global buffer overflow: $GLOBAL_OVERFLOW_COUNT"
    echo "  Double-free:           $DOUBLE_FREE_COUNT"
    echo "  Memory leaks:          $LEAK_COUNT"
    echo ""

    # Show details if not summary-only
    if [[ $SUMMARY_ONLY -eq 0 ]]; then
        # Use-after-free details
        if [[ $UAF_COUNT -gt 0 ]] && [[ -z "$FILTER" || "$FILTER" == "use-after-free" ]]; then
            echo -e "${RED}=== Use-After-Free Errors ($UAF_COUNT) ===${NC}"
            grep -A 15 "heap-use-after-free" "$LOG_FILE" | head -100
            echo ""
        fi

        # Heap overflow details
        if [[ $HEAP_OVERFLOW_COUNT -gt 0 ]] && [[ -z "$FILTER" || "$FILTER" == "heap-overflow" ]]; then
            echo -e "${RED}=== Heap Buffer Overflow Errors ($HEAP_OVERFLOW_COUNT) ===${NC}"
            grep -A 15 "heap-buffer-overflow" "$LOG_FILE" | head -100
            echo ""
        fi

        # Stack overflow details
        if [[ $STACK_OVERFLOW_COUNT -gt 0 ]] && [[ -z "$FILTER" || "$FILTER" == "stack-overflow" ]]; then
            echo -e "${RED}=== Stack Buffer Overflow Errors ($STACK_OVERFLOW_COUNT) ===${NC}"
            grep -A 15 "stack-buffer-overflow" "$LOG_FILE" | head -100
            echo ""
        fi

        # Memory leak details
        if [[ $LEAK_COUNT -gt 0 ]] && [[ -z "$FILTER" || "$FILTER" == "leak" ]]; then
            echo -e "${RED}=== Memory Leak Errors ($LEAK_COUNT) ===${NC}"
            grep -A 20 "LeakSanitizer: detected memory leaks" "$LOG_FILE" | head -100
            echo ""

            # Extract leak summary
            if grep -q "SUMMARY: AddressSanitizer:" "$LOG_FILE"; then
                echo -e "${YELLOW}Leak Summary:${NC}"
                grep "SUMMARY: AddressSanitizer:" "$LOG_FILE" | grep -v "heap-" | head -20
                echo ""
            fi
        fi
    fi

    # Recommendations
    if [[ $TOTAL_ERRORS -gt 0 ]]; then
        echo -e "${YELLOW}=== Recommendations ===${NC}"
        if [[ $UAF_COUNT -gt 0 ]]; then
            echo "- Use-after-free: Use RefPtr/NonnullRefPtr to keep objects alive"
        fi
        if [[ $HEAP_OVERFLOW_COUNT -gt 0 || $STACK_OVERFLOW_COUNT -gt 0 ]]; then
            echo "- Buffer overflow: Use Vector<T> instead of fixed arrays, check bounds"
        fi
        if [[ $DOUBLE_FREE_COUNT -gt 0 ]]; then
            echo "- Double-free: Use OwnPtr/NonnullOwnPtr for unique ownership"
        fi
        if [[ $LEAK_COUNT -gt 0 ]]; then
            echo "- Memory leak: Use OwnPtr/RefPtr, check for reference cycles"
        fi
        echo ""
    fi

elif [[ "$SANITIZER_TYPE" == "ubsan" ]]; then
    # UBSAN error types
    INT_OVERFLOW_COUNT=$(grep -c "signed integer overflow" "$LOG_FILE" 2>/dev/null || echo "0")
    NULL_DEREF_COUNT=$(grep -c "null pointer" "$LOG_FILE" 2>/dev/null || echo "0")
    MISALIGNED_COUNT=$(grep -c "misaligned" "$LOG_FILE" 2>/dev/null || echo "0")
    INDEX_OOB_COUNT=$(grep -c "index.*out of bounds" "$LOG_FILE" 2>/dev/null || echo "0")
    INVALID_ENUM_COUNT=$(grep -c "invalid.*enum" "$LOG_FILE" 2>/dev/null || echo "0")

    TOTAL_ERRORS=$((INT_OVERFLOW_COUNT + NULL_DEREF_COUNT + MISALIGNED_COUNT + INDEX_OOB_COUNT + INVALID_ENUM_COUNT))

    echo -e "${YELLOW}=== UBSAN Error Summary ===${NC}"
    echo "Total errors: $TOTAL_ERRORS"
    echo ""
    echo "  Integer overflow:      $INT_OVERFLOW_COUNT"
    echo "  Null pointer:          $NULL_DEREF_COUNT"
    echo "  Misaligned pointer:    $MISALIGNED_COUNT"
    echo "  Index out of bounds:   $INDEX_OOB_COUNT"
    echo "  Invalid enum:          $INVALID_ENUM_COUNT"
    echo ""

    # Show details if not summary-only
    if [[ $SUMMARY_ONLY -eq 0 ]]; then
        if [[ $INT_OVERFLOW_COUNT -gt 0 ]]; then
            echo -e "${RED}=== Integer Overflow Errors ($INT_OVERFLOW_COUNT) ===${NC}"
            grep -B 2 -A 5 "signed integer overflow" "$LOG_FILE" | head -50
            echo ""
        fi

        if [[ $NULL_DEREF_COUNT -gt 0 ]]; then
            echo -e "${RED}=== Null Pointer Errors ($NULL_DEREF_COUNT) ===${NC}"
            grep -B 2 -A 5 "null pointer" "$LOG_FILE" | head -50
            echo ""
        fi
    fi

    # Recommendations
    if [[ $TOTAL_ERRORS -gt 0 ]]; then
        echo -e "${YELLOW}=== Recommendations ===${NC}"
        if [[ $INT_OVERFLOW_COUNT -gt 0 ]]; then
            echo "- Integer overflow: Use AK::checked_add/mul, validate IPC sizes"
        fi
        if [[ $NULL_DEREF_COUNT -gt 0 ]]; then
            echo "- Null pointer: Check for null before access, use Optional<T>"
        fi
        if [[ $MISALIGNED_COUNT -gt 0 ]]; then
            echo "- Misaligned pointer: Use memcpy for unaligned access"
        fi
        if [[ $INDEX_OOB_COUNT -gt 0 ]]; then
            echo "- Index OOB: Use Vector::at() with bounds checking"
        fi
        if [[ $INVALID_ENUM_COUNT -gt 0 ]]; then
            echo "- Invalid enum: Validate enum values during IPC deserialization"
        fi
        echo ""
    fi

elif [[ "$SANITIZER_TYPE" == "tsan" ]]; then
    # TSAN error types
    DATA_RACE_COUNT=$(grep -c "WARNING: ThreadSanitizer: data race" "$LOG_FILE" 2>/dev/null || echo "0")
    DEADLOCK_COUNT=$(grep -c "WARNING: ThreadSanitizer: lock-order-inversion" "$LOG_FILE" 2>/dev/null || echo "0")

    TOTAL_ERRORS=$((DATA_RACE_COUNT + DEADLOCK_COUNT))

    echo -e "${YELLOW}=== TSAN Error Summary ===${NC}"
    echo "Total errors: $TOTAL_ERRORS"
    echo ""
    echo "  Data races:   $DATA_RACE_COUNT"
    echo "  Deadlocks:    $DEADLOCK_COUNT"
    echo ""

    if [[ $SUMMARY_ONLY -eq 0 && $DATA_RACE_COUNT -gt 0 ]]; then
        echo -e "${RED}=== Data Race Errors ($DATA_RACE_COUNT) ===${NC}"
        grep -A 20 "WARNING: ThreadSanitizer: data race" "$LOG_FILE" | head -100
        echo ""
    fi

    # Recommendations
    if [[ $TOTAL_ERRORS -gt 0 ]]; then
        echo -e "${YELLOW}=== Recommendations ===${NC}"
        if [[ $DATA_RACE_COUNT -gt 0 ]]; then
            echo "- Data race: Use Threading::Mutex or Atomic<T> for shared data"
        fi
        if [[ $DEADLOCK_COUNT -gt 0 ]]; then
            echo "- Deadlock: Ensure consistent lock ordering"
        fi
        echo ""
    fi
fi

# Extract unique stack traces
if [[ $VERBOSE -eq 1 ]]; then
    echo -e "${BLUE}=== Unique Error Locations ===${NC}"
    grep -oP "(?<=in ).*?(?= )" "$LOG_FILE" 2>/dev/null | sort | uniq -c | sort -rn | head -20 || true
    echo ""

    echo -e "${BLUE}=== Affected Files ===${NC}"
    grep -oP "(?<=/)[\w/]+\.cpp:\d+" "$LOG_FILE" 2>/dev/null | sort | uniq -c | sort -rn | head -20 || true
    echo ""
fi

# Summary
if [[ "$SANITIZER_TYPE" != "unknown" ]]; then
    echo -e "${GREEN}=== Next Steps ===${NC}"
    echo "1. Review errors above (use -f FILTER to focus on specific type)"
    echo "2. Reproduce error with minimal test case"
    echo "3. Fix using appropriate pattern (RefPtr, OwnPtr, bounds check, etc.)"
    echo "4. Verify fix with sanitizers"
    echo "5. Add regression test"
else
    echo -e "${YELLOW}Could not determine sanitizer type${NC}"
fi
