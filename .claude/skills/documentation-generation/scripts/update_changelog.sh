#!/usr/bin/env bash
# Generate changelog entries from git commits

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Configuration
CHANGELOG_FILE="${PROJECT_ROOT}/CHANGELOG.md"
CATEGORIES=("Added" "Changed" "Deprecated" "Removed" "Fixed" "Security")

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Generate changelog entries from git commit history.

OPTIONS:
    -h, --help              Show this help message
    -s, --since TAG         Generate entries since tag/commit (default: latest tag)
    -u, --until TAG         Generate entries until tag/commit (default: HEAD)
    -o, --output FILE       Output file (default: CHANGELOG.md)
    --dry-run               Print changelog without modifying file
    --format FORMAT         Output format: markdown, json (default: markdown)

COMMIT MESSAGE CONVENTIONS:
    Commits are categorized based on prefixes:
    - "Add:" or "feat:" -> Added
    - "Change:" or "refactor:" -> Changed
    - "Deprecate:" -> Deprecated
    - "Remove:" -> Removed
    - "Fix:" or "bug:" -> Fixed
    - "Security:" or "sec:" -> Security

EXAMPLES:
    $(basename "$0")                        # Generate unreleased changes
    $(basename "$0") --since v1.0.0         # Changes since v1.0.0
    $(basename "$0") --dry-run              # Preview without writing

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

get_latest_tag() {
    git describe --tags --abbrev=0 2>/dev/null || echo ""
}

categorize_commit() {
    local message="$1"
    local category="Changed"  # Default category

    # Check commit message prefix
    if [[ "$message" =~ ^(Add:|feat:) ]]; then
        category="Added"
    elif [[ "$message" =~ ^(Change:|refactor:) ]]; then
        category="Changed"
    elif [[ "$message" =~ ^Deprecate: ]]; then
        category="Deprecated"
    elif [[ "$message" =~ ^Remove: ]]; then
        category="Removed"
    elif [[ "$message" =~ ^(Fix:|bug:) ]]; then
        category="Fixed"
    elif [[ "$message" =~ ^(Security:|sec:) ]]; then
        category="Security"
    fi

    echo "$category"
}

extract_commit_info() {
    local hash="$1"
    local subject
    local author
    local date

    subject="$(git log -1 --pretty=format:'%s' "$hash")"
    author="$(git log -1 --pretty=format:'%an' "$hash")"
    date="$(git log -1 --pretty=format:'%ad' --date=short "$hash")"

    echo "$subject|$author|$date"
}

generate_markdown_changelog() {
    local since="$1"
    local until="$2"

    log_info "Generating changelog from $since to $until"

    # Get commits in range
    local commits
    if [[ -n "$since" ]]; then
        commits="$(git log --pretty=format:'%H' "$since..$until" 2>/dev/null || true)"
    else
        commits="$(git log --pretty=format:'%H' "$until" 2>/dev/null || true)"
    fi

    if [[ -z "$commits" ]]; then
        log_warn "No commits found in range"
        return 1
    fi

    # Organize commits by category
    declare -A categorized_commits
    for category in "${CATEGORIES[@]}"; do
        categorized_commits[$category]=""
    done

    while IFS= read -r hash; do
        local info
        info="$(extract_commit_info "$hash")"
        local subject="${info%%|*}"
        local rest="${info#*|}"
        local author="${rest%%|*}"
        local date="${rest#*|}"

        # Remove conventional commit prefix
        subject="$(echo "$subject" | sed -E 's/^(Add|Change|Fix|Remove|Deprecate|Security|feat|bug|refactor|sec):\s*//')"

        local category
        category="$(categorize_commit "$(git log -1 --pretty=format:'%s' "$hash")")"

        categorized_commits[$category]+="- $subject\n"
    done <<< "$commits"

    # Generate markdown
    echo "## [Unreleased]"
    echo ""

    for category in "${CATEGORIES[@]}"; do
        if [[ -n "${categorized_commits[$category]}" ]]; then
            echo "### $category"
            echo ""
            echo -e "${categorized_commits[$category]}"
        fi
    done
}

generate_json_changelog() {
    local since="$1"
    local until="$2"

    echo "{"
    echo "  \"version\": \"Unreleased\","
    echo "  \"date\": \"$(date -I)\","
    echo "  \"changes\": {"

    local first_category=true
    for category in "${CATEGORIES[@]}"; do
        if [[ "$first_category" == false ]]; then
            echo ","
        fi
        first_category=false

        echo "    \"$(echo "$category" | tr '[:upper:]' '[:lower:]')\": ["

        # Get commits for this category
        local commits
        if [[ -n "$since" ]]; then
            commits="$(git log --pretty=format:'%H' "$since..$until" 2>/dev/null || true)"
        else
            commits="$(git log --pretty=format:'%H' "$until" 2>/dev/null || true)"
        fi

        local first_commit=true
        while IFS= read -r hash; do
            local commit_category
            commit_category="$(categorize_commit "$(git log -1 --pretty=format:'%s' "$hash")")"

            if [[ "$commit_category" == "$category" ]]; then
                local info
                info="$(extract_commit_info "$hash")"
                local subject="${info%%|*}"

                # Remove prefix
                subject="$(echo "$subject" | sed -E 's/^(Add|Change|Fix|Remove|Deprecate|Security|feat|bug|refactor|sec):\s*//')"

                if [[ "$first_commit" == false ]]; then
                    echo ","
                fi
                first_commit=false

                echo -n "      \"$subject\""
            fi
        done <<< "$commits"

        echo ""
        echo -n "    ]"
    done

    echo ""
    echo "  }"
    echo "}"
}

update_changelog_file() {
    local content="$1"
    local dry_run="$2"

    if [[ "$dry_run" == true ]]; then
        log_info "Dry run - would add the following to $CHANGELOG_FILE:"
        echo "----------------------------------------"
        echo "$content"
        echo "----------------------------------------"
        return 0
    fi

    # Check if CHANGELOG.md exists
    if [[ ! -f "$CHANGELOG_FILE" ]]; then
        log_warn "Creating new $CHANGELOG_FILE"
        cat > "$CHANGELOG_FILE" <<EOF
# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

EOF
    fi

    # Insert new content after header
    local temp_file
    temp_file="$(mktemp)"

    # Extract header (everything before first ## version)
    sed -n '1,/^## \[/p' "$CHANGELOG_FILE" | head -n -1 > "$temp_file"

    # Add new content
    echo "$content" >> "$temp_file"
    echo "" >> "$temp_file"

    # Add existing versions
    sed -n '/^## \[/,$p' "$CHANGELOG_FILE" >> "$temp_file"

    # Replace original file
    mv "$temp_file" "$CHANGELOG_FILE"

    log_info "Updated $CHANGELOG_FILE"
}

main() {
    local since=""
    local until="HEAD"
    local output_file="$CHANGELOG_FILE"
    local dry_run=false
    local format="markdown"

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)
                usage
                exit 0
                ;;
            -s|--since)
                since="$2"
                shift 2
                ;;
            -u|--until)
                until="$2"
                shift 2
                ;;
            -o|--output)
                output_file="$2"
                shift 2
                ;;
            --dry-run)
                dry_run=true
                shift
                ;;
            --format)
                format="$2"
                shift 2
                ;;
            -*)
                log_error "Unknown option: $1"
                usage
                exit 1
                ;;
            *)
                log_error "Unexpected argument: $1"
                usage
                exit 1
                ;;
        esac
    done

    # Validate format
    if [[ ! "$format" =~ ^(markdown|json)$ ]]; then
        log_error "Invalid format: $format"
        log_info "Valid formats: markdown, json"
        exit 1
    fi

    # Check if we're in a git repository
    if ! git rev-parse --git-dir &>/dev/null; then
        log_error "Not in a git repository"
        exit 1
    fi

    cd "$PROJECT_ROOT"

    # Get latest tag if since not specified
    if [[ -z "$since" ]]; then
        since="$(get_latest_tag)"
        if [[ -z "$since" ]]; then
            log_warn "No tags found, generating all commits"
        else
            log_info "Using latest tag: $since"
        fi
    fi

    # Generate changelog
    local content
    if [[ "$format" == "markdown" ]]; then
        content="$(generate_markdown_changelog "$since" "$until")"
    else
        content="$(generate_json_changelog "$since" "$until")"
    fi

    # Output or update file
    if [[ "$format" == "json" ]] || [[ "$output_file" != "$CHANGELOG_FILE" ]]; then
        # Write to stdout or custom file
        if [[ "$dry_run" == true ]]; then
            echo "$content"
        else
            echo "$content" > "$output_file"
            log_info "Written to $output_file"
        fi
    else
        # Update CHANGELOG.md
        update_changelog_file "$content" "$dry_run"
    fi
}

main "$@"
