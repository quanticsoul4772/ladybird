#!/bin/bash
# Example: Generating various types of flame graphs for Ladybird

set -e

PERF_DATA="${1:-perf.data}"
OUTPUT_DIR="flame_graphs"

echo "=== Flame Graph Generation for Ladybird ==="
echo "Input: $PERF_DATA"
echo ""

mkdir -p "$OUTPUT_DIR"

# Check for FlameGraph tools
if ! command -v stackcollapse-perf.pl &> /dev/null; then
    echo "Error: FlameGraph tools not found"
    echo ""
    echo "Install with:"
    echo "  git clone https://github.com/brendangregg/FlameGraph"
    echo "  export PATH=\$PATH:\$PWD/FlameGraph"
    exit 1
fi

# 1. Standard CPU flame graph
echo "[1] Generating standard CPU flame graph..."
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    flamegraph.pl \
        --title="Ladybird CPU Flame Graph" \
        --width=1800 \
        --hash \
        > "$OUTPUT_DIR/cpu_flamegraph.svg"
echo "✓ Saved: $OUTPUT_DIR/cpu_flamegraph.svg"
echo ""

# 2. Inverted flame graph (icicle graph)
echo "[2] Generating inverted flame graph (icicle)..."
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    flamegraph.pl \
        --inverted \
        --title="Ladybird Icicle Graph" \
        --width=1800 \
        > "$OUTPUT_DIR/icicle_graph.svg"
echo "✓ Saved: $OUTPUT_DIR/icicle_graph.svg"
echo ""

# 3. Differential flame graph (requires two perf.data files)
if [ -f "perf.data.old" ]; then
    echo "[3] Generating differential flame graph..."

    # Generate baseline folded stacks
    perf script -i perf.data.old | \
        stackcollapse-perf.pl > "$OUTPUT_DIR/baseline.folded"

    # Generate current folded stacks
    perf script -i "$PERF_DATA" | \
        stackcollapse-perf.pl > "$OUTPUT_DIR/current.folded"

    # Generate differential flame graph
    difffolded.pl \
        "$OUTPUT_DIR/baseline.folded" \
        "$OUTPUT_DIR/current.folded" | \
        flamegraph.pl \
            --title="Ladybird Performance Diff (red=regression, blue=improvement)" \
            --width=1800 \
            > "$OUTPUT_DIR/diff_flamegraph.svg"

    echo "✓ Saved: $OUTPUT_DIR/diff_flamegraph.svg"
    echo "  Red flames: Regressions (slower)"
    echo "  Blue flames: Improvements (faster)"
else
    echo "[3] Skipping differential flame graph (perf.data.old not found)"
fi
echo ""

# 4. Filtered flame graphs (focus on specific subsystems)
echo "[4] Generating subsystem-specific flame graphs..."

# LibWeb rendering subsystem
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    grep -i "LibWeb\|layout\|paint\|render" | \
    flamegraph.pl \
        --title="LibWeb Rendering Subsystem" \
        --width=1800 \
        > "$OUTPUT_DIR/libweb_rendering.svg"
echo "✓ Saved: $OUTPUT_DIR/libweb_rendering.svg (LibWeb rendering)"

# JavaScript execution subsystem
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    grep -i "LibJS\|javascript\|bytecode\|jit" | \
    flamegraph.pl \
        --title="JavaScript Execution Subsystem" \
        --width=1800 \
        > "$OUTPUT_DIR/javascript_execution.svg"
echo "✓ Saved: $OUTPUT_DIR/javascript_execution.svg (JavaScript)"

# Networking subsystem
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    grep -i "RequestServer\|HTTP\|TLS\|socket" | \
    flamegraph.pl \
        --title="Networking Subsystem" \
        --width=1800 \
        > "$OUTPUT_DIR/networking.svg"
echo "✓ Saved: $OUTPUT_DIR/networking.svg (Networking)"
echo ""

# 5. Off-CPU flame graph (requires off-CPU profiling data)
if [ -f "off_cpu_perf.data" ]; then
    echo "[5] Generating off-CPU flame graph..."
    perf script -i off_cpu_perf.data | \
        stackcollapse-perf.pl | \
        flamegraph.pl \
            --title="Ladybird Off-CPU Time" \
            --color=io \
            --width=1800 \
            > "$OUTPUT_DIR/off_cpu_flamegraph.svg"
    echo "✓ Saved: $OUTPUT_DIR/off_cpu_flamegraph.svg"
else
    echo "[5] Skipping off-CPU flame graph (off_cpu_perf.data not found)"
    echo "    To generate: perf record -e sched:sched_switch -g -a -- ./Ladybird"
fi
echo ""

echo "=== Flame Graph Generation Complete ==="
echo "Output directory: $OUTPUT_DIR/"
echo ""
echo "Viewing tips:"
echo "  - Use Ctrl+F to search for function names"
echo "  - Click frames to zoom in"
echo "  - Right-click to reset zoom"
echo "  - Wide flames = hot code paths"
echo "  - Deep stacks = excessive call depth"
echo ""
echo "Open in browser:"
echo "  firefox $OUTPUT_DIR/cpu_flamegraph.svg"
