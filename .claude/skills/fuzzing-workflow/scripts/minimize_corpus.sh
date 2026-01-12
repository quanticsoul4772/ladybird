#!/usr/bin/env bash
# Optimize fuzzing corpus by removing redundant test cases

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
CORPUS_DIR=""
TARGET_BINARY=""
OUTPUT_DIR=""
METHOD="libfuzzer"
KEEP_ORIGINAL=1

# Usage
usage() {
    cat <<EOF
Usage: $0 [OPTIONS] CORPUS_DIR TARGET_BINARY

Minimize fuzzing corpus by removing redundant test cases

OPTIONS:
    -o, --output DIR        Output directory for minimized corpus
                            (default: CORPUS_DIR-minimized/)
    -m, --method METHOD     Minimization method: libfuzzer, afl
                            (default: libfuzzer)
    -d, --delete-original   Delete original corpus after minimization
    -h, --help              Show this help

ARGUMENTS:
    CORPUS_DIR              Directory containing corpus files
    TARGET_BINARY           Path to fuzz target binary

DESCRIPTION:
    This tool removes redundant test cases from a fuzzing corpus while
    preserving coverage. It keeps only the inputs that contribute unique
    code coverage, significantly reducing corpus size and improving
    fuzzing efficiency.

EXAMPLES:
    # Minimize corpus with LibFuzzer
    $0 Fuzzing/Corpus/FuzzIPCMessages/ Build/fuzzers/bin/FuzzIPCMessages

    # Minimize and replace original
    $0 -d Fuzzing/Corpus/FuzzIPCMessages/ Build/fuzzers/bin/FuzzIPCMessages

    # Use AFL++ minimization
    $0 -m afl Fuzzing/Corpus/FuzzIPCMessages-afl-findings/default/queue/ Build/fuzzers/bin/FuzzIPCMessages

    # Custom output directory
    $0 -o my-minimized-corpus/ Fuzzing/Corpus/FuzzIPCMessages/ Build/fuzzers/bin/FuzzIPCMessages

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
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        -m|--method)
            METHOD="$2"
            shift 2
            ;;
        -d|--delete-original)
            KEEP_ORIGINAL=0
            shift
            ;;
        -h|--help)
            usage
            ;;
        -*)
            log_error "Unknown option: $1"
            usage
            ;;
        *)
            if [[ -z "$CORPUS_DIR" ]]; then
                CORPUS_DIR="$1"
            elif [[ -z "$TARGET_BINARY" ]]; then
                TARGET_BINARY="$1"
            else
                log_error "Too many arguments"
                usage
            fi
            shift
            ;;
    esac
done

# Validate arguments
if [[ -z "$CORPUS_DIR" ]] || [[ -z "$TARGET_BINARY" ]]; then
    log_error "Missing required arguments"
    usage
fi

if [[ ! -d "$CORPUS_DIR" ]]; then
    log_error "Corpus directory not found: $CORPUS_DIR"
    exit 1
fi

if [[ ! -f "$TARGET_BINARY" ]]; then
    log_error "Target binary not found: $TARGET_BINARY"
    exit 1
fi

# Set default output directory
if [[ -z "$OUTPUT_DIR" ]]; then
    OUTPUT_DIR="${CORPUS_DIR}-minimized"
fi

# Normalize method
METHOD=$(echo "$METHOD" | tr '[:upper:]' '[:lower:]')

echo "============================================"
echo "Corpus Minimization"
echo "============================================"
echo ""
log_info "Configuration:"
echo "  Method:     $METHOD"
echo "  Input:      $CORPUS_DIR"
echo "  Output:     $OUTPUT_DIR"
echo "  Target:     $TARGET_BINARY"
echo ""

# Count original corpus
log_info "Analyzing original corpus..."
ORIGINAL_COUNT=$(find "$CORPUS_DIR" -type f ! -name "README.txt" ! -name "*.log" | wc -l)
ORIGINAL_SIZE=$(du -sh "$CORPUS_DIR" | cut -f1)

if [[ $ORIGINAL_COUNT -eq 0 ]]; then
    log_error "No files found in corpus directory"
    exit 1
fi

log_success "Original corpus: $ORIGINAL_COUNT files, $ORIGINAL_SIZE"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Perform minimization based on method
case "$METHOD" in
    libfuzzer|lf)
        log_info "Minimizing corpus with LibFuzzer merge..."

        # LibFuzzer uses -merge=1 to deduplicate based on coverage
        log_info "Running: $TARGET_BINARY -merge=1 $OUTPUT_DIR $CORPUS_DIR"
        echo ""

        # Run merge
        "$TARGET_BINARY" -merge=1 "$OUTPUT_DIR" "$CORPUS_DIR" 2>&1 | tee "minimize_$(date +%Y%m%d_%H%M%S).log"

        EXIT_CODE=${PIPESTATUS[0]}

        if [[ $EXIT_CODE -ne 0 ]]; then
            log_warn "Merge completed with exit code $EXIT_CODE"
        fi
        ;;

    afl|afl++)
        log_info "Minimizing corpus with AFL++ cmin..."

        # Check for afl-cmin
        if ! command -v afl-cmin &> /dev/null; then
            log_error "afl-cmin not found"
            echo "Install AFL++: sudo apt install afl++"
            exit 1
        fi

        # AFL cmin requires a temporary directory
        TEMP_INPUT="$(mktemp -d)"
        cp -r "$CORPUS_DIR"/* "$TEMP_INPUT/" 2>/dev/null || true

        log_info "Running: afl-cmin -i $TEMP_INPUT -o $OUTPUT_DIR -- $TARGET_BINARY @@"
        echo ""

        # Run cmin
        afl-cmin \
            -i "$TEMP_INPUT" \
            -o "$OUTPUT_DIR" \
            -m none \
            -t 1000 \
            -- "$TARGET_BINARY" @@ 2>&1 | tee "minimize_$(date +%Y%m%d_%H%M%S).log"

        EXIT_CODE=${PIPESTATUS[0]}

        # Clean up temp directory
        rm -rf "$TEMP_INPUT"

        if [[ $EXIT_CODE -ne 0 ]]; then
            log_warn "Cmin completed with exit code $EXIT_CODE"
        fi
        ;;

    *)
        log_error "Unknown minimization method: $METHOD"
        echo "Supported methods: libfuzzer, afl"
        exit 1
        ;;
esac

echo ""
log_info "Analyzing minimized corpus..."

# Count minimized corpus
MINIMIZED_COUNT=$(find "$OUTPUT_DIR" -type f ! -name "README.txt" ! -name "*.log" | wc -l)
MINIMIZED_SIZE=$(du -sh "$OUTPUT_DIR" | cut -f1)

if [[ $MINIMIZED_COUNT -eq 0 ]]; then
    log_error "Minimization produced no files"
    exit 1
fi

log_success "Minimized corpus: $MINIMIZED_COUNT files, $MINIMIZED_SIZE"
echo ""

# Calculate reduction
REDUCTION_PERCENT=$((100 - (MINIMIZED_COUNT * 100 / ORIGINAL_COUNT)))
SIZE_REDUCTION=$(echo "scale=1; 100 - ($(du -sb "$OUTPUT_DIR" | cut -f1) * 100.0 / $(du -sb "$CORPUS_DIR" | cut -f1))" | bc 2>/dev/null || echo "N/A")

echo "============================================"
echo "Minimization Results"
echo "============================================"
echo ""
echo "  Original:  $ORIGINAL_COUNT files, $ORIGINAL_SIZE"
echo "  Minimized: $MINIMIZED_COUNT files, $MINIMIZED_SIZE"
echo "  Reduction: $REDUCTION_PERCENT% fewer files, ${SIZE_REDUCTION}% smaller"
echo ""

# Show file size distribution
log_info "File size distribution in minimized corpus:"
find "$OUTPUT_DIR" -type f ! -name "README.txt" -exec wc -c {} \; | \
    awk '{
        if ($1 < 100) small++;
        else if ($1 < 1024) medium++;
        else if ($1 < 10240) large++;
        else huge++;
        total++;
    }
    END {
        printf "  < 100 bytes:    %d (%.1f%%)\n", small, small*100.0/total;
        printf "  100-1KB:        %d (%.1f%%)\n", medium, medium*100.0/total;
        printf "  1-10KB:         %d (%.1f%%)\n", large, large*100.0/total;
        printf "  > 10KB:         %d (%.1f%%)\n", huge, huge*100.0/total;
    }'

echo ""

# Replace original if requested
if [[ $KEEP_ORIGINAL -eq 0 ]]; then
    log_warn "Replacing original corpus..."

    # Backup original
    BACKUP_DIR="${CORPUS_DIR}.backup.$(date +%Y%m%d_%H%M%S)"
    mv "$CORPUS_DIR" "$BACKUP_DIR"
    mv "$OUTPUT_DIR" "$CORPUS_DIR"

    log_success "Original corpus backed up to: $BACKUP_DIR"
    log_success "Minimized corpus moved to: $CORPUS_DIR"
else
    log_success "Minimized corpus saved to: $OUTPUT_DIR"
    echo ""
    log_info "To use minimized corpus for fuzzing:"
    echo "  $TARGET_BINARY $OUTPUT_DIR"
fi

echo ""
echo "============================================"
echo "Corpus Minimization Complete"
echo "============================================"
echo ""
log_info "Recommendations:"
echo "  1. Run fuzzing campaign with minimized corpus"
echo "  2. Periodically re-minimize as corpus grows"
echo "  3. Merge findings from multiple campaigns"
echo ""
echo "Merge multiple corpora:"
echo "  $TARGET_BINARY -merge=1 merged/ corpus1/ corpus2/ corpus3/"
