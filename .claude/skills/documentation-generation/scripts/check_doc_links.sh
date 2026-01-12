#!/usr/bin/env bash
# Check for broken links in markdown documentation

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Configuration
DOCS_DIRS=("${PROJECT_ROOT}/Documentation" "${PROJECT_ROOT}/docs")
IGNORE_EXTERNAL=false
IGNORE_ANCHORS=false

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS] [FILES...]

Check for broken links in markdown documentation files.

OPTIONS:
    -h, --help              Show this help message
    -d, --dir DIR           Documentation directory to check (can be repeated)
    --ignore-external       Skip checking external HTTP(S) links
    --ignore-anchors        Skip checking heading anchors
    -v, --verbose           Show all checked links, not just broken ones

EXAMPLES:
    $(basename "$0")                    # Check all markdown files
    $(basename "$0") README.md          # Check specific file
    $(basename "$0") --ignore-external  # Only check internal links

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

find_markdown_files() {
    local dir="$1"
    find "$dir" -name "*.md" -type f 2>/dev/null || true
}

extract_links() {
    local file="$1"
    # Extract markdown links: [text](url)
    grep -oP '\[([^\]]+)\]\(([^)]+)\)' "$file" 2>/dev/null | \
        sed -n 's/\[.*\](\(.*\))/\1/p' || true
}

check_internal_file() {
    local base_file="$1"
    local link="$2"
    local base_dir
    base_dir="$(dirname "$base_file")"

    # Handle absolute vs relative paths
    local target_file
    if [[ "$link" =~ ^/ ]]; then
        target_file="${PROJECT_ROOT}${link}"
    else
        target_file="${base_dir}/${link}"
    fi

    # Resolve .. and . in path
    target_file="$(realpath -m "$target_file" 2>/dev/null || echo "$target_file")"

    if [[ -f "$target_file" ]] || [[ -d "$target_file" ]]; then
        return 0
    else
        return 1
    fi
}

check_anchor() {
    local file="$1"
    local anchor="$2"

    # Extract headings from markdown
    local headings
    headings="$(grep -oP '^#{1,6}\s+\K.*' "$file" 2>/dev/null || true)"

    # Convert heading to anchor format
    # GitHub converts: "Heading Text" -> "heading-text"
    local expected_anchor
    expected_anchor="$(echo "$anchor" | tr '[:upper:]' '[:lower:]' | tr ' ' '-' | tr -cd '[:alnum:]-')"

    while IFS= read -r heading; do
        local heading_anchor
        heading_anchor="$(echo "$heading" | tr '[:upper:]' '[:lower:]' | tr ' ' '-' | tr -cd '[:alnum:]-')"
        if [[ "$heading_anchor" == "$expected_anchor" ]]; then
            return 0
        fi
    done <<< "$headings"

    return 1
}

check_external_link() {
    local url="$1"

    # Use curl to check if URL is accessible
    if curl --output /dev/null --silent --head --fail --max-time 10 "$url" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

check_link() {
    local file="$1"
    local link="$2"
    local verbose="$3"

    # Skip empty links
    if [[ -z "$link" ]]; then
        return 0
    fi

    # Handle different link types
    if [[ "$link" =~ ^https?:// ]]; then
        # External link
        if [[ "$IGNORE_EXTERNAL" == true ]]; then
            return 0
        fi

        if [[ "$verbose" == true ]]; then
            log_info "Checking external: $link"
        fi

        if check_external_link "$link"; then
            return 0
        else
            log_error "Broken external link in $(basename "$file"): $link"
            return 1
        fi

    elif [[ "$link" =~ ^# ]]; then
        # Anchor in current file
        if [[ "$IGNORE_ANCHORS" == true ]]; then
            return 0
        fi

        local anchor="${link#\#}"
        if [[ "$verbose" == true ]]; then
            log_info "Checking anchor: #$anchor"
        fi

        if check_anchor "$file" "$anchor"; then
            return 0
        else
            log_error "Broken anchor in $(basename "$file"): $link"
            return 1
        fi

    elif [[ "$link" =~ ^[^/]+\.(md|html)# ]]; then
        # Link with anchor: file.md#section
        local file_part="${link%%\#*}"
        local anchor_part="${link#*\#}"

        if [[ "$verbose" == true ]]; then
            log_info "Checking file+anchor: $link"
        fi

        if ! check_internal_file "$file" "$file_part"; then
            log_error "Broken link in $(basename "$file"): $file_part (file not found)"
            return 1
        fi

        if [[ "$IGNORE_ANCHORS" == false ]]; then
            local base_dir
            base_dir="$(dirname "$file")"
            local target_file="${base_dir}/${file_part}"

            if ! check_anchor "$target_file" "$anchor_part"; then
                log_error "Broken anchor in $(basename "$file"): $link (anchor not found)"
                return 1
            fi
        fi

        return 0

    else
        # Internal file link
        if [[ "$verbose" == true ]]; then
            log_info "Checking internal: $link"
        fi

        if check_internal_file "$file" "$link"; then
            return 0
        else
            log_error "Broken link in $(basename "$file"): $link"
            return 1
        fi
    fi
}

check_file() {
    local file="$1"
    local verbose="$2"
    local broken_count=0
    local total_count=0

    log_info "Checking: $(realpath --relative-to="$PROJECT_ROOT" "$file" 2>/dev/null || basename "$file")"

    # Extract all links from file
    local links
    links="$(extract_links "$file")"

    if [[ -z "$links" ]]; then
        return 0
    fi

    while IFS= read -r link; do
        ((total_count++))
        if ! check_link "$file" "$link" "$verbose"; then
            ((broken_count++))
        fi
    done <<< "$links"

    if [[ $broken_count -gt 0 ]]; then
        log_error "Found $broken_count broken links in $(basename "$file") (out of $total_count total)"
        return 1
    else
        log_info "All $total_count links OK in $(basename "$file")"
        return 0
    fi
}

main() {
    local files=()
    local verbose=false
    local dirs=()

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)
                usage
                exit 0
                ;;
            -d|--dir)
                dirs+=("$2")
                shift 2
                ;;
            --ignore-external)
                IGNORE_EXTERNAL=true
                shift
                ;;
            --ignore-anchors)
                IGNORE_ANCHORS=true
                shift
                ;;
            -v|--verbose)
                verbose=true
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

    # Use default dirs if none specified
    if [[ ${#dirs[@]} -eq 0 ]]; then
        dirs=("${DOCS_DIRS[@]}")
    fi

    # Find files to check
    if [[ ${#files[@]} -eq 0 ]]; then
        log_info "Searching for markdown files..."
        for dir in "${dirs[@]}"; do
            if [[ -d "$dir" ]]; then
                while IFS= read -r file; do
                    files+=("$file")
                done < <(find_markdown_files "$dir")
            fi
        done

        if [[ ${#files[@]} -eq 0 ]]; then
            log_warn "No markdown files found"
            exit 0
        fi

        log_info "Found ${#files[@]} markdown files"
    fi

    # Check each file
    local failed_files=0
    local checked_files=0

    for file in "${files[@]}"; do
        if [[ ! -f "$file" ]]; then
            log_warn "File not found: $file"
            continue
        fi

        ((checked_files++))
        if ! check_file "$file" "$verbose"; then
            ((failed_files++))
        fi
    done

    # Summary
    log_info "Summary:"
    log_info "  Files checked: $checked_files"
    if [[ $failed_files -gt 0 ]]; then
        log_error "  Files with broken links: $failed_files"
        exit 1
    else
        log_info "  Files with broken links: 0"
        log_info "All links are valid!"
    fi
}

main "$@"
