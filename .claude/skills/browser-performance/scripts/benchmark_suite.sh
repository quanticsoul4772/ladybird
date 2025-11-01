#!/bin/bash
# Run performance benchmark suite and detect regressions

set -e

BUILD_DIR="${1:-./Build/release}"
BASELINE_FILE="benchmark_baseline.json"
OUTPUT_DIR="benchmark_results"
REGRESSION_THRESHOLD=5  # Percentage

echo "=== Ladybird Benchmark Suite ==="
echo "Build directory: $BUILD_DIR"
echo ""

mkdir -p "$OUTPUT_DIR"

# Find all benchmark executables
BENCHMARKS=$(find "$BUILD_DIR" -type f -name "*Benchmark*" -executable 2>/dev/null || true)

if [ -z "$BENCHMARKS" ]; then
    echo "No benchmark executables found in $BUILD_DIR"
    echo ""
    echo "Build benchmarks first:"
    echo "  cmake --build $BUILD_DIR --target all"
    exit 1
fi

echo "Found benchmarks:"
for bench in $BENCHMARKS; do
    echo "  - $(basename $bench)"
done
echo ""

# Run all benchmarks
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_FILE="$OUTPUT_DIR/results_$TIMESTAMP.json"

echo "Running benchmarks..."
echo "{"  > "$RESULTS_FILE"

FIRST=true
for bench in $BENCHMARKS; do
    BENCH_NAME=$(basename "$bench")
    echo "  Running $BENCH_NAME..."

    # Run benchmark and capture JSON output
    BENCH_OUTPUT=$("$bench" --json 2>&1 || true)

    # Append to results
    if [ "$FIRST" = true ]; then
        FIRST=false
    else
        echo "," >> "$RESULTS_FILE"
    fi

    echo "  \"$BENCH_NAME\": {" >> "$RESULTS_FILE"
    echo "    $BENCH_OUTPUT" >> "$RESULTS_FILE"
    echo "  }" >> "$RESULTS_FILE"
done

echo "}" >> "$RESULTS_FILE"

echo ""
echo "✓ Benchmark results saved: $RESULTS_FILE"
echo ""

# Compare against baseline if it exists
if [ -f "$BASELINE_FILE" ]; then
    echo "=== Comparing Against Baseline ==="
    echo ""

    # Create comparison script inline
    python3 << EOF
import json
import sys

with open("$BASELINE_FILE") as f:
    baseline = json.load(f)

with open("$RESULTS_FILE") as f:
    current = json.load(f)

regressions = []
improvements = []
threshold = $REGRESSION_THRESHOLD

for bench_name in current:
    if bench_name not in baseline:
        print(f"ℹ️  New benchmark: {bench_name}")
        continue

    current_bench = current[bench_name]
    baseline_bench = baseline[bench_name]

    for test_name in current_bench:
        if test_name not in baseline_bench:
            continue

        current_time = current_bench[test_name]
        baseline_time = baseline_bench[test_name]

        if baseline_time == 0:
            continue

        change_pct = ((current_time - baseline_time) / baseline_time) * 100

        if change_pct > threshold:
            regressions.append((bench_name, test_name, change_pct, baseline_time, current_time))
        elif change_pct < -threshold:
            improvements.append((bench_name, test_name, abs(change_pct), baseline_time, current_time))

if regressions:
    print("⚠️  Performance Regressions Detected:")
    print("")
    for bench, test, pct, baseline_t, current_t in regressions:
        print(f"  {bench}::{test}")
        print(f"    Baseline: {baseline_t:.2f}ms → Current: {current_t:.2f}ms (+{pct:.1f}%)")
    print("")

if improvements:
    print("✅ Performance Improvements:")
    print("")
    for bench, test, pct, baseline_t, current_t in improvements:
        print(f"  {bench}::{test}")
        print(f"    Baseline: {baseline_t:.2f}ms → Current: {current_t:.2f}ms (-{pct:.1f}%)")
    print("")

if not regressions and not improvements:
    print("✓ No significant performance changes")
    print("")

# Exit with error if regressions found
if regressions:
    sys.exit(1)
EOF

    COMPARISON_RESULT=$?

    if [ $COMPARISON_RESULT -ne 0 ]; then
        echo "❌ Benchmark comparison failed (regressions detected)"
        exit 1
    fi

else
    echo "No baseline found. Creating new baseline..."
    cp "$RESULTS_FILE" "$BASELINE_FILE"
    echo "✓ Baseline saved: $BASELINE_FILE"
    echo ""
    echo "Future runs will compare against this baseline."
fi

echo ""
echo "=== Benchmark Suite Complete ==="
echo ""
echo "Commands:"
echo "  View results: cat $RESULTS_FILE"
echo "  Update baseline: cp $RESULTS_FILE $BASELINE_FILE"
echo "  Compare: python3 scripts/compare_benchmarks.py $BASELINE_FILE $RESULTS_FILE"
