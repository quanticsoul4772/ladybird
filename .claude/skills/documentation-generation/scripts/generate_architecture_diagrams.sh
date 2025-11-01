#!/usr/bin/env bash
# Generate architecture diagrams from Mermaid source files

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Configuration
DOCS_DIR="${PROJECT_ROOT}/Documentation"
IMAGES_DIR="${DOCS_DIR}/Images"
MERMAID_CLI="mmdc"  # mermaid-cli command

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] [FILES...]

Generate PNG/SVG diagrams from Mermaid (.mmd) source files.

OPTIONS:
    -h, --help          Show this help message
    -o, --output DIR    Output directory (default: Documentation/Images)
    -f, --format FMT    Output format: png, svg, pdf (default: svg)
    --width WIDTH       Image width in pixels (default: 1920)
    --height HEIGHT     Image height in pixels (default: 1080)
    --theme THEME       Mermaid theme: default, dark, forest, neutral (default: default)
    --background COLOR  Background color (default: white)
    --check             Check if diagrams need regeneration

EXAMPLES:
    $(basename "$0")                              # Generate all diagrams
    $(basename "$0") diagram.mmd                  # Generate specific diagram
    $(basename "$0") --format png --theme dark    # PNG with dark theme

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

check_mermaid_cli() {
    if ! command -v "$MERMAID_CLI" &> /dev/null; then
        log_error "Mermaid CLI is not installed"
        log_info "Install with:"
        log_info "  npm install -g @mermaid-js/mermaid-cli"
        log_info ""
        log_info "Or use Docker:"
        log_info "  docker pull minlag/mermaid-cli"
        log_info "  alias mmdc='docker run --rm -v \$(pwd):/data minlag/mermaid-cli'"
        return 1
    fi

    local version
    version="$($MERMAID_CLI --version 2>&1 | head -1)"
    log_info "Found Mermaid CLI: $version"
}

find_mermaid_files() {
    local search_dir="${1:-.}"
    find "$search_dir" -name "*.mmd" -type f 2>/dev/null
}

check_if_stale() {
    local source="$1"
    local output="$2"

    if [[ ! -f "$output" ]]; then
        return 1  # Output doesn't exist, needs generation
    fi

    if [[ "$source" -nt "$output" ]]; then
        return 1  # Source is newer, needs regeneration
    fi

    return 0  # Up-to-date
}

generate_diagram() {
    local source="$1"
    local output="$2"
    local format="$3"
    local width="$4"
    local height="$5"
    local theme="$6"
    local background="$7"

    log_info "Generating: $(basename "$source") -> $(basename "$output")"

    # Build mermaid-cli command
    local cmd=(
        "$MERMAID_CLI"
        -i "$source"
        -o "$output"
        -w "$width"
        -H "$height"
        -t "$theme"
        -b "$background"
    )

    if "${cmd[@]}" 2>&1; then
        log_info "Generated: $output"
        return 0
    else
        log_error "Failed to generate: $output"
        return 1
    fi
}

main() {
    local output_dir="$IMAGES_DIR"
    local format="svg"
    local width="1920"
    local height="1080"
    local theme="default"
    local background="white"
    local check_only=false
    local files=()

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)
                usage
                exit 0
                ;;
            -o|--output)
                output_dir="$2"
                shift 2
                ;;
            -f|--format)
                format="$2"
                shift 2
                ;;
            --width)
                width="$2"
                shift 2
                ;;
            --height)
                height="$2"
                shift 2
                ;;
            --theme)
                theme="$2"
                shift 2
                ;;
            --background)
                background="$2"
                shift 2
                ;;
            --check)
                check_only=true
                shift
                ;;
            -*)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
            *)
                files+=("$1")
                shift
                ;;
        esac
    done

    # Validate format
    if [[ ! "$format" =~ ^(png|svg|pdf)$ ]]; then
        log_error "Invalid format: $format"
        log_info "Valid formats: png, svg, pdf"
        exit 1
    fi

    # Validate theme
    if [[ ! "$theme" =~ ^(default|dark|forest|neutral)$ ]]; then
        log_error "Invalid theme: $theme"
        log_info "Valid themes: default, dark, forest, neutral"
        exit 1
    fi

    # Check for mermaid-cli
    check_mermaid_cli || exit 1

    # Create output directory
    mkdir -p "$output_dir"

    # Find files to process
    if [[ ${#files[@]} -eq 0 ]]; then
        # No files specified, find all .mmd files
        log_info "Searching for .mmd files in $DOCS_DIR..."
        mapfile -t files < <(find_mermaid_files "$DOCS_DIR")

        if [[ ${#files[@]} -eq 0 ]]; then
            log_warn "No .mmd files found in $DOCS_DIR"
            exit 0
        fi

        log_info "Found ${#files[@]} Mermaid files"
    fi

    # Process each file
    local generated=0
    local skipped=0
    local failed=0

    for source in "${files[@]}"; do
        if [[ ! -f "$source" ]]; then
            log_warn "File not found: $source"
            continue
        fi

        # Determine output filename
        local basename
        basename="$(basename "$source" .mmd)"
        local output="$output_dir/${basename}.${format}"

        # Check if regeneration needed
        if [[ "$check_only" == true ]]; then
            if check_if_stale "$source" "$output"; then
                log_info "Up-to-date: $basename"
                ((skipped++))
            else
                log_info "Needs regeneration: $basename"
                ((generated++))
            fi
            continue
        fi

        if check_if_stale "$source" "$output"; then
            log_info "Skipping (up-to-date): $basename"
            ((skipped++))
            continue
        fi

        # Generate diagram
        if generate_diagram "$source" "$output" "$format" "$width" "$height" "$theme" "$background"; then
            ((generated++))
        else
            ((failed++))
        fi
    done

    # Summary
    log_info "Summary:"
    log_info "  Generated: $generated"
    log_info "  Skipped: $skipped"
    if [[ $failed -gt 0 ]]; then
        log_error "  Failed: $failed"
        exit 1
    fi

    if [[ "$check_only" == true ]] && [[ $generated -gt 0 ]]; then
        log_warn "Some diagrams need regeneration"
        exit 1
    fi

    log_info "Done"
}

main "$@"
