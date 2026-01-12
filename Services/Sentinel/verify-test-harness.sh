#!/bin/bash
# Verification script for BehavioralAnalyzer test harness
# Run from repository root: ./Services/Sentinel/verify-test-harness.sh

set -e

echo "========================================"
echo "BehavioralAnalyzer Test Harness Verification"
echo "========================================"
echo ""

ERRORS=0

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

check_file() {
    local file=$1
    local desc=$2
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $desc"
        echo "  Path: $file"
        echo "  Size: $(du -h "$file" | cut -f1)"
        echo "  Lines: $(wc -l < "$file")"
        echo ""
    else
        echo -e "${RED}✗${NC} $desc"
        echo "  Missing: $file"
        echo ""
        ERRORS=$((ERRORS + 1))
    fi
}

check_dir() {
    local dir=$1
    local desc=$2
    if [ -d "$dir" ]; then
        echo -e "${GREEN}✓${NC} $desc"
        echo "  Path: $dir"
        echo "  Files: $(find "$dir" -type f | wc -l)"
        echo ""
    else
        echo -e "${RED}✗${NC} $desc"
        echo "  Missing: $dir"
        echo ""
        ERRORS=$((ERRORS + 1))
    fi
}

check_executable() {
    local file=$1
    local desc=$2
    if [ -f "$file" ] && [ -x "$file" ]; then
        echo -e "${GREEN}✓${NC} $desc"
        echo "  Path: $file"
        echo ""
    else
        echo -e "${RED}✗${NC} $desc"
        if [ -f "$file" ]; then
            echo "  File exists but not executable: $file"
        else
            echo "  Missing: $file"
        fi
        echo ""
        ERRORS=$((ERRORS + 1))
    fi
}

echo "1. Directory Structure"
echo "--------------------"
check_dir "Services/Sentinel/Test" "Test directory"
check_dir "Services/Sentinel/Test/behavioral" "Behavioral test directory"
check_dir "Services/Sentinel/Test/behavioral/benign" "Benign test files directory"
check_dir "Services/Sentinel/Test/behavioral/malicious" "Malicious test files directory"

echo "2. Test Files"
echo "-------------"
check_file "Services/Sentinel/TestBehavioralAnalyzer.cpp" "Unit test file"
check_file "Services/Sentinel/BenchmarkBehavioralAnalyzer.cpp" "Benchmark file"

echo "3. Documentation"
echo "----------------"
check_file "Services/Sentinel/Test/behavioral/README.md" "Test dataset documentation"
check_file "Services/Sentinel/Test/behavioral/TEST_HARNESS_SUMMARY.md" "Test harness summary"
check_file "Services/Sentinel/PHASE2_TEST_HARNESS_DELIVERABLES.md" "Deliverables document"

echo "4. Benign Test Files"
echo "--------------------"
check_executable "Services/Sentinel/Test/behavioral/benign/hello.sh" "hello.sh (executable)"
check_executable "Services/Sentinel/Test/behavioral/benign/calculator.py" "calculator.py (executable)"

echo "5. Malicious Test Files"
echo "-----------------------"
check_file "Services/Sentinel/Test/behavioral/malicious/eicar.txt" "eicar.txt"
check_executable "Services/Sentinel/Test/behavioral/malicious/ransomware-sim.sh" "ransomware-sim.sh (executable)"

echo "6. CMake Integration"
echo "--------------------"
if grep -q "TestBehavioralAnalyzer" Services/Sentinel/CMakeLists.txt; then
    echo -e "${GREEN}✓${NC} TestBehavioralAnalyzer in CMakeLists.txt"
else
    echo -e "${RED}✗${NC} TestBehavioralAnalyzer not found in CMakeLists.txt"
    ERRORS=$((ERRORS + 1))
fi

if grep -q "BenchmarkBehavioralAnalyzer" Services/Sentinel/CMakeLists.txt; then
    echo -e "${GREEN}✓${NC} BenchmarkBehavioralAnalyzer in CMakeLists.txt"
else
    echo -e "${RED}✗${NC} BenchmarkBehavioralAnalyzer not found in CMakeLists.txt"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "7. CTest Registration"
echo "---------------------"
if [ -f "Build/release/Services/Sentinel/CTestTestfile.cmake" ]; then
    if grep -q "TestBehavioralAnalyzer" Build/release/Services/Sentinel/CTestTestfile.cmake; then
        echo -e "${GREEN}✓${NC} Test registered in CTest"
    else
        echo -e "${YELLOW}⚠${NC} Test not yet registered (run cmake --preset Release)"
    fi
else
    echo -e "${YELLOW}⚠${NC} CTest file not found (run cmake --preset Release)"
fi
echo ""

echo "8. Test File Contents"
echo "---------------------"
# Check for key test cases
TEST_FILE="Services/Sentinel/TestBehavioralAnalyzer.cpp"
if [ -f "$TEST_FILE" ]; then
    WEEK1_TESTS=$(grep -c "TEST_CASE.*temp_directory\|TEST_CASE.*nsjail" "$TEST_FILE" || true)
    WEEK2_TESTS=$(grep -c "TEST_CASE.*benign_file\|TEST_CASE.*malicious_pattern" "$TEST_FILE" || true)
    WEEK3_TESTS=$(grep -c "TEST_CASE.*syscall" "$TEST_FILE" || true)
    TOTAL_TESTS=$(grep -c "^TEST_CASE" "$TEST_FILE" || true)

    echo "  Week 1 tests (infrastructure): $WEEK1_TESTS"
    echo "  Week 2 tests (analysis): $WEEK2_TESTS"
    echo "  Week 3 tests (syscalls): $WEEK3_TESTS"
    echo "  Total test cases: $TOTAL_TESTS"

    if [ "$TOTAL_TESTS" -ge 25 ]; then
        echo -e "${GREEN}✓${NC} Sufficient test coverage ($TOTAL_TESTS tests)"
    else
        echo -e "${YELLOW}⚠${NC} Fewer tests than expected ($TOTAL_TESTS < 25)"
    fi
else
    echo -e "${RED}✗${NC} Test file not found"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "9. Benchmark Contents"
echo "---------------------"
BENCH_FILE="Services/Sentinel/BenchmarkBehavioralAnalyzer.cpp"
if [ -f "$BENCH_FILE" ]; then
    BENCHMARKS=$(grep -c "benchmark_file_analysis" "$BENCH_FILE" || true)
    echo "  Benchmark calls: $BENCHMARKS"

    if [ "$BENCHMARKS" -ge 3 ]; then
        echo -e "${GREEN}✓${NC} Sufficient benchmarks ($BENCHMARKS)"
    else
        echo -e "${YELLOW}⚠${NC} Fewer benchmarks than expected ($BENCHMARKS < 3)"
    fi
else
    echo -e "${RED}✗${NC} Benchmark file not found"
    ERRORS=$((ERRORS + 1))
fi
echo ""

echo "========================================"
echo "Verification Summary"
echo "========================================"
if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "Test harness is ready for implementation."
    echo ""
    echo "Next steps:"
    echo "  1. Implement BehavioralAnalyzer.h and BehavioralAnalyzer.cpp"
    echo "  2. Build tests: cmake --build Build/release --target TestBehavioralAnalyzer"
    echo "  3. Run tests: ctest -R TestBehavioralAnalyzer -V"
    echo "  4. Run benchmarks: ./Build/release/bin/BenchmarkBehavioralAnalyzer"
    exit 0
else
    echo -e "${RED}✗ $ERRORS error(s) found${NC}"
    echo ""
    echo "Please fix the errors above before proceeding."
    exit 1
fi
