#!/bin/bash
# Verify Core Browser Test Suite Implementation
# Usage: ./verify-tests.sh

set -e

echo "====================================="
echo "Core Browser Test Suite Verification"
echo "====================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

cd "$(dirname "$0")"

echo -e "${BLUE}1. Checking test files exist...${NC}"
test_files=(
    "tests/core-browser/navigation.spec.ts"
    "tests/core-browser/tabs.spec.ts"
    "tests/core-browser/history.spec.ts"
    "tests/core-browser/bookmarks.spec.ts"
)

for file in "${test_files[@]}"; do
    if [ -f "$file" ]; then
        echo -e "  ${GREEN}✓${NC} $file"
    else
        echo -e "  ✗ $file - MISSING!"
        exit 1
    fi
done
echo ""

echo -e "${BLUE}2. Counting test cases...${NC}"
nav_count=$(grep -c "^  test(" tests/core-browser/navigation.spec.ts || echo 0)
tabs_count=$(grep -c "^  test(" tests/core-browser/tabs.spec.ts || echo 0)
hist_count=$(grep -c "^  test(" tests/core-browser/history.spec.ts || echo 0)
book_count=$(grep -c "^  test(" tests/core-browser/bookmarks.spec.ts || echo 0)

echo "  navigation.spec.ts: $nav_count tests (expected 15)"
echo "  tabs.spec.ts: $tabs_count tests (expected 12)"
echo "  history.spec.ts: $hist_count tests (expected 8)"
echo "  bookmarks.spec.ts: $book_count tests (expected 10)"

total=$((nav_count + tabs_count + hist_count + book_count))
echo "  ---"
echo "  TOTAL: $total tests (expected 45)"
echo ""

if [ $total -eq 45 ]; then
    echo -e "${GREEN}✓ Test count verification PASSED${NC}"
else
    echo "✗ Test count mismatch! Expected 45, got $total"
    exit 1
fi
echo ""

echo -e "${BLUE}3. Checking test structure...${NC}"
for file in "${test_files[@]}"; do
    # Check for test.describe
    if grep -q "test.describe(" "$file"; then
        echo -e "  ${GREEN}✓${NC} $file has test.describe"
    else
        echo "  ✗ $file missing test.describe"
        exit 1
    fi

    # Check for priority tags
    if grep -q "tag: '@p" "$file"; then
        echo -e "  ${GREEN}✓${NC} $file has priority tags"
    else
        echo "  ✗ $file missing priority tags"
        exit 1
    fi
done
echo ""

echo -e "${BLUE}4. Lines of code...${NC}"
total_lines=$(wc -l tests/core-browser/*.ts | tail -1 | awk '{print $1}')
echo "  Total: $total_lines lines across 4 test files"
echo ""

echo -e "${BLUE}5. Test file sizes...${NC}"
ls -lh tests/core-browser/*.ts | awk '{print "  " $9 ": " $5}'
echo ""

echo -e "${GREEN}====================================="
echo "All verifications PASSED!"
echo "=====================================${NC}"
echo ""
echo "Next steps:"
echo "  1. npm install (if not already done)"
echo "  2. npx playwright test tests/core-browser/ --list"
echo "  3. npx playwright test tests/core-browser/"
echo ""
