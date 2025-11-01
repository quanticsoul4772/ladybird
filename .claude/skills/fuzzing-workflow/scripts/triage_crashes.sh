#!/usr/bin/env bash
# Analyze and triage crash findings from fuzzing campaigns

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
CRASHES_DIR=""
TARGET_BINARY=""
OUTPUT_DIR=""
DEDUP=1
VERBOSE=0
MINIMIZE=0
PARALLEL=1

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] CRASHES_DIR TARGET_BINARY

Analyze and triage crash findings from fuzzing campaigns

OPTIONS:
    -o, --output DIR        Output directory for reports (default: crash_reports/)
    -n, --no-dedup          Don't deduplicate crashes
    -m, --minimize          Minimize crash inputs
    -p, --parallel N        Number of parallel analysis jobs (default: 1)
    -v, --verbose           Verbose output
    -h, --help              Show this help

ARGUMENTS:
    CRASHES_DIR             Directory containing crash files
    TARGET_BINARY           Path to fuzz target binary

EXAMPLES:
    # Analyze crashes from LibFuzzer
    $0 Fuzzing/Corpus/FuzzIPCMessages-crashes/ Build/fuzzers/bin/FuzzIPCMessages

    # Analyze and minimize crashes
    $0 -m Fuzzing/Corpus/FuzzIPCMessages-crashes/ Build/fuzzers/bin/FuzzIPCMessages

    # Analyze AFL++ crashes
    $0 Fuzzing/Corpus/FuzzIPCMessages-afl-findings/default/crashes/ Build/fuzzers/bin/FuzzIPCMessages

    # Verbose output with 4 parallel jobs
    $0 -v -p 4 crashes/ Build/fuzzers/bin/FuzzIPCMessages

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
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -n|--no-dedup)
            DEDUP=0
            shift
            ;;
        -m|--minimize)
            MINIMIZE=1
            shift
            ;;
        -p|--parallel)
            PARALLEL="$2"
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
            if [[ -z "$CRASHES_DIR" ]]; then
                CRASHES_DIR="$1"
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
if [[ -z "$CRASHES_DIR" ]] || [[ -z "$TARGET_BINARY" ]]; then
    log_error "Missing required arguments"
    usage
fi

if [[ ! -d "$CRASHES_DIR" ]]; then
    log_error "Crashes directory not found: $CRASHES_DIR"
    exit 1
fi

if [[ ! -f "$TARGET_BINARY" ]]; then
    log_error "Target binary not found: $TARGET_BINARY"
    exit 1
fi

# Set default output directory
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="crash_reports_$(date +%Y%m%d_%H%M%S)"
fi

mkdir -p "$OUTPUT_DIR"

echo "============================================"
echo "Crash Triage Analysis"
echo "============================================"
echo ""
log_info "Configuration:"
echo "  Crashes:    $CRASHES_DIR"
echo "  Target:     $TARGET_BINARY"
echo "  Output:     $OUTPUT_DIR"
echo "  Dedup:      $DEDUP"
echo "  Minimize:   $MINIMIZE"
echo "  Parallel:   $PARALLEL"
echo ""

# Find all crash files
log_info "Finding crash files..."
mapfile -t CRASH_FILES < <(find "$CRASHES_DIR" -type f ! -name "README.txt" ! -name "*.log" | sort)

CRASH_COUNT=${#CRASH_FILES[@]}

if [[ $CRASH_COUNT -eq 0 ]]; then
    log_warn "No crash files found in $CRASHES_DIR"
    exit 0
fi

log_success "Found $CRASH_COUNT crash files"
echo ""

# Function to analyze a single crash
analyze_crash() {
    local crash_file="$1"
    local crash_name=$(basename "$crash_file")
    local output_file="$OUTPUT_DIR/${crash_name}.trace"

    log_debug "Analyzing: $crash_name"

    # Run target with crash input
    ASAN_OPTIONS="halt_on_error=1:symbolize=1:print_stacktrace=1" \
        "$TARGET_BINARY" "$crash_file" > "$output_file" 2>&1 || true

    # Extract crash type
    local crash_type="unknown"
    if grep -q "SEGV on unknown address" "$output_file"; then
        crash_type="segv_unknown"
    elif grep -q "heap-use-after-free" "$output_file"; then
        crash_type="use_after_free"
    elif grep -q "heap-buffer-overflow" "$output_file"; then
        crash_type="buffer_overflow"
    elif grep -q "stack-buffer-overflow" "$output_file"; then
        crash_type="stack_overflow"
    elif grep -q "nullptr-dereference" "$output_file" || grep -q "SEGV on unknown address 0x0" "$output_file"; then
        crash_type="nullptr"
    elif grep -q "timeout" "$output_file"; then
        crash_type="timeout"
    elif grep -q "out-of-memory" "$output_file"; then
        crash_type="oom"
    fi

    # Extract stack trace hash for deduplication
    local stack_hash=""
    if command -v md5sum &> /dev/null; then
        stack_hash=$(grep -A 20 "ERROR: AddressSanitizer" "$output_file" 2>/dev/null | \
            grep "^    #" | head -10 | md5sum | cut -d' ' -f1)
    fi

    # Extract severity indicators
    local severity="low"
    if grep -q "write" "$output_file" && grep -q "overflow\|use-after-free" "$output_file"; then
        severity="critical"
    elif grep -q "overflow\|use-after-free\|SEGV" "$output_file"; then
        severity="high"
    elif grep -q "nullptr" "$output_file"; then
        severity="medium"
    fi

    # Create summary file
    cat > "$output_file.summary" <<EOF
File: $crash_name
Type: $crash_type
Severity: $severity
Stack Hash: $stack_hash
Size: $(wc -c < "$crash_file") bytes
EOF

    echo "$output_file.summary"
}

export -f analyze_crash
export -f log_debug
export OUTPUT_DIR TARGET_BINARY VERBOSE BLUE NC MAGENTA

# Analyze crashes in parallel
log_info "Analyzing crashes..."
if command -v parallel &> /dev/null && [[ $PARALLEL -gt 1 ]]; then
    log_info "Using GNU parallel with $PARALLEL jobs"
    printf '%s\n' "${CRASH_FILES[@]}" | \
        parallel -j "$PARALLEL" --bar analyze_crash {}
else
    # Sequential processing
    local count=0
    for crash_file in "${CRASH_FILES[@]}"; do
        count=$((count + 1))
        echo -ne "\r  Progress: $count/$CRASH_COUNT"
        analyze_crash "$crash_file" > /dev/null
    done
    echo ""
fi

log_success "Analysis complete"
echo ""

# Deduplicate crashes
if [[ $DEDUP -eq 1 ]]; then
    log_info "Deduplicating crashes by stack trace..."

    declare -A UNIQUE_CRASHES
    declare -A CRASH_TYPES

    for summary_file in "$OUTPUT_DIR"/*.summary; do
        [[ -f "$summary_file" ]] || continue

        local stack_hash=$(grep "^Stack Hash:" "$summary_file" | cut -d' ' -f3)
        local crash_type=$(grep "^Type:" "$summary_file" | cut -d' ' -f2)
        local crash_name=$(grep "^File:" "$summary_file" | cut -d' ' -f2)

        if [[ -z "${UNIQUE_CRASHES[$stack_hash]:-}" ]]; then
            UNIQUE_CRASHES[$stack_hash]="$crash_name"
            CRASH_TYPES[$stack_hash]="$crash_type"
        fi
    done

    UNIQUE_COUNT=${#UNIQUE_CRASHES[@]}
    log_success "Found $UNIQUE_COUNT unique crashes (from $CRASH_COUNT total)"
    echo ""

    # Create unique crashes directory
    UNIQUE_DIR="$OUTPUT_DIR/unique"
    mkdir -p "$UNIQUE_DIR"

    for stack_hash in "${!UNIQUE_CRASHES[@]}"; do
        crash_name="${UNIQUE_CRASHES[$stack_hash]}"
        crash_type="${CRASH_TYPES[$stack_hash]}"

        # Copy representative crash
        cp "$CRASHES_DIR/$crash_name" "$UNIQUE_DIR/${crash_type}_${crash_name}"
        cp "$OUTPUT_DIR/${crash_name}.trace" "$UNIQUE_DIR/${crash_type}_${crash_name}.trace"
        cp "$OUTPUT_DIR/${crash_name}.summary" "$UNIQUE_DIR/${crash_type}_${crash_name}.summary"
    done

    log_success "Unique crashes saved to: $UNIQUE_DIR/"
fi

# Generate summary report
log_info "Generating summary report..."

REPORT_FILE="$OUTPUT_DIR/SUMMARY.txt"

cat > "$REPORT_FILE" <<EOF
============================================
Crash Triage Summary Report
============================================
Date: $(date)
Crashes Directory: $CRASHES_DIR
Target Binary: $TARGET_BINARY
Total Crashes: $CRASH_COUNT
Unique Crashes: ${UNIQUE_COUNT:-$CRASH_COUNT}

============================================
Crash Breakdown by Type
============================================
EOF

# Count by type
declare -A TYPE_COUNTS
for summary_file in "$OUTPUT_DIR"/*.summary; do
    [[ -f "$summary_file" ]] || continue
    crash_type=$(grep "^Type:" "$summary_file" | cut -d' ' -f2)
    TYPE_COUNTS[$crash_type]=$((${TYPE_COUNTS[$crash_type]:-0} + 1))
done

for crash_type in "${!TYPE_COUNTS[@]}"; do
    printf "  %-20s %d\n" "$crash_type:" "${TYPE_COUNTS[$crash_type]}" >> "$REPORT_FILE"
done

cat >> "$REPORT_FILE" <<EOF

============================================
Severity Breakdown
============================================
EOF

# Count by severity
declare -A SEVERITY_COUNTS
for summary_file in "$OUTPUT_DIR"/*.summary; do
    [[ -f "$summary_file" ]] || continue
    severity=$(grep "^Severity:" "$summary_file" | cut -d' ' -f2)
    SEVERITY_COUNTS[$severity]=$((${SEVERITY_COUNTS[$severity]:-0} + 1))
done

for severity in critical high medium low; do
    count=${SEVERITY_COUNTS[$severity]:-0}
    if [[ $count -gt 0 ]]; then
        printf "  %-20s %d\n" "$severity:" "$count" >> "$REPORT_FILE"
    fi
done

cat >> "$REPORT_FILE" <<EOF

============================================
Top Priority Crashes
============================================
EOF

# List critical/high severity crashes
for summary_file in "$OUTPUT_DIR"/*.summary; do
    [[ -f "$summary_file" ]] || continue
    severity=$(grep "^Severity:" "$summary_file" | cut -d' ' -f2)
    if [[ "$severity" == "critical" ]] || [[ "$severity" == "high" ]]; then
        crash_name=$(grep "^File:" "$summary_file" | cut -d' ' -f2)
        crash_type=$(grep "^Type:" "$summary_file" | cut -d' ' -f2)
        printf "  [%s] %s - %s\n" "$severity" "$crash_type" "$crash_name" >> "$REPORT_FILE"
    fi
done | head -20 >> "$REPORT_FILE"

log_success "Summary report saved to: $REPORT_FILE"

# Display report
echo ""
cat "$REPORT_FILE"

# Minimize crashes if requested
if [[ $MINIMIZE -eq 1 ]]; then
    echo ""
    log_info "Minimizing crash inputs..."

    # Get unique crashes if deduplicated, otherwise all crashes
    if [[ -d "$UNIQUE_DIR" ]]; then
        MINIMIZE_FILES=("$UNIQUE_DIR"/*)
    else
        MINIMIZE_FILES=("${CRASH_FILES[@]}")
    fi

    MINIMIZED_DIR="$OUTPUT_DIR/minimized"
    mkdir -p "$MINIMIZED_DIR"

    local count=0
    local total=${#MINIMIZE_FILES[@]}

    for crash_file in "${MINIMIZE_FILES[@]}"; do
        [[ -f "$crash_file" ]] || continue

        count=$((count + 1))
        crash_name=$(basename "$crash_file")
        echo -ne "\r  Minimizing: $count/$total"

        # Run LibFuzzer minimization
        "$TARGET_BINARY" \
            -minimize_crash=1 \
            -max_total_time=60 \
            -exact_artifact_path="$MINIMIZED_DIR/$crash_name" \
            "$crash_file" > /dev/null 2>&1 || true
    done
    echo ""

    log_success "Minimized crashes saved to: $MINIMIZED_DIR/"
fi

echo ""
echo "============================================"
echo "Triage Complete"
echo "============================================"
echo ""
log_info "Next steps:"
echo "  1. Review priority crashes in: $OUTPUT_DIR/unique/"
echo "  2. Read summary report: $REPORT_FILE"
echo "  3. Reproduce crashes with: ./reproduce_crash.sh <crash-file> <target>"
echo "  4. Convert to unit tests"
