#!/bin/bash
# Memory profiling for Ladybird using multiple tools

set -e

TARGET="${1:-Ladybird}"
BUILD_DIR="${BUILD_DIR:-./Build/release}"
TARGET_BIN="$BUILD_DIR/bin/$TARGET"
OUTPUT_DIR="memory_profile"
TOOL="${TOOL:-heaptrack}"  # heaptrack, massif, asan

echo "=== Ladybird Memory Profiling ==="
echo "Target: $TARGET"
echo "Tool: $TOOL"
echo "Output: $OUTPUT_DIR/"
echo ""

mkdir -p "$OUTPUT_DIR"

case "$TOOL" in
    heaptrack)
        if ! command -v heaptrack &> /dev/null; then
            echo "Error: heaptrack not found"
            echo "Install: sudo apt install heaptrack heaptrack-gui"
            exit 1
        fi

        echo "Running heaptrack..."
        cd "$OUTPUT_DIR"
        heaptrack "$TARGET_BIN" "${@:2}"
        cd - > /dev/null

        HEAPTRACK_FILE=$(ls -t "$OUTPUT_DIR"/heaptrack.*.gz | head -1)

        echo ""
        echo "✓ Profile saved: $HEAPTRACK_FILE"
        echo ""

        # Print summary
        if command -v heaptrack_print &> /dev/null; then
            echo "=== Memory Usage Summary ==="
            heaptrack_print "$HEAPTRACK_FILE" | head -100
        fi

        echo ""
        echo "Analyze with: heaptrack_gui $HEAPTRACK_FILE"
        ;;

    massif)
        if ! command -v valgrind &> /dev/null; then
            echo "Error: valgrind not found"
            echo "Install: sudo apt install valgrind"
            exit 1
        fi

        echo "Running Valgrind massif..."
        valgrind \
            --tool=massif \
            --massif-out-file="$OUTPUT_DIR/massif.out" \
            --time-unit=ms \
            --detailed-freq=1 \
            --max-snapshots=200 \
            --threshold=0.1 \
            "$TARGET_BIN" "${@:2}"

        echo ""
        echo "✓ Profile saved: $OUTPUT_DIR/massif.out"
        echo ""

        # Print summary
        if command -v ms_print &> /dev/null; then
            echo "=== Memory Usage Over Time ==="
            ms_print "$OUTPUT_DIR/massif.out" | head -100
        fi

        echo ""
        echo "Analyze with: massif-visualizer $OUTPUT_DIR/massif.out"
        ;;

    asan)
        # Rebuild with ASAN if needed
        ASAN_BUILD_DIR="./Build/sanitizers"
        ASAN_BIN="$ASAN_BUILD_DIR/bin/$TARGET"

        if [ ! -f "$ASAN_BIN" ]; then
            echo "ASAN build not found. Building with Sanitizers preset..."
            cmake --preset Sanitizers
            cmake --build "$ASAN_BUILD_DIR" --target "$TARGET"
        fi

        echo "Running with AddressSanitizer..."
        ASAN_OPTIONS="detect_leaks=1:log_path=$OUTPUT_DIR/asan.log:print_stats=1" \
        LSAN_OPTIONS="report_objects=1" \
            "$ASAN_BIN" "${@:2}"

        echo ""
        if [ -f "$OUTPUT_DIR/asan.log" ]; then
            echo "=== ASAN Report ==="
            cat "$OUTPUT_DIR/asan.log"*
            echo ""
            echo "✓ ASAN logs saved: $OUTPUT_DIR/asan.log.*"
        else
            echo "✓ No memory errors detected"
        fi
        ;;

    valgrind)
        if ! command -v valgrind &> /dev/null; then
            echo "Error: valgrind not found"
            echo "Install: sudo apt install valgrind"
            exit 1
        fi

        echo "Running Valgrind memcheck..."
        valgrind \
            --tool=memcheck \
            --leak-check=full \
            --show-leak-kinds=all \
            --track-origins=yes \
            --verbose \
            --log-file="$OUTPUT_DIR/valgrind.log" \
            "$TARGET_BIN" "${@:2}"

        echo ""
        echo "=== Valgrind Report ==="
        cat "$OUTPUT_DIR/valgrind.log"
        echo ""
        echo "✓ Full report: $OUTPUT_DIR/valgrind.log"
        ;;

    *)
        echo "Error: Unknown tool '$TOOL'"
        echo ""
        echo "Usage: TOOL=<tool> $0 <target> [args...]"
        echo ""
        echo "Available tools:"
        echo "  heaptrack  - Heap memory profiler (recommended)"
        echo "  massif     - Valgrind heap profiler"
        echo "  asan       - AddressSanitizer leak detection"
        echo "  valgrind   - Valgrind memcheck"
        exit 1
        ;;
esac

echo ""
echo "=== Memory Profiling Complete ==="
