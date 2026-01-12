#!/usr/bin/env bash
# Generate API documentation using Doxygen

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Configuration
DOXYFILE="${PROJECT_ROOT}/Doxyfile"
OUTPUT_DIR="${PROJECT_ROOT}/docs/api"
DOXYGEN_VERSION_MIN="1.9.0"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Generate API documentation using Doxygen.

OPTIONS:
    -h, --help          Show this help message
    -c, --check         Check if docs need regeneration (exit 1 if stale)
    -o, --output DIR    Output directory (default: docs/api)
    -f, --format FMT    Output format: html, latex, xml, all (default: html)
    --clean             Clean output directory before generating

EXAMPLES:
    $(basename "$0")                  # Generate HTML docs
    $(basename "$0") --format all     # Generate all formats
    $(basename "$0") --check          # Check if docs are up-to-date

EOF
}

log_info() {
    echo -e "${GREEN}[INFO]${NC} $*"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $*" >&2
}

check_doxygen() {
    if ! command -v doxygen &> /dev/null; then
        log_error "Doxygen is not installed"
        log_info "Install with:"
        log_info "  Ubuntu/Debian: sudo apt install doxygen graphviz"
        log_info "  macOS: brew install doxygen graphviz"
        log_info "  Arch: sudo pacman -S doxygen graphviz"
        return 1
    fi

    local version
    version="$(doxygen --version)"
    log_info "Found Doxygen version $version"

    # Compare versions (simplified check)
    if [[ "$version" < "$DOXYGEN_VERSION_MIN" ]]; then
        log_warn "Doxygen version $version is older than recommended $DOXYGEN_VERSION_MIN"
        log_warn "Some features may not work correctly"
    fi

    # Check for graphviz (for diagrams)
    if ! command -v dot &> /dev/null; then
        log_warn "Graphviz (dot) is not installed - diagrams will not be generated"
        log_info "Install with: sudo apt install graphviz (or brew install graphviz)"
    fi
}

create_doxyfile() {
    log_info "Creating default Doxyfile..."

    cat > "$DOXYFILE" <<'EOF'
# Doxyfile for Ladybird API Documentation

# Project information
PROJECT_NAME           = "Ladybird Browser"
PROJECT_BRIEF          = "Independent web browser"
PROJECT_VERSION        = "1.0.0"
PROJECT_LOGO           =

# Input configuration
INPUT                  = Libraries/ Services/
FILE_PATTERNS          = *.h *.cpp
RECURSIVE              = YES
EXCLUDE_PATTERNS       = */Tests/* */Build/* */vcpkg/*
EXCLUDE_SYMBOLS        = *::Detail *::Internal

# Output configuration
OUTPUT_DIRECTORY       = docs/api
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
GENERATE_XML           = NO

# HTML output options
HTML_OUTPUT            = html
HTML_FILE_EXTENSION    = .html
HTML_COLORSTYLE_HUE    = 220
HTML_COLORSTYLE_SAT    = 100
HTML_COLORSTYLE_GAMMA  = 80
HTML_TIMESTAMP         = YES
HTML_DYNAMIC_SECTIONS  = YES

# Code extraction
EXTRACT_ALL            = NO
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES
EXTRACT_LOCAL_CLASSES  = NO

# Warnings
QUIET                  = NO
WARNINGS               = YES
WARN_IF_UNDOCUMENTED   = YES
WARN_IF_DOC_ERROR      = YES
WARN_NO_PARAMDOC       = YES

# Source browsing
SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO
REFERENCED_BY_RELATION = YES
REFERENCES_RELATION    = YES

# Diagrams
HAVE_DOT               = YES
DOT_NUM_THREADS        = 4
CALL_GRAPH             = NO
CALLER_GRAPH           = NO
CLASS_DIAGRAMS         = YES
CLASS_GRAPH            = YES
COLLABORATION_GRAPH    = YES
INCLUDE_GRAPH          = YES
INCLUDED_BY_GRAPH      = YES

# Preprocessing
ENABLE_PREPROCESSING   = YES
MACRO_EXPANSION        = YES
EXPAND_ONLY_PREDEF     = NO
PREDEFINED             = __attribute__(x)=

# Additional features
GENERATE_TREEVIEW      = YES
DISABLE_INDEX          = NO
GENERATE_TODOLIST      = YES
SEARCHENGINE           = YES
EOF

    log_info "Created $DOXYFILE"
}

check_if_stale() {
    if [[ ! -d "$OUTPUT_DIR" ]]; then
        log_info "Output directory does not exist - docs need generation"
        return 1
    fi

    # Find newest source file
    local newest_source
    newest_source="$(find "$PROJECT_ROOT/Libraries" "$PROJECT_ROOT/Services" \
        -name "*.h" -o -name "*.cpp" 2>/dev/null | \
        xargs stat -c '%Y %n' 2>/dev/null | \
        sort -rn | head -1 | cut -d' ' -f2-)"

    if [[ -z "$newest_source" ]]; then
        log_warn "No source files found"
        return 0
    fi

    # Find oldest generated file
    local oldest_generated
    oldest_generated="$(find "$OUTPUT_DIR" -type f 2>/dev/null | \
        xargs stat -c '%Y %n' 2>/dev/null | \
        sort -n | head -1 | cut -d' ' -f2-)"

    if [[ -z "$oldest_generated" ]]; then
        log_info "No generated files found - docs need generation"
        return 1
    fi

    # Compare timestamps
    if [[ "$newest_source" -nt "$oldest_generated" ]]; then
        log_info "Source files are newer than generated docs"
        return 1
    fi

    log_info "Documentation is up-to-date"
    return 0
}

generate_docs() {
    local format="$1"

    cd "$PROJECT_ROOT"

    # Update Doxyfile for requested format
    case "$format" in
        html)
            sed -i 's/GENERATE_HTML.*/GENERATE_HTML = YES/' "$DOXYFILE"
            sed -i 's/GENERATE_LATEX.*/GENERATE_LATEX = NO/' "$DOXYFILE"
            sed -i 's/GENERATE_XML.*/GENERATE_XML = NO/' "$DOXYFILE"
            ;;
        latex)
            sed -i 's/GENERATE_HTML.*/GENERATE_HTML = NO/' "$DOXYFILE"
            sed -i 's/GENERATE_LATEX.*/GENERATE_LATEX = YES/' "$DOXYFILE"
            sed -i 's/GENERATE_XML.*/GENERATE_XML = NO/' "$DOXYFILE"
            ;;
        xml)
            sed -i 's/GENERATE_HTML.*/GENERATE_HTML = NO/' "$DOXYFILE"
            sed -i 's/GENERATE_LATEX.*/GENERATE_LATEX = NO/' "$DOXYFILE"
            sed -i 's/GENERATE_XML.*/GENERATE_XML = YES/' "$DOXYFILE"
            ;;
        all)
            sed -i 's/GENERATE_HTML.*/GENERATE_HTML = YES/' "$DOXYFILE"
            sed -i 's/GENERATE_LATEX.*/GENERATE_LATEX = YES/' "$DOXYFILE"
            sed -i 's/GENERATE_XML.*/GENERATE_XML = YES/' "$DOXYFILE"
            ;;
    esac

    log_info "Generating documentation ($format)..."
    if doxygen "$DOXYFILE"; then
        log_info "Documentation generated successfully in $OUTPUT_DIR"

        if [[ "$format" == "html" ]] || [[ "$format" == "all" ]]; then
            local index_file="$OUTPUT_DIR/html/index.html"
            if [[ -f "$index_file" ]]; then
                log_info "Open documentation: file://$index_file"
            fi
        fi
    else
        log_error "Doxygen generation failed"
        return 1
    fi
}

main() {
    local check_only=false
    local clean=false
    local format="html"

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--check)
                check_only=true
                shift
                ;;
            -o|--output)
                OUTPUT_DIR="$2"
                shift 2
                ;;
            -f|--format)
                format="$2"
                shift 2
                ;;
            --clean)
                clean=true
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Validate format
    if [[ ! "$format" =~ ^(html|latex|xml|all)$ ]]; then
        log_error "Invalid format: $format"
        log_info "Valid formats: html, latex, xml, all"
        exit 1
    fi

    # Check for Doxygen
    check_doxygen || exit 1

    # Create Doxyfile if it doesn't exist
    if [[ ! -f "$DOXYFILE" ]]; then
        log_warn "Doxyfile not found at $DOXYFILE"
        create_doxyfile
    fi

    # Check-only mode
    if [[ "$check_only" == true ]]; then
        if check_if_stale; then
            log_info "Documentation is up-to-date"
            exit 0
        else
            log_warn "Documentation is out-of-date"
            exit 1
        fi
    fi

    # Clean if requested
    if [[ "$clean" == true ]]; then
        log_info "Cleaning $OUTPUT_DIR..."
        rm -rf "$OUTPUT_DIR"
    fi

    # Generate documentation
    generate_docs "$format"
}

main "$@"
