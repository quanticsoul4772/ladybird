#!/bin/bash
# Sentinel Memory Leak Detection Script
# Phase 5 Days 29-30
#
# This script runs Sentinel tests with AddressSanitizer (ASAN) and LeakSanitizer
# to detect memory leaks, use-after-free, buffer overflows, and other memory issues.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "=================================="
echo "  Sentinel Memory Leak Detection"
echo "  Phase 5 Days 29-30"
echo "=================================="
echo ""

# Check if we're in the Ladybird directory
if [ ! -f "CMakeLists.txt" ]; then
    echo -e "${RED}Error: Must run from Ladybird root directory${NC}"
    exit 1
fi

# Use the official Sanitizer preset
BUILD_DIR="Build/sanitizers"
if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Sanitizer build not found. Configuring...${NC}"
    echo "Using CMake Sanitizer preset (includes vcpkg, ASAN, UBSAN)..."

    cmake --preset Sanitizer

    if [ $? -ne 0 ]; then
        echo -e "${RED}CMake configuration failed!${NC}"
        exit 1
    fi
fi

# Build the tests
echo ""
echo -e "${GREEN}Building Sentinel tests with ASAN...${NC}"
cmake --build "$BUILD_DIR" --target sentinelservice -j$(nproc) || {
    echo -e "${RED}Build failed!${NC}"
    exit 1
}

echo ""
echo "Building test executables..."
cmake --build "$BUILD_DIR" --target TestPolicyGraph -j$(nproc)
cmake --build "$BUILD_DIR" --target TestPhase3Integration -j$(nproc)
cmake --build "$BUILD_DIR" --target TestBackend -j$(nproc)
cmake --build "$BUILD_DIR" --target TestDownloadVetting -j$(nproc)

# Create results directory
RESULTS_DIR="test_results/memory_leaks"
mkdir -p "$RESULTS_DIR"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Set ASAN options
export ASAN_OPTIONS="detect_leaks=1:check_initialization_order=1:detect_stack_use_after_return=1:strict_init_order=1:halt_on_error=0:log_path=$RESULTS_DIR/asan_${TIMESTAMP}.log"
export LSAN_OPTIONS="suppressions=scripts/lsan_suppressions.txt:print_suppressions=0"

echo ""
echo -e "${GREEN}ASAN Options:${NC}"
echo "  $ASAN_OPTIONS"
echo ""

# Test results tracking
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0
LEAKS_FOUND=0

# Function to run a test with leak detection
run_leak_test() {
    local test_name="$1"
    local test_executable="$2"

    echo ""
    echo "========================================"
    echo "Running: $test_name"
    echo "========================================"

    TESTS_RUN=$((TESTS_RUN + 1))

    # Run the test and capture output
    local output_file="$RESULTS_DIR/${test_name}_${TIMESTAMP}.txt"

    if "$test_executable" > "$output_file" 2>&1; then
        # Test passed, check for leaks
        if grep -q "LeakSanitizer" "$output_file" || grep -q "ERROR: AddressSanitizer" "$output_file"; then
            echo -e "${RED}✗ $test_name: MEMORY LEAK DETECTED${NC}"
            LEAKS_FOUND=$((LEAKS_FOUND + 1))
            TESTS_FAILED=$((TESTS_FAILED + 1))

            # Show leak summary
            echo ""
            echo "Leak summary:"
            grep -A 5 "Direct leak\|Indirect leak\|ERROR: AddressSanitizer" "$output_file" | head -20
        else
            echo -e "${GREEN}✓ $test_name: PASSED (no leaks)${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
        fi
    else
        echo -e "${RED}✗ $test_name: FAILED (test error)${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))

        # Show error summary
        tail -50 "$output_file"
    fi

    echo ""
    echo "Full output saved to: $output_file"
}

# Run all tests
echo -e "${GREEN}Starting leak detection tests...${NC}"
echo ""

run_leak_test "TestPolicyGraph" "$BUILD_DIR/Services/Sentinel/TestPolicyGraph"
run_leak_test "TestPhase3Integration" "$BUILD_DIR/Services/Sentinel/TestPhase3Integration"
run_leak_test "TestBackend" "$BUILD_DIR/Services/Sentinel/TestBackend"
run_leak_test "TestDownloadVetting" "$BUILD_DIR/Services/Sentinel/TestDownloadVetting"

# Print summary
echo ""
echo "========================================"
echo "  Memory Leak Detection Summary"
echo "========================================"
echo ""
echo "Tests run:      $TESTS_RUN"
echo "Tests passed:   $TESTS_PASSED"
echo "Tests failed:   $TESTS_FAILED"
echo ""

if [ $LEAKS_FOUND -eq 0 ]; then
    echo -e "${GREEN}✓ No memory leaks detected!${NC}"
else
    echo -e "${RED}✗ Memory leaks found in $LEAKS_FOUND test(s)${NC}"
    echo ""
    echo "Review detailed reports in: $RESULTS_DIR/"
    echo ""
    echo "Common leak sources to check:"
    echo "  1. Unfreed heap allocations (missing delete/free)"
    echo "  2. Circular references in smart pointers"
    echo "  3. Static containers with heap-allocated data"
    echo "  4. Database connections not properly closed"
    echo "  5. Socket connections not closed"
fi

echo ""
echo "Full ASAN logs: $RESULTS_DIR/asan_${TIMESTAMP}.log.*"
echo ""

# Exit with appropriate code
if [ $LEAKS_FOUND -eq 0 ] && [ $TESTS_FAILED -eq 0 ]; then
    exit 0
else
    exit 1
fi
