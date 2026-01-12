#!/bin/bash
# Example: Complete Linux perf workflow for profiling Ladybird

set -e

# Configuration
URL="${1:-https://example.com}"
BUILD_DIR="./Build/release"
LADYBIRD_BIN="$BUILD_DIR/bin/Ladybird"
OUTPUT_DIR="perf_results"

echo "=== Ladybird Performance Profiling Workflow ==="
echo "URL: $URL"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Step 1: Basic CPU profiling
echo "[Step 1] Recording CPU profile..."
perf record \
    -F 99 \
    -g \
    --call-graph=dwarf \
    -o "$OUTPUT_DIR/perf.data" \
    -- "$LADYBIRD_BIN" "$URL"

echo "✓ Profile recorded to $OUTPUT_DIR/perf.data"
echo ""

# Step 2: Generate text report
echo "[Step 2] Generating text report (top hotspots)..."
perf report \
    --stdio \
    -i "$OUTPUT_DIR/perf.data" \
    --sort=cpu \
    > "$OUTPUT_DIR/perf_report.txt"

echo "Top 10 hotspots:"
head -30 "$OUTPUT_DIR/perf_report.txt" | tail -20
echo ""
echo "✓ Full report saved to $OUTPUT_DIR/perf_report.txt"
echo ""

# Step 3: Generate flame graph
echo "[Step 3] Generating flame graph..."
if command -v stackcollapse-perf.pl &> /dev/null; then
    perf script -i "$OUTPUT_DIR/perf.data" | \
        stackcollapse-perf.pl | \
        flamegraph.pl > "$OUTPUT_DIR/flamegraph.svg"
    echo "✓ Flame graph saved to $OUTPUT_DIR/flamegraph.svg"
else
    echo "⚠️  FlameGraph scripts not found. Install from:"
    echo "   git clone https://github.com/brendangregg/FlameGraph"
    echo "   export PATH=\$PATH:\$PWD/FlameGraph"
fi
echo ""

# Step 4: Annotate hot functions
echo "[Step 4] Finding hottest function for annotation..."
HOTTEST_FUNC=$(perf report --stdio -i "$OUTPUT_DIR/perf.data" | \
    grep -A 1 "^#" | \
    grep "%" | \
    head -1 | \
    awk '{print $NF}')

if [ -n "$HOTTEST_FUNC" ]; then
    echo "Hottest function: $HOTTEST_FUNC"
    echo "Generating annotated assembly..."
    perf annotate \
        -i "$OUTPUT_DIR/perf.data" \
        -s "$HOTTEST_FUNC" \
        > "$OUTPUT_DIR/annotate_${HOTTEST_FUNC}.txt" 2>/dev/null || true
    echo "✓ Annotation saved to $OUTPUT_DIR/annotate_${HOTTEST_FUNC}.txt"
fi
echo ""

# Step 5: Summary statistics
echo "[Step 5] Performance statistics..."
perf stat \
    "$LADYBIRD_BIN" "$URL" \
    2>&1 | tee "$OUTPUT_DIR/perf_stat.txt"

echo ""
echo "=== Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR/"
echo ""
echo "Next steps:"
echo "  1. View flame graph: firefox $OUTPUT_DIR/flamegraph.svg"
echo "  2. Review report: less $OUTPUT_DIR/perf_report.txt"
echo "  3. Analyze statistics: cat $OUTPUT_DIR/perf_stat.txt"
echo "  4. Interactive analysis: perf report -i $OUTPUT_DIR/perf.data"
