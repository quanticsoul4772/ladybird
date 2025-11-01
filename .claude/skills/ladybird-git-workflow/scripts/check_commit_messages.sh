#!/bin/bash
# Validate commit message format for Ladybird
# Install: cp scripts/check_commit_messages.sh .git/hooks/commit-msg && chmod +x .git/hooks/commit-msg

commit_msg_file="$1"
commit_msg=$(cat "$commit_msg_file")

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Skip check for merge commits
if echo "$commit_msg" | grep -q "^Merge"; then
    exit 0
fi

# Skip check for revert commits
if echo "$commit_msg" | grep -q "^Revert"; then
    exit 0
fi

# Get first line
first_line=$(echo "$commit_msg" | head -n1)

echo "Validating commit message..."

# Check 1: Format is "Category: Description"
if ! echo "$first_line" | grep -qE '^[A-Za-z0-9_/+-]+: [A-Z]'; then
    echo -e "${RED}ERROR: Invalid commit message format${NC}"
    echo ""
    echo "Expected format:"
    echo "  Category: Brief imperative description"
    echo ""
    echo "Examples:"
    echo "  LibWeb: Add CSS grid support"
    echo "  LibJS: Fix memory leak in running_execution_context()"
    echo "  Sentinel: Improve YARA rule matching performance"
    echo "  RequestServer+Sentinel: Fix critical performance issues"
    echo "  docs: Add fingerprinting detection documentation"
    echo ""
    echo "Your message:"
    echo "  $first_line"
    echo ""
    echo "Common categories:"
    echo "  LibWeb, LibJS, LibGfx, LibCore, AK"
    echo "  RequestServer, WebContent, ImageDecoder"
    echo "  UI/Qt, UI/AppKit"
    echo "  Meta, Tests, docs"
    echo "  Sentinel (fork-specific)"
    exit 1
fi

# Check 2: First line length (max 72 chars)
if [ ${#first_line} -gt 72 ]; then
    echo -e "${RED}ERROR: First line too long${NC}"
    echo "  Length: ${#first_line} characters (max 72)"
    echo "  Line: $first_line"
    echo ""
    echo "Tips for shortening:"
    echo "  - Remove unnecessary words"
    echo "  - Use abbreviations (API, CSS, HTML)"
    echo "  - Move details to commit body"
    exit 1
fi

# Check 3: No period at end of first line
if echo "$first_line" | grep -q '\.$'; then
    echo -e "${YELLOW}WARNING: First line should not end with a period${NC}"
    echo "  Line: $first_line"
    echo ""
    echo "Remove the trailing period"
    # Don't fail, just warn
fi

# Check 4: Imperative mood detection (heuristic)
if echo "$first_line" | grep -qE ': (Added|Fixed|Removed|Updated|Changed|Implemented|Created)'; then
    echo -e "${YELLOW}WARNING: Use imperative mood, not past tense${NC}"
    echo "  Line: $first_line"
    echo ""
    echo "Use:"
    echo "  Add (not Added)"
    echo "  Fix (not Fixed)"
    echo "  Remove (not Removed)"
    echo "  Update (not Updated)"
    echo "  Implement (not Implemented)"
    # Don't fail, just warn
fi

if echo "$first_line" | grep -qE ': (Adding|Fixing|Removing|Updating|Changing|Implementing)'; then
    echo -e "${YELLOW}WARNING: Use imperative mood, not present continuous${NC}"
    echo "  Line: $first_line"
    echo ""
    echo "Use:"
    echo "  Add (not Adding)"
    echo "  Fix (not Fixing)"
    echo "  Update (not Updating)"
    # Don't fail, just warn
fi

# Check 5: Description starts with capital letter
category=$(echo "$first_line" | cut -d':' -f1)
description=$(echo "$first_line" | cut -d':' -f2- | sed 's/^ *//')
first_char=$(echo "$description" | cut -c1)
if ! echo "$first_char" | grep -q '[A-Z]'; then
    echo -e "${RED}ERROR: Description must start with capital letter${NC}"
    echo "  Category: $category"
    echo "  Description: $description"
    echo "  First char: '$first_char'"
    exit 1
fi

# Check 6: Common typos in categories
case "$category" in
    libweb|Libweb|LibWeb:)
        echo -e "${YELLOW}WARNING: Category should be 'LibWeb' (check casing)${NC}"
        ;;
    libjs|Libjs|LibJs)
        echo -e "${YELLOW}WARNING: Category should be 'LibJS' (check casing)${NC}"
        ;;
    Docs|DOCS)
        echo -e "${YELLOW}WARNING: Category should be 'docs' (lowercase)${NC}"
        ;;
esac

echo -e "${GREEN}✓ Commit message format OK${NC}"
exit 0
