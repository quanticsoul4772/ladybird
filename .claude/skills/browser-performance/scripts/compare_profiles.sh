#!/bin/bash
# Compare performance profiles before and after changes

set -e

BEFORE="${1}"
AFTER="${2}"
OUTPUT_DIR="profile_comparison"

if [ -z "$BEFORE" ] || [ -z "$AFTER" ]; then
    echo "Usage: $0 <before_profile> <after_profile>"
    echo ""
    echo "Examples:"
    echo "  $0 perf.data.old perf.data.new"
    echo "  $0 callgrind.out.before callgrind.out.after"
    echo "  $0 baseline.json current.json"
    exit 1
fi

if [ ! -f "$BEFORE" ] || [ ! -f "$AFTER" ]; then
    echo "Error: Profile files not found"
    exit 1
fi

echo "=== Performance Profile Comparison ==="
echo "Before: $BEFORE"
echo "After:  $AFTER"
echo ""

mkdir -p "$OUTPUT_DIR"

# Detect profile type
if [[ "$BEFORE" == *.data ]]; then
    PROFILE_TYPE="perf"
elif [[ "$BEFORE" == callgrind.* ]]; then
    PROFILE_TYPE="callgrind"
elif [[ "$BEFORE" == *.json ]]; then
    PROFILE_TYPE="benchmark"
else
    echo "Error: Unknown profile type"
    exit 1
fi

case "$PROFILE_TYPE" in
    perf)
        echo "Profile type: Linux perf"
        echo ""

        # Generate differential flame graph
        if command -v stackcollapse-perf.pl &> /dev/null; then
            echo "Generating differential flame graph..."

            perf script -i "$BEFORE" | \
                stackcollapse-perf.pl > "$OUTPUT_DIR/before.folded"

            perf script -i "$AFTER" | \
                stackcollapse-perf.pl > "$OUTPUT_DIR/after.folded"

            if command -v difffolded.pl &> /dev/null; then
                difffolded.pl \
                    "$OUTPUT_DIR/before.folded" \
                    "$OUTPUT_DIR/after.folded" | \
                    flamegraph.pl \
                        --title="Performance Diff (red=regression, blue=improvement)" \
                        --width=1800 \
                        > "$OUTPUT_DIR/diff_flamegraph.svg"

                echo "✓ Differential flame graph: $OUTPUT_DIR/diff_flamegraph.svg"
                echo "  Red = Regressions (slower)"
                echo "  Blue = Improvements (faster)"
            else
                echo "⚠️  difffolded.pl not found, generating separate flame graphs..."

                flamegraph.pl \
                    --title="Before" \
                    < "$OUTPUT_DIR/before.folded" \
                    > "$OUTPUT_DIR/before_flame.svg"

                flamegraph.pl \
                    --title="After" \
                    < "$OUTPUT_DIR/after.folded" \
                    > "$OUTPUT_DIR/after_flame.svg"

                echo "✓ Before: $OUTPUT_DIR/before_flame.svg"
                echo "✓ After:  $OUTPUT_DIR/after_flame.svg"
            fi
        fi

        echo ""

        # Generate text comparison
        echo "=== Top Function Comparison ==="
        echo ""
        echo "BEFORE:"
        perf report --stdio -i "$BEFORE" | head -30
        echo ""
        echo "AFTER:"
        perf report --stdio -i "$AFTER" | head -30

        # Use perf diff if available
        if command -v perf &> /dev/null; then
            echo ""
            echo "=== Perf Diff Analysis ==="
            perf diff "$BEFORE" "$AFTER" | head -50
        fi
        ;;

    callgrind)
        echo "Profile type: Callgrind"
        echo ""

        if ! command -v callgrind_annotate &> /dev/null; then
            echo "Error: callgrind_annotate not found"
            exit 1
        fi

        echo "=== Instruction Count Comparison ==="
        echo ""

        echo "BEFORE:"
        callgrind_annotate "$BEFORE" | head -40 > "$OUTPUT_DIR/before.txt"
        cat "$OUTPUT_DIR/before.txt"

        echo ""
        echo "AFTER:"
        callgrind_annotate "$AFTER" | head -40 > "$OUTPUT_DIR/after.txt"
        cat "$OUTPUT_DIR/after.txt"

        echo ""
        echo "Text reports saved:"
        echo "  $OUTPUT_DIR/before.txt"
        echo "  $OUTPUT_DIR/after.txt"

        # Try to extract total instruction counts
        BEFORE_COUNT=$(callgrind_annotate "$BEFORE" | grep "Ir" | head -1 | awk '{print $1}' | tr -d ',')
        AFTER_COUNT=$(callgrind_annotate "$AFTER" | grep "Ir" | head -1 | awk '{print $1}' | tr -d ',')

        if [ -n "$BEFORE_COUNT" ] && [ -n "$AFTER_COUNT" ]; then
            echo ""
            echo "=== Total Instruction Count ==="
            echo "Before: $BEFORE_COUNT"
            echo "After:  $AFTER_COUNT"

            DIFF=$((AFTER_COUNT - BEFORE_COUNT))
            PCT=$(echo "scale=2; ($DIFF * 100) / $BEFORE_COUNT" | bc)

            if [ $DIFF -gt 0 ]; then
                echo "Change: +$DIFF (+${PCT}%) ⚠️  REGRESSION"
            else
                echo "Change: $DIFF (${PCT}%) ✅ IMPROVEMENT"
            fi
        fi
        ;;

    benchmark)
        echo "Profile type: Benchmark JSON"
        echo ""

        # Python comparison script
        python3 << EOF
import json
import sys

with open("$BEFORE") as f:
    before = json.load(f)

with open("$AFTER") as f:
    after = json.load(f)

print("=== Benchmark Comparison ===")
print("")

regressions = []
improvements = []
unchanged = []

for name in sorted(set(before.keys()) | set(after.keys())):
    if name not in before:
        print(f"➕ NEW: {name}")
        continue
    if name not in after:
        print(f"➖ REMOVED: {name}")
        continue

    before_time = before[name]
    after_time = after[name]

    if before_time == 0:
        continue

    change_pct = ((after_time - before_time) / before_time) * 100

    if abs(change_pct) < 1:
        unchanged.append((name, before_time, after_time))
    elif change_pct > 0:
        regressions.append((name, before_time, after_time, change_pct))
    else:
        improvements.append((name, before_time, after_time, abs(change_pct)))

if regressions:
    print("")
    print("⚠️  REGRESSIONS (slower):")
    for name, before_t, after_t, pct in sorted(regressions, key=lambda x: -x[3]):
        print(f"  {name}")
        print(f"    {before_t:.2f}ms → {after_t:.2f}ms (+{pct:.1f}%)")

if improvements:
    print("")
    print("✅ IMPROVEMENTS (faster):")
    for name, before_t, after_t, pct in sorted(improvements, key=lambda x: -x[3]):
        print(f"  {name}")
        print(f"    {before_t:.2f}ms → {after_t:.2f}ms (-{pct:.1f}%)")

if unchanged:
    print("")
    print(f"✓ {len(unchanged)} benchmarks unchanged (< 1% difference)")

# Summary
print("")
print("=== Summary ===")
print(f"Regressions:  {len(regressions)}")
print(f"Improvements: {len(improvements)}")
print(f"Unchanged:    {len(unchanged)}")

# Exit with error if regressions
if regressions:
    sys.exit(1)
EOF
        ;;
esac

echo ""
echo "=== Comparison Complete ==="
echo "Output directory: $OUTPUT_DIR/"
