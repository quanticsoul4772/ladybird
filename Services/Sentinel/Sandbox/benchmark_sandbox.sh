#!/bin/bash
set -euo pipefail

echo "=== Sentinel Sandbox Performance Benchmarks ==="
echo "Date: $(date)"
echo "================================================"
echo ""

# Check if TestSandbox exists
if [ ! -f "./Build/release/bin/TestSandbox" ]; then
    echo "ERROR: TestSandbox not built"
    exit 1
fi

echo "Running TestSandbox performance tests..."
echo ""

# Run tests and extract performance metrics
./Build/release/bin/TestSandbox 2>&1 | tee /tmp/benchmark_output.log

echo ""
echo "================================================"
echo "Performance Metrics Summary"
echo "================================================"
echo ""

# Extract execution times
echo "Individual Test Execution Times:"
grep "Execution time:" /tmp/benchmark_output.log || echo "No timing data found"

echo ""
echo "Analysis Times:"
grep "Analysis time:" /tmp/benchmark_output.log || echo "No timing data found"

echo ""
echo "Overall Statistics:"
grep -E "Total files analyzed|Tier 1 executions" /tmp/benchmark_output.log || echo "No statistics found"

echo ""
echo "================================================"
echo "Performance Targets:"
echo "  - Target: <100ms per analysis (production)"
echo "  - Stub mode typical: <5ms per analysis"
echo "  - Unit test overhead: ~1ms"
echo "================================================"
echo ""

# Calculate average if multiple timings exist
TIMINGS=$(grep -o "time: [0-9]* ms" /tmp/benchmark_output.log | grep -o "[0-9]*" || echo "")
if [ -n "$TIMINGS" ]; then
    TOTAL=0
    COUNT=0
    while IFS= read -r TIME; do
        TOTAL=$((TOTAL + TIME))
        COUNT=$((COUNT + 1))
    done <<< "$TIMINGS"

    if [ $COUNT -gt 0 ]; then
        AVG=$((TOTAL / COUNT))
        echo "Calculated Metrics:"
        echo "  - Total measurements: $COUNT"
        echo "  - Average time: ${AVG}ms"
        echo "  - Total time: ${TOTAL}ms"

        if [ $AVG -lt 100 ]; then
            echo "  - Status: ✅ PASS (within 100ms target)"
        else
            echo "  - Status: ⚠️  WARN (exceeds 100ms target)"
        fi
    fi
fi

echo ""
echo "================================================"
echo "Benchmark complete"
echo "================================================"
