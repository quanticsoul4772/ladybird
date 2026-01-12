#!/usr/bin/env bash
# Check for Performance Regressions in Ladybird
# Compares benchmark results between base and PR branches

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

log_section() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$*${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Configuration
BASE_RESULTS="${BASE_RESULTS:-base-benchmarks.json}"
PR_RESULTS="${PR_RESULTS:-pr-benchmarks.json}"
THRESHOLD="${THRESHOLD:-5.0}"  # Percentage regression threshold
OUTPUT_REPORT="${OUTPUT_REPORT:-regression-report.md}"

REGRESSIONS_FOUND=0
IMPROVEMENTS_FOUND=0

# Parse JSON benchmarks
parse_benchmarks() {
    local json_file="$1"

    if [ ! -f "$json_file" ]; then
        log_error "Benchmark file not found: $json_file"
        return 1
    fi

    # Extract benchmark data using jq (if available) or python
    if command -v jq &> /dev/null; then
        jq -r '.benchmarks[] | "\(.name)|\(.value)|\(.unit)"' "$json_file"
    elif command -v python3 &> /dev/null; then
        python3 -c "
import json, sys
data = json.load(open('$json_file'))
for b in data['benchmarks']:
    print(f\"{b['name']}|{b['value']}|{b['unit']}\")
"
    else
        log_error "Neither jq nor python3 found. Cannot parse benchmarks."
        return 1
    fi
}

# Compare two benchmark values
compare_benchmarks() {
    local name="$1"
    local base_value="$2"
    local pr_value="$3"
    local unit="$4"

    # Calculate percentage change
    local change=$(awk "BEGIN {
        if ($base_value == 0) { print 0 }
        else { print (($pr_value - $base_value) / $base_value) * 100 }
    }")

    local abs_change=${change#-}  # Absolute value

    # Determine if it's a regression
    local status="✓ OK"
    local color="$GREEN"

    if (( $(echo "$abs_change > $THRESHOLD" | bc -l) )); then
        if (( $(echo "$change > 0" | bc -l) )); then
            # Slower is regression (for time metrics)
            if [[ "$unit" == "ms" ]] || [[ "$unit" == "s" ]]; then
                status="❌ REGRESSION"
                color="$RED"
                REGRESSIONS_FOUND=$((REGRESSIONS_FOUND + 1))
            else
                status="✅ IMPROVEMENT"
                color="$GREEN"
                IMPROVEMENTS_FOUND=$((IMPROVEMENTS_FOUND + 1))
            fi
        else
            # Faster is improvement (for time metrics)
            if [[ "$unit" == "ms" ]] || [[ "$unit" == "s" ]]; then
                status="✅ IMPROVEMENT"
                color="$GREEN"
                IMPROVEMENTS_FOUND=$((IMPROVEMENTS_FOUND + 1))
            else
                status="❌ REGRESSION"
                color="$RED"
                REGRESSIONS_FOUND=$((REGRESSIONS_FOUND + 1))
            fi
        fi
    fi

    # Print comparison
    printf "%-50s %10s -> %10s (%+.2f%%) %s\n" \
        "$name" \
        "$base_value$unit" \
        "$pr_value$unit" \
        "$change" \
        "$status"

    # Append to report
    printf "| %-50s | %10s | %10s | %+.2f%% | %s |\n" \
        "$name" \
        "$base_value$unit" \
        "$pr_value$unit" \
        "$change" \
        "$status" >> "$OUTPUT_REPORT"
}

# Generate markdown report
generate_report() {
    log_section "Generating Performance Report"

    cat > "$OUTPUT_REPORT" <<EOF
# Performance Comparison Report

**Date**: $(date -u +"%Y-%m-%d %H:%M UTC")
**Threshold**: ${THRESHOLD}% change
**Base**: $BASE_RESULTS
**PR**: $PR_RESULTS

## Summary

- **Regressions**: $REGRESSIONS_FOUND
- **Improvements**: $IMPROVEMENTS_FOUND
- **Status**: $([ $REGRESSIONS_FOUND -eq 0 ] && echo "✅ PASSED" || echo "❌ FAILED")

## Detailed Results

| Benchmark | Base | PR | Change | Status |
|-----------|------|----|---------:|--------|
EOF

    log_info "Report initialized: $OUTPUT_REPORT"
}

# Compare all benchmarks
compare_all() {
    log_section "Comparing Benchmarks"

    # Parse base benchmarks
    declare -A base_benchmarks
    while IFS='|' read -r name value unit; do
        base_benchmarks["$name"]="$value|$unit"
    done < <(parse_benchmarks "$BASE_RESULTS")

    # Parse PR benchmarks and compare
    while IFS='|' read -r name value unit; do
        if [ -n "${base_benchmarks[$name]:-}" ]; then
            IFS='|' read -r base_value base_unit <<< "${base_benchmarks[$name]}"

            if [ "$unit" != "$base_unit" ]; then
                log_warn "Unit mismatch for $name: $base_unit vs $unit"
                continue
            fi

            compare_benchmarks "$name" "$base_value" "$value" "$unit"
        else
            log_warn "Benchmark not found in base: $name"
            printf "| %-50s | %10s | %10s | %8s | %s |\n" \
                "$name" \
                "N/A" \
                "$value$unit" \
                "NEW" \
                "ℹ️ NEW" >> "$OUTPUT_REPORT"
        fi
    done < <(parse_benchmarks "$PR_RESULTS")

    # Check for removed benchmarks
    for name in "${!base_benchmarks[@]}"; do
        # Check if this benchmark exists in PR results
        if ! parse_benchmarks "$PR_RESULTS" | grep -q "^$name|"; then
            IFS='|' read -r base_value base_unit <<< "${base_benchmarks[$name]}"
            log_warn "Benchmark removed: $name"
            printf "| %-50s | %10s | %10s | %8s | %s |\n" \
                "$name" \
                "$base_value$base_unit" \
                "N/A" \
                "REMOVED" \
                "⚠️ REMOVED" >> "$OUTPUT_REPORT"
        fi
    done
}

# Add recommendations
add_recommendations() {
    cat >> "$OUTPUT_REPORT" <<'EOF'

## Recommendations

EOF

    if [ $REGRESSIONS_FOUND -gt 0 ]; then
        cat >> "$OUTPUT_REPORT" <<EOF
### ❌ Regressions Detected

This PR introduces $REGRESSIONS_FOUND performance regression(s) exceeding the ${THRESHOLD}% threshold.

**Action Required**:
1. Review the regressed benchmarks above
2. Profile the affected code paths
3. Optimize or justify the regression
4. Re-run benchmarks after fixes

### Profiling

\`\`\`bash
# Profile with perf
perf record -F 99 -g ./Build/release/bin/Ladybird --benchmark <regressed_test>
perf report

# Generate flame graph
perf script | flamegraph.pl > flamegraph.svg
\`\`\`
EOF
    else
        cat >> "$OUTPUT_REPORT" <<EOF
### ✅ No Regressions

No significant performance regressions detected. Good work!
EOF
    fi

    if [ $IMPROVEMENTS_FOUND -gt 0 ]; then
        cat >> "$OUTPUT_REPORT" <<EOF

### ✅ Performance Improvements

This PR includes $IMPROVEMENTS_FOUND performance improvement(s). Great job!
EOF
    fi
}

# Main
main() {
    log_info "Checking for performance regressions..."
    log_info "Base results: $BASE_RESULTS"
    log_info "PR results: $PR_RESULTS"
    log_info "Threshold: ${THRESHOLD}%"
    echo ""

    # Verify files exist
    if [ ! -f "$BASE_RESULTS" ]; then
        log_error "Base results not found: $BASE_RESULTS"
        exit 1
    fi

    if [ ! -f "$PR_RESULTS" ]; then
        log_error "PR results not found: $PR_RESULTS"
        exit 1
    fi

    # Generate report
    generate_report
    compare_all
    add_recommendations

    # Print summary
    log_section "Summary"
    echo ""
    echo "Regressions:  $REGRESSIONS_FOUND"
    echo "Improvements: $IMPROVEMENTS_FOUND"
    echo ""

    if [ $REGRESSIONS_FOUND -gt 0 ]; then
        log_error "❌ Performance regressions detected!"
        log_info "Full report: $OUTPUT_REPORT"
        exit 1
    else
        log_info "✅ No performance regressions detected"
        log_info "Full report: $OUTPUT_REPORT"
        exit 0
    fi
}

# Handle arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --base)
            BASE_RESULTS="$2"
            shift 2
            ;;
        --pr)
            PR_RESULTS="$2"
            shift 2
            ;;
        --threshold)
            THRESHOLD="$2"
            shift 2
            ;;
        --output)
            OUTPUT_REPORT="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --base FILE        Base benchmark results (JSON)"
            echo "  --pr FILE          PR benchmark results (JSON)"
            echo "  --threshold PCT    Regression threshold percentage (default: 5.0)"
            echo "  --output FILE      Output report file (default: regression-report.md)"
            echo "  --help             Show this help"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

main "$@"
