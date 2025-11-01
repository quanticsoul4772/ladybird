#!/usr/bin/env bash
# Wrapper for Tests/LibWeb/add_libweb_test.py with enhanced features

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LADYBIRD_ROOT="$(cd "$SCRIPT_DIR/../../../.." && pwd)"

# Color output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

usage() {
    cat <<EOF
Usage: $0 <test-name> <test-type> [options]

Create a new LibWeb test with boilerplate and optionally run/rebaseline.

Arguments:
    test-name    Name of the test (kebab-case recommended)
    test-type    Type: Text, Layout, Ref, or Screenshot

Options:
    --async           Create async test (Text tests only)
    --run             Run the test after creation
    --rebaseline      Run and rebaseline the test after creation
    --edit            Open test in editor after creation
    --spec URL        Add spec reference to test (as comment)

Examples:
    # Create simple text test
    $0 element-clone-node Text

    # Create async text test and rebaseline
    $0 fetch-api-test Text --async --rebaseline

    # Create layout test and open in editor
    $0 grid-layout Layout --edit

    # Create ref test with spec reference
    $0 box-shadow Ref --spec https://www.w3.org/TR/css-backgrounds-3/#box-shadow
EOF
    exit 1
}

if [ $# -lt 2 ]; then
    usage
fi

TEST_NAME="$1"
TEST_TYPE="$2"
shift 2

# Default options
ASYNC_FLAG=""
RUN_TEST=false
REBASELINE_TEST=false
OPEN_EDITOR=false
SPEC_URL=""

# Parse options
while [[ $# -gt 0 ]]; do
    case "$1" in
        --async)
            ASYNC_FLAG="--async"
            shift
            ;;
        --run)
            RUN_TEST=true
            shift
            ;;
        --rebaseline)
            REBASELINE_TEST=true
            RUN_TEST=true
            shift
            ;;
        --edit)
            OPEN_EDITOR=true
            shift
            ;;
        --spec)
            SPEC_URL="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            echo -e "${RED}Error: Unknown option $1${NC}"
            usage
            ;;
    esac
done

# Validate test type
case "$TEST_TYPE" in
    Text|Layout|Ref|Screenshot)
        ;;
    *)
        echo -e "${RED}Error: Invalid test type '$TEST_TYPE'${NC}"
        echo -e "Valid types: Text, Layout, Ref, Screenshot"
        exit 1
        ;;
esac

cd "$LADYBIRD_ROOT"

# Create test
echo -e "${GREEN}Creating ${TEST_TYPE} test: ${TEST_NAME}${NC}"
if [ -n "$ASYNC_FLAG" ]; then
    echo -e "${YELLOW}Mode: Async${NC}"
fi
echo ""

./Tests/LibWeb/add_libweb_test.py "$TEST_NAME" "$TEST_TYPE" $ASYNC_FLAG

TEST_FILE="Tests/LibWeb/${TEST_TYPE}/input/${TEST_NAME}.html"

# Add spec reference if provided
if [ -n "$SPEC_URL" ]; then
    echo -e "${BLUE}Adding spec reference: ${SPEC_URL}${NC}"

    # Insert spec comment after <!DOCTYPE html>
    SPEC_COMMENT="<!-- Spec: ${SPEC_URL} -->"
    sed -i "1 a\\${SPEC_COMMENT}" "$TEST_FILE"
fi

echo -e "${GREEN}Test created: ${TEST_FILE}${NC}"

# Open in editor if requested
if [ "$OPEN_EDITOR" = true ]; then
    EDITOR="${EDITOR:-vim}"
    echo -e "${BLUE}Opening in editor: ${EDITOR}${NC}"
    "$EDITOR" "$TEST_FILE"
fi

# Run test if requested
if [ "$RUN_TEST" = true ]; then
    echo -e "${BLUE}Running test...${NC}"

    if [ "$REBASELINE_TEST" = true ]; then
        echo -e "${YELLOW}Rebaselining test${NC}"
        ./Meta/ladybird.py run test-web --rebaseline -f "${TEST_TYPE}/input/${TEST_NAME}.html"
    else
        ./Meta/ladybird.py run test-web -f "${TEST_TYPE}/input/${TEST_NAME}.html"
    fi
fi

# Show next steps
echo ""
echo -e "${GREEN}Next steps:${NC}"
echo -e "1. Edit test:      ${YELLOW}vim ${TEST_FILE}${NC}"

if [ "$TEST_TYPE" = "Text" ] || [ "$TEST_TYPE" = "Layout" ]; then
    echo -e "2. Rebaseline:     ${YELLOW}./Meta/ladybird.py run test-web --rebaseline -f ${TEST_TYPE}/input/${TEST_NAME}.html${NC}"
elif [ "$TEST_TYPE" = "Ref" ]; then
    EXPECTED_FILE="Tests/LibWeb/Ref/expected/${TEST_NAME}-ref.html"
    echo -e "2. Edit reference: ${YELLOW}vim ${EXPECTED_FILE}${NC}"
elif [ "$TEST_TYPE" = "Screenshot" ]; then
    echo -e "2. Generate screenshot:"
    echo -e "   ${YELLOW}./Meta/ladybird.py run ladybird --headless --layout-test-mode ${TEST_FILE}${NC}"
    echo -e "   ${YELLOW}mv ~/Downloads/screenshot-*.png Tests/LibWeb/Screenshot/images/${TEST_NAME}-ref.png${NC}"
fi

echo -e "3. Run test:       ${YELLOW}./Meta/ladybird.py run test-web -f ${TEST_TYPE}/input/${TEST_NAME}.html${NC}"
echo -e "4. Commit:         ${YELLOW}git add Tests/LibWeb/${TEST_TYPE}/ && git commit${NC}"
