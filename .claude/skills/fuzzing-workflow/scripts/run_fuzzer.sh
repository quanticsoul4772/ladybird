#!/usr/bin/env bash
# Launch fuzzing campaigns with AFL++ or LibFuzzer for Ladybird components

set -euo pipefail

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
FUZZER_TYPE="libfuzzer"
BUILD_DIR="$REPO_ROOT/Build/fuzzers"
TARGET=""
CORPUS_DIR=""
TIMEOUT=10
JOBS=1
WORKERS=1
MAX_LEN=65536
RSS_LIMIT=2048
DICT_FILE=""
EXTRA_ARGS=()
RUN_TIME=""

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] TARGET

Launch fuzzing campaigns for Ladybird browser components

OPTIONS:
    -t, --type TYPE         Fuzzer type: libfuzzer, afl (default: libfuzzer)
    -b, --build-dir DIR     Build directory (default: Build/fuzzers)
    -c, --corpus DIR        Corpus directory (default: Fuzzing/Corpus/TARGET/)
    -d, --dict FILE         Dictionary file for fuzzing
    -j, --jobs N            Number of parallel jobs (default: 1)
    -w, --workers N         Number of worker threads (default: 1)
    -m, --max-len N         Maximum input length (default: 65536)
    -r, --rss-limit MB      RSS memory limit in MB (default: 2048)
    -T, --timeout SECS      Timeout per input in seconds (default: 10)
    -R, --run-time SECS     Total run time in seconds (optional)
    -h, --help              Show this help

TARGET:
    Name of fuzz target binary (e.g., FuzzIPCMessages, FuzzHTMLParser)

EXAMPLES:
    # Run IPC message fuzzer with LibFuzzer
    $0 FuzzIPCMessages

    # Run HTML parser fuzzer with 4 jobs
    $0 -j 4 -w 4 FuzzHTMLParser

    # Run with custom corpus and dictionary
    $0 -c my-corpus/ -d ipc.dict FuzzIPCMessages

    # Run with AFL++
    $0 -t afl FuzzIPCMessages

    # Run for 1 hour
    $0 -R 3600 -j 8 FuzzIPCMessages

EOF
    exit 1
}

log_info() {
    echo -e "${BLUE}==>${NC} $1"
}

log_success() {
    echo -e "${GREEN}✓${NC} $1"
}

log_error() {
    echo -e "${RED}✗${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            FUZZER_TYPE="$2"
            shift 2
            ;;
        -b|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -c|--corpus)
            CORPUS_DIR="$2"
            shift 2
            ;;
        -d|--dict)
            DICT_FILE="$2"
            shift 2
            ;;
        -j|--jobs)
            JOBS="$2"
            shift 2
            ;;
        -w|--workers)
            WORKERS="$2"
            shift 2
            ;;
        -m|--max-len)
            MAX_LEN="$2"
            shift 2
            ;;
        -r|--rss-limit)
            RSS_LIMIT="$2"
            shift 2
            ;;
        -T|--timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        -R|--run-time)
            RUN_TIME="$2"
            shift 2
            ;;
        -h|--help)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            usage
            ;;
        *)
            TARGET="$1"
            shift
            ;;
    esac
done

# Validate target
if [[ -z "$TARGET" ]]; then
    log_error "No target specified"
    usage
fi

# Set default corpus directory
if [[ -z "$CORPUS_DIR" ]]; then
    CORPUS_DIR="$REPO_ROOT/Fuzzing/Corpus/${TARGET}"
fi

# Normalize fuzzer type
FUZZER_TYPE=$(echo "$FUZZER_TYPE" | tr '[:upper:]' '[:lower:]')

echo "============================================"
echo "Ladybird Fuzzing Campaign Launcher"
echo "============================================"
echo ""
log_info "Configuration:"
echo "  Fuzzer:     $FUZZER_TYPE"
echo "  Target:     $TARGET"
echo "  Build dir:  $BUILD_DIR"
echo "  Corpus:     $CORPUS_DIR"
echo "  Jobs:       $JOBS"
echo "  Workers:    $WORKERS"
echo "  Timeout:    ${TIMEOUT}s"
echo "  Max len:    $MAX_LEN bytes"
echo "  RSS limit:  ${RSS_LIMIT}MB"
[[ -n "$DICT_FILE" ]] && echo "  Dictionary: $DICT_FILE"
[[ -n "$RUN_TIME" ]] && echo "  Run time:   ${RUN_TIME}s"
echo ""

# Check build directory
if [[ ! -d "$BUILD_DIR" ]]; then
    log_error "Build directory not found: $BUILD_DIR"
    echo ""
    echo "Build fuzzers with:"
    echo "  cmake -B Build/fuzzers -DENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo"
    echo "  cmake --build Build/fuzzers"
    exit 1
fi

# Find target binary
TARGET_PATH="$BUILD_DIR/bin/$TARGET"
if [[ ! -f "$TARGET_PATH" ]]; then
    # Try without bin/ subdirectory
    TARGET_PATH="$BUILD_DIR/$TARGET"
    if [[ ! -f "$TARGET_PATH" ]]; then
        log_error "Target binary not found: $TARGET"
        echo ""
        echo "Available targets in $BUILD_DIR:"
        find "$BUILD_DIR" -maxdepth 2 -type f -executable -name "Fuzz*" 2>/dev/null || echo "  (none found)"
        exit 1
    fi
fi

log_success "Found target: $TARGET_PATH"

# Create corpus directories
mkdir -p "$CORPUS_DIR"

# Check if corpus is empty
CORPUS_COUNT=$(find "$CORPUS_DIR" -type f 2>/dev/null | wc -l)
if [[ $CORPUS_COUNT -eq 0 ]]; then
    log_warn "Corpus directory is empty: $CORPUS_DIR"
    log_info "Creating seed corpus..."

    # Create basic seed inputs
    mkdir -p "$CORPUS_DIR"
    echo -n "" > "$CORPUS_DIR/empty"
    echo -n "A" > "$CORPUS_DIR/single_byte"
    python3 -c "import sys; sys.stdout.buffer.write(b'\\x00')" > "$CORPUS_DIR/null_byte"
    python3 -c "import sys; sys.stdout.buffer.write(b'A' * 1024)" > "$CORPUS_DIR/1kb"

    log_success "Created basic seed corpus (4 files)"
fi

# Validate dictionary file
if [[ -n "$DICT_FILE" ]]; then
    if [[ ! -f "$DICT_FILE" ]]; then
        # Try relative to Fuzzing directory
        ALT_DICT="$REPO_ROOT/Fuzzing/$DICT_FILE"
        if [[ -f "$ALT_DICT" ]]; then
            DICT_FILE="$ALT_DICT"
            log_success "Found dictionary: $DICT_FILE"
        else
            log_error "Dictionary file not found: $DICT_FILE"
            exit 1
        fi
    fi
fi

# Launch fuzzer based on type
case "$FUZZER_TYPE" in
    libfuzzer|lf)
        log_info "Launching LibFuzzer campaign..."
        echo ""

        # Build LibFuzzer arguments
        FUZZER_ARGS=(
            "-max_len=$MAX_LEN"
            "-timeout=$TIMEOUT"
            "-rss_limit_mb=$RSS_LIMIT"
        )

        [[ -n "$DICT_FILE" ]] && FUZZER_ARGS+=("-dict=$DICT_FILE")
        [[ -n "$RUN_TIME" ]] && FUZZER_ARGS+=("-max_total_time=$RUN_TIME")

        # Add parallelization
        if [[ $JOBS -gt 1 ]]; then
            FUZZER_ARGS+=("-jobs=$JOBS")
            FUZZER_ARGS+=("-workers=$WORKERS")
        fi

        # Create crash directory
        CRASH_DIR="$CORPUS_DIR/../${TARGET}-crashes"
        mkdir -p "$CRASH_DIR"

        # Run LibFuzzer
        log_info "Command: $TARGET_PATH ${FUZZER_ARGS[*]} $CORPUS_DIR"
        echo ""

        "$TARGET_PATH" "${FUZZER_ARGS[@]}" "$CORPUS_DIR" 2>&1 | tee "fuzzing_${TARGET}_$(date +%Y%m%d_%H%M%S).log"

        EXIT_CODE=${PIPESTATUS[0]}

        echo ""
        if [[ $EXIT_CODE -eq 0 ]]; then
            log_success "Fuzzing completed successfully"
        else
            log_warn "Fuzzing exited with code $EXIT_CODE"
        fi

        # Check for crashes
        CRASH_COUNT=$(find . -maxdepth 1 -name "crash-*" -o -name "timeout-*" | wc -l)
        if [[ $CRASH_COUNT -gt 0 ]]; then
            log_warn "Found $CRASH_COUNT crashes/timeouts"
            echo ""
            echo "Crashes found:"
            find . -maxdepth 1 -name "crash-*" -o -name "timeout-*" | head -10

            # Move crashes to crash directory
            mv crash-* timeout-* "$CRASH_DIR/" 2>/dev/null || true
            log_info "Moved crashes to: $CRASH_DIR/"
        fi

        # Show corpus stats
        CORPUS_COUNT=$(find "$CORPUS_DIR" -type f | wc -l)
        log_info "Final corpus size: $CORPUS_COUNT files"
        ;;

    afl|afl++)
        log_info "Launching AFL++ campaign..."
        echo ""

        # Check for AFL++
        if ! command -v afl-fuzz &> /dev/null; then
            log_error "afl-fuzz not found"
            echo "Install AFL++: sudo apt install afl++"
            exit 1
        fi

        # Check if binary is instrumented
        if ! ldd "$TARGET_PATH" 2>/dev/null | grep -q "SanitizerCoverage"; then
            log_warn "Binary may not be instrumented for AFL++"
            log_warn "Build with: AFL_USE_ASAN=1 cmake -DCMAKE_CXX_COMPILER=afl-clang-fast++"
        fi

        # Create AFL directories
        INPUT_DIR="$CORPUS_DIR"
        OUTPUT_DIR="$CORPUS_DIR/../${TARGET}-afl-findings"
        mkdir -p "$INPUT_DIR" "$OUTPUT_DIR"

        # Build AFL arguments
        AFL_ARGS=(
            "-i" "$INPUT_DIR"
            "-o" "$OUTPUT_DIR"
            "-m" "none"
            "-t" "$((TIMEOUT * 1000))"
        )

        [[ -n "$DICT_FILE" ]] && AFL_ARGS+=("-x" "$DICT_FILE")

        # Add timeout
        if [[ -n "$RUN_TIME" ]]; then
            AFL_ARGS=("timeout" "$RUN_TIME" "afl-fuzz" "${AFL_ARGS[@]}")
        else
            AFL_ARGS=("afl-fuzz" "${AFL_ARGS[@]}")
        fi

        # Run AFL++
        log_info "Command: ${AFL_ARGS[*]} -- $TARGET_PATH @@"
        echo ""

        "${AFL_ARGS[@]}" -- "$TARGET_PATH" @@

        EXIT_CODE=$?

        echo ""
        if [[ $EXIT_CODE -eq 0 ]] || [[ $EXIT_CODE -eq 124 ]]; then
            log_success "Fuzzing completed"
        else
            log_warn "Fuzzing exited with code $EXIT_CODE"
        fi

        # Check for crashes
        if [[ -d "$OUTPUT_DIR/default/crashes" ]]; then
            CRASH_COUNT=$(find "$OUTPUT_DIR/default/crashes" -type f ! -name "README.txt" | wc -l)
            if [[ $CRASH_COUNT -gt 0 ]]; then
                log_warn "Found $CRASH_COUNT unique crashes"
                echo ""
                echo "Crashes in: $OUTPUT_DIR/default/crashes/"
            else
                log_success "No crashes found"
            fi
        fi

        # Show queue stats
        if [[ -d "$OUTPUT_DIR/default/queue" ]]; then
            QUEUE_COUNT=$(find "$OUTPUT_DIR/default/queue" -type f ! -name "README.txt" | wc -l)
            log_info "Queue size: $QUEUE_COUNT test cases"
        fi
        ;;

    *)
        log_error "Unknown fuzzer type: $FUZZER_TYPE"
        echo "Supported types: libfuzzer, afl"
        exit 1
        ;;
esac

echo ""
echo "============================================"
echo "Fuzzing Campaign Complete"
echo "============================================"
