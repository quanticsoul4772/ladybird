#!/usr/bin/env bash
# Rebaseline multiple LibWeb tests at once

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LADYBIRD_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
    cat <<EOF
Usage: $0 [options] <test-pattern>

Rebaseline LibWeb tests matching a pattern.

Arguments:
    test-pattern    Pattern to match tests (supports wildcards)

Options:
    --type TYPE     Only rebaseline tests of this type (Text, Layout, Ref, Screenshot)
    --list          List matching tests without rebaselining
    --interactive   Ask for confirmation before each test
    --help          Show this help

Examples:
    # Rebaseline all DOM tests
    $0 "Text/input/DOM-*.html"

    # Rebaseline all Text tests
    $0 --type Text "*.html"

    # List matching tests
    $0 --list "Layout/input/flex-*.html"

    # Rebaseline with confirmation
    $0 --interactive "Text/input/element-*.html"

    # Rebaseline all tests in subdirectory
    $0 "Text/input/events/*.html"
EOF
    exit 1
}

# Default options
TEST_TYPE=""
LIST_ONLY=false
INTERACTIVE=false
PATTERN=""

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --type)
            TEST_TYPE="$2"
            shift 2
            ;;
        --list)
            LIST_ONLY=true
            shift
            ;;
        --interactive)
            INTERACTIVE=true
            shift
            ;;
        --help)
            usage
            ;;
        -*)
            echo -e "${RED}Error: Unknown option $1${NC}"
            usage
            ;;
        *)
            PATTERN="$1"
            shift
            ;;
    esac
done

if [ -z "$PATTERN" ]; then
    usage
fi

cd "$LADYBIRD_ROOT"

# Build test paths
SEARCH_PATHS=()

if [ -n "$TEST_TYPE" ]; then
    # Specific test type
    case "$TEST_TYPE" in
        Text|Layout|Ref|Screenshot)
            SEARCH_PATHS+=("Tests/LibWeb/${TEST_TYPE}/input/${PATTERN}")
            ;;
        *)
            echo -e "${RED}Error: Invalid test type '$TEST_TYPE'${NC}"
            exit 1
            ;;
    esac
else
    # All test types
    for type in Text Layout Ref Screenshot; do
        SEARCH_PATHS+=("Tests/LibWeb/${type}/input/${PATTERN}")
    done
fi

# Find matching tests
MATCHING_TESTS=()
for path in "${SEARCH_PATHS[@]}"; do
    while IFS= read -r -d '' file; do
        MATCHING_TESTS+=("$file")
    done < <(find Tests/LibWeb -path "$path" -type f -print0 2>/dev/null || true)
done

# Remove duplicates and sort
MATCHING_TESTS=($(printf '%s\n' "${MATCHING_TESTS[@]}" | sort -u))

if [ "${#MATCHING_TESTS[@]}" -eq 0 ]; then
    echo -e "${YELLOW}No tests match pattern: ${PATTERN}${NC}"
    exit 0
fi

echo -e "${BLUE}Found ${#MATCHING_TESTS[@]} matching tests${NC}"
echo ""

# List mode
if [ "$LIST_ONLY" = true ]; then
    for test in "${MATCHING_TESTS[@]}"; do
        echo -e "  ${GREEN}${test}${NC}"
    done
    exit 0
fi

# Rebaseline tests
REBASELINED=0
SKIPPED=0
FAILED=0

for test in "${MATCHING_TESTS[@]}"; do
    # Extract relative path from Tests/LibWeb/
    RELATIVE_PATH="${test#Tests/LibWeb/}"

    echo -e "${BLUE}Test: ${RELATIVE_PATH}${NC}"

    # Interactive confirmation
    if [ "$INTERACTIVE" = true ]; then
        read -p "Rebaseline this test? [Y/n] " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]] && [[ -n $REPLY ]]; then
            echo -e "${YELLOW}Skipped${NC}"
            ((SKIPPED++))
            echo ""
            continue
        fi
    fi

    # Rebaseline
    if ./Meta/ladybird.py run test-web --rebaseline -f "$RELATIVE_PATH" 2>&1 | grep -q "PASS"; then
        echo -e "${GREEN}✓ Rebaselined${NC}"
        ((REBASELINED++))
    else
        echo -e "${RED}✗ Failed${NC}"
        ((FAILED++))
    fi

    echo ""
done

# Summary
echo -e "${BLUE}Rebaseline summary:${NC}"
echo -e "  ${GREEN}Rebaselined: ${REBASELINED}${NC}"
if [ "$SKIPPED" -gt 0 ]; then
    echo -e "  ${YELLOW}Skipped: ${SKIPPED}${NC}"
fi
if [ "$FAILED" -gt 0 ]; then
    echo -e "  ${RED}Failed: ${FAILED}${NC}"
fi

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
