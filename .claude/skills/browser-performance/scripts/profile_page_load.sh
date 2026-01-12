#!/bin/bash
# Profile Ladybird loading a specific URL and analyze performance

set -e

URL="${1:-https://example.com}"
BUILD_DIR="${BUILD_DIR:-./Build/release}"
LADYBIRD_BIN="$BUILD_DIR/bin/Ladybird"
OUTPUT_DIR="page_load_profile"
PROFILER="${PROFILER:-perf}"  # perf, callgrind, heaptrack

echo "=== Ladybird Page Load Profiling ==="
echo "URL: $URL"
echo "Profiler: $PROFILER"
echo "Output: $OUTPUT_DIR/"
echo ""

mkdir -p "$OUTPUT_DIR"

case "$PROFILER" in
    perf)
        echo "Running CPU profiling with Linux perf..."
        if ! command -v perf &> /dev/null; then
            echo "Error: perf not found. Install with: sudo apt install linux-tools-generic"
            exit 1
        fi

        # Record CPU profile
        perf record \
            -F 999 \
            -g \
            --call-graph=dwarf \
            -o "$OUTPUT_DIR/perf.data" \
            -- "$LADYBIRD_BIN" "$URL"

        # Generate text report
        echo ""
        echo "Top CPU hotspots:"
        perf report --stdio -i "$OUTPUT_DIR/perf.data" | head -40

        # Generate flame graph if tools available
        if command -v stackcollapse-perf.pl &> /dev/null; then
            echo ""
            echo "Generating flame graph..."
            perf script -i "$OUTPUT_DIR/perf.data" | \
                stackcollapse-perf.pl | \
                flamegraph.pl > "$OUTPUT_DIR/flamegraph.svg"
            echo "✓ Flame graph saved: $OUTPUT_DIR/flamegraph.svg"
        fi

        echo ""
        echo "Interactive analysis: perf report -i $OUTPUT_DIR/perf.data"
        ;;

    callgrind)
        echo "Running call graph profiling with Valgrind callgrind..."
        if ! command -v valgrind &> /dev/null; then
            echo "Error: valgrind not found. Install with: sudo apt install valgrind"
            exit 1
        fi

        # Run with callgrind
        valgrind \
            --tool=callgrind \
            --callgrind-out-file="$OUTPUT_DIR/callgrind.out" \
            --dump-instr=yes \
            --collect-jumps=yes \
            --cache-sim=yes \
            --branch-sim=yes \
            "$LADYBIRD_BIN" "$URL"

        echo ""
        echo "✓ Callgrind output saved: $OUTPUT_DIR/callgrind.out"
        echo ""

        if command -v callgrind_annotate &> /dev/null; then
            echo "Top functions by instruction count:"
            callgrind_annotate "$OUTPUT_DIR/callgrind.out" | head -40
        fi

        echo ""
        echo "Visualize with: kcachegrind $OUTPUT_DIR/callgrind.out"
        ;;

    heaptrack)
        echo "Running heap memory profiling with Heaptrack..."
        if ! command -v heaptrack &> /dev/null; then
            echo "Error: heaptrack not found. Install with: sudo apt install heaptrack"
            exit 1
        fi

        # Run with heaptrack
        cd "$OUTPUT_DIR"
        heaptrack "$LADYBIRD_BIN" "$URL"
        cd - > /dev/null

        HEAPTRACK_FILE=$(ls -t "$OUTPUT_DIR"/heaptrack.*.gz | head -1)

        echo ""
        echo "✓ Heaptrack output saved: $HEAPTRACK_FILE"
        echo ""

        if command -v heaptrack_print &> /dev/null; then
            echo "Memory usage summary:"
            heaptrack_print "$HEAPTRACK_FILE" | head -50
        fi

        echo ""
        echo "Visualize with: heaptrack_gui $HEAPTRACK_FILE"
        ;;

    massif)
        echo "Running heap profiling with Valgrind massif..."
        if ! command -v valgrind &> /dev/null; then
            echo "Error: valgrind not found. Install with: sudo apt install valgrind"
            exit 1
        fi

        # Run with massif
        valgrind \
            --tool=massif \
            --massif-out-file="$OUTPUT_DIR/massif.out" \
            --time-unit=ms \
            --detailed-freq=1 \
            --max-snapshots=100 \
            "$LADYBIRD_BIN" "$URL"

        echo ""
        echo "✓ Massif output saved: $OUTPUT_DIR/massif.out"
        echo ""

        if command -v ms_print &> /dev/null; then
            echo "Memory usage over time:"
            ms_print "$OUTPUT_DIR/massif.out" | head -80
        fi

        echo ""
        echo "Visualize with: massif-visualizer $OUTPUT_DIR/massif.out"
        ;;

    *)
        echo "Error: Unknown profiler '$PROFILER'"
        echo ""
        echo "Usage: PROFILER=<tool> $0 <url>"
        echo ""
        echo "Available profilers:"
        echo "  perf       - CPU profiling (Linux only)"
        echo "  callgrind  - Call graph profiling"
        echo "  heaptrack  - Heap memory profiling"
        echo "  massif     - Heap snapshot profiling"
        exit 1
        ;;
esac

echo ""
echo "=== Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR/"
