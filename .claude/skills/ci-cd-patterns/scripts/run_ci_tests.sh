#!/usr/bin/env bash
# Run CI Test Suite for Ladybird
# Executes all test categories with proper error handling

set -euo pipefail

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*"
}

log_section() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}$*${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Configuration
BUILD_DIR="${BUILD_DIR:-Build/release}"
TEST_PRESET="${TEST_PRESET:-Release}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)}"
TEST_TIMEOUT="${TEST_TIMEOUT:-300}"
FAIL_FAST="${FAIL_FAST:-false}"

# Test categories
RUN_UNIT_TESTS="${RUN_UNIT_TESTS:-true}"
RUN_LIBWEB_TESTS="${RUN_LIBWEB_TESTS:-true}"
RUN_JS_TESTS="${RUN_JS_TESTS:-true}"
RUN_WPT_TESTS="${RUN_WPT_TESTS:-false}"  # Slow, disabled by default

# Track results
TESTS_PASSED=0
TESTS_FAILED=0
FAILED_TESTS=()

# Run a test suite
run_test_suite() {
    local suite_name="$1"
    local test_command="$2"

    log_section "Running $suite_name"

    local start_time=$(date +%s)

    if eval "$test_command"; then
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_info "✓ $suite_name PASSED (${duration}s)"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        local end_time=$(date +%s)
        local duration=$((end_time - start_time))
        log_error "✗ $suite_name FAILED (${duration}s)"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS+=("$suite_name")

        if [[ "$FAIL_FAST" == "true" ]]; then
            log_error "Fail-fast enabled. Stopping test execution."
            exit 1
        fi

        return 1
    fi
}

# Run unit tests via CTest
run_unit_tests() {
    if [[ "$RUN_UNIT_TESTS" != "true" ]]; then
        log_warn "Unit tests disabled"
        return 0
    fi

    log_section "Unit Tests"

    # Core tests (fast)
    run_test_suite "AK Tests" \
        "ctest --test-dir $BUILD_DIR -R '^TestAK' --output-on-failure --timeout $TEST_TIMEOUT"

    run_test_suite "LibCore Tests" \
        "ctest --test-dir $BUILD_DIR -R '^TestLibCore' --output-on-failure --timeout $TEST_TIMEOUT"

    # LibWeb tests
    run_test_suite "LibWeb Tests" \
        "ctest --test-dir $BUILD_DIR -R '^TestLibWeb' --output-on-failure --timeout $TEST_TIMEOUT"

    # LibGfx tests
    run_test_suite "LibGfx Tests" \
        "ctest --test-dir $BUILD_DIR -R '^TestLibGfx' --output-on-failure --timeout $TEST_TIMEOUT"

    # LibHTTP tests
    run_test_suite "LibHTTP Tests" \
        "ctest --test-dir $BUILD_DIR -R '^TestLibHTTP' --output-on-failure --timeout $TEST_TIMEOUT"

    # All other tests in parallel
    run_test_suite "All Unit Tests" \
        "ctest --preset $TEST_PRESET --parallel $PARALLEL_JOBS --output-on-failure --timeout $TEST_TIMEOUT"
}

# Run LibWeb browser tests
run_libweb_tests() {
    if [[ "$RUN_LIBWEB_TESTS" != "true" ]]; then
        log_warn "LibWeb tests disabled"
        return 0
    fi

    log_section "LibWeb Browser Tests"

    # Text tests (JavaScript API tests)
    run_test_suite "LibWeb Text Tests" \
        "./Meta/ladybird.py run test-web -- -f Text/input/ --timeout $TEST_TIMEOUT"

    # Layout tests
    run_test_suite "LibWeb Layout Tests" \
        "./Meta/ladybird.py run test-web -- -f Layout/ --timeout $TEST_TIMEOUT"

    # Ref tests (screenshot comparison against reference HTML)
    if command -v convert &> /dev/null; then  # Requires ImageMagick
        run_test_suite "LibWeb Ref Tests" \
            "./Meta/ladybird.py run test-web -- -f Ref/ --timeout $TEST_TIMEOUT"
    else
        log_warn "ImageMagick not found. Skipping Ref tests."
    fi
}

# Run JavaScript tests
run_js_tests() {
    if [[ "$RUN_JS_TESTS" != "true" ]]; then
        log_warn "JavaScript tests disabled"
        return 0
    fi

    log_section "JavaScript Tests"

    # LibJS unit tests
    run_test_suite "LibJS Tests" \
        "ctest --test-dir $BUILD_DIR -R '^TestLibJS' --output-on-failure --timeout $TEST_TIMEOUT"

    # Test262 conformance tests (if available)
    if [ -f "$BUILD_DIR/bin/test262-runner" ]; then
        run_test_suite "Test262 Conformance" \
            "$BUILD_DIR/bin/test262-runner --timeout $TEST_TIMEOUT"
    else
        log_warn "test262-runner not found. Skipping Test262 tests."
    fi
}

# Run Web Platform Tests
run_wpt_tests() {
    if [[ "$RUN_WPT_TESTS" != "true" ]]; then
        log_warn "WPT tests disabled (slow)"
        return 0
    fi

    log_section "Web Platform Tests"

    if [ ! -f "./Meta/WPT.sh" ]; then
        log_warn "WPT.sh not found. Skipping WPT tests."
        return 0
    fi

    # Run WPT tests
    local wpt_log="wpt-results-$(date +%Y%m%d-%H%M%S).log"

    run_test_suite "Web Platform Tests" \
        "./Meta/WPT.sh run --log $wpt_log --timeout $TEST_TIMEOUT"

    # Compare against expectations (if baseline exists)
    if [ -f "Tests/LibWeb/WPT/expectations.log" ]; then
        log_info "Comparing WPT results against baseline..."
        ./Meta/WPT.sh compare \
            --log "$wpt_log" \
            Tests/LibWeb/WPT/expectations.log \
            || log_warn "WPT results differ from baseline"
    fi
}

# Generate test report
generate_report() {
    log_section "Test Summary"

    local total_tests=$((TESTS_PASSED + TESTS_FAILED))
    local pass_rate=0

    if [ $total_tests -gt 0 ]; then
        pass_rate=$(awk "BEGIN {printf \"%.1f\", ($TESTS_PASSED / $total_tests) * 100}")
    fi

    echo ""
    echo "Total test suites:  $total_tests"
    echo "Passed:             $TESTS_PASSED"
    echo "Failed:             $TESTS_FAILED"
    echo "Pass rate:          ${pass_rate}%"
    echo ""

    if [ $TESTS_FAILED -gt 0 ]; then
        log_error "Failed test suites:"
        for test in "${FAILED_TESTS[@]}"; do
            echo "  - $test"
        done
        echo ""
        return 1
    else
        log_info "✓ All test suites passed!"
        return 0
    fi
}

# Export test results in JUnit format
export_junit() {
    local output_file="${JUNIT_OUTPUT:-test-results.xml}"

    log_info "Exporting JUnit test results to $output_file..."

    # CTest can generate JUnit output
    if [ -f "$BUILD_DIR/Testing/TAG" ]; then
        local test_dir=$(head -1 "$BUILD_DIR/Testing/TAG")
        if [ -f "$BUILD_DIR/Testing/$test_dir/Test.xml" ]; then
            cp "$BUILD_DIR/Testing/$test_dir/Test.xml" "$output_file"
            log_info "JUnit results exported"
        fi
    fi
}

# Main
main() {
    log_info "Starting Ladybird CI test suite..."
    log_info "Configuration:"
    log_info "  - Build directory: $BUILD_DIR"
    log_info "  - Test preset: $TEST_PRESET"
    log_info "  - Parallel jobs: $PARALLEL_JOBS"
    log_info "  - Test timeout: ${TEST_TIMEOUT}s"
    log_info "  - Fail fast: $FAIL_FAST"
    echo ""

    # Verify build exists
    if [ ! -d "$BUILD_DIR" ]; then
        log_error "Build directory not found: $BUILD_DIR"
        exit 1
    fi

    # Run test suites
    local start_time=$(date +%s)

    run_unit_tests
    run_libweb_tests
    run_js_tests
    run_wpt_tests

    local end_time=$(date +%s)
    local total_duration=$((end_time - start_time))

    # Generate report
    echo ""
    generate_report

    log_info "Total test time: ${total_duration}s"

    # Export results
    export_junit

    # Exit with appropriate code
    if [ $TESTS_FAILED -gt 0 ]; then
        exit 1
    else
        exit 0
    fi
}

# Handle arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --preset)
            TEST_PRESET="$2"
            shift 2
            ;;
        --timeout)
            TEST_TIMEOUT="$2"
            shift 2
            ;;
        --parallel)
            PARALLEL_JOBS="$2"
            shift 2
            ;;
        --fail-fast)
            FAIL_FAST="true"
            shift
            ;;
        --no-wpt)
            RUN_WPT_TESTS="false"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --build-dir DIR    Build directory (default: Build/release)"
            echo "  --preset PRESET    Test preset (default: Release)"
            echo "  --timeout SEC      Test timeout in seconds (default: 300)"
            echo "  --parallel N       Parallel test jobs (default: nproc)"
            echo "  --fail-fast        Stop on first failure"
            echo "  --no-wpt           Skip Web Platform Tests"
            echo "  --help             Show this help"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

main "$@"
