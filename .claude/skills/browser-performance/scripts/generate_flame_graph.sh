#!/bin/bash
# Generate flame graphs from perf.data files

set -e

PERF_DATA="${1:-perf.data}"
OUTPUT="${2:-flamegraph.svg}"

if [ ! -f "$PERF_DATA" ]; then
    echo "Error: perf data file not found: $PERF_DATA"
    echo ""
    echo "Usage: $0 <perf.data> [output.svg]"
    exit 1
fi

# Check for FlameGraph tools
if ! command -v stackcollapse-perf.pl &> /dev/null; then
    echo "Error: FlameGraph tools not found"
    echo ""
    echo "Install FlameGraph:"
    echo "  git clone https://github.com/brendangregg/FlameGraph"
    echo "  export PATH=\$PATH:\$PWD/FlameGraph"
    exit 1
fi

echo "Generating flame graph from $PERF_DATA..."

# Generate flame graph
perf script -i "$PERF_DATA" | \
    stackcollapse-perf.pl | \
    flamegraph.pl \
        --title="Ladybird Performance Flame Graph" \
        --width=1800 \
        --hash \
        > "$OUTPUT"

echo "✓ Flame graph saved to: $OUTPUT"
echo ""
echo "Open with: firefox $OUTPUT"
echo ""
echo "Tips:"
echo "  - Ctrl+F to search for functions"
echo "  - Click to zoom into stack"
echo "  - Wide flames = hot functions (CPU time)"
echo "  - Deep stacks = excessive call depth"
