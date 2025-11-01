#!/bin/bash
# Pre-commit checks for Ladybird development
# Install: cp scripts/prepare_commit.sh .git/hooks/pre-commit && chmod +x .git/hooks/pre-commit

set -e

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Running pre-commit checks for Ladybird..."
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Track overall status
CHECKS_PASSED=true

# Check 1: Code formatting
echo ""
echo "📝 Checking code formatting..."
if [ -d "Build/release" ]; then
    if ninja -C Build/release check-style 2>&1 | grep -q "FAILED"; then
        echo -e "${RED}✗ Code formatting check failed${NC}"
        echo "  Run: ninja -C Build/release check-style"
        echo "  Or apply fixes automatically (if supported)"
        CHECKS_PASSED=false
    else
        echo -e "${GREEN}✓ Code formatting OK${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Build directory not found, skipping format check${NC}"
fi

# Check 2: No debug code left in
echo ""
echo "🔍 Checking for debug code..."
if git diff --cached | grep -E "console\.log|dbgln.*FIXME|TODO.*REMOVE|XXX|HACK" > /dev/null; then
    echo -e "${RED}✗ Found potential debug code in staged changes:${NC}"
    git diff --cached | grep -E "console\.log|dbgln.*FIXME|TODO.*REMOVE|XXX|HACK" || true
    echo "  Remove debug code before committing"
    CHECKS_PASSED=false
else
    echo -e "${GREEN}✓ No debug code found${NC}"
fi

# Check 3: No large files
echo ""
echo "📦 Checking for large files..."
LARGE_FILES=$(git diff --cached --name-only | xargs -I {} ls -l {} 2>/dev/null | awk '$5 > 1048576 {print $9, $5}')
if [ -n "$LARGE_FILES" ]; then
    echo -e "${RED}✗ Found files larger than 1MB:${NC}"
    echo "$LARGE_FILES"
    echo "  Consider using Git LFS or excluding from repo"
    CHECKS_PASSED=false
else
    echo -e "${GREEN}✓ No large files${NC}"
fi

# Check 4: No sensitive data
echo ""
echo "🔒 Checking for sensitive data..."
SENSITIVE_PATTERNS="(password|secret|api[_-]?key|private[_-]?key|token|credential)"
if git diff --cached | grep -iE "$SENSITIVE_PATTERNS" > /dev/null; then
    echo -e "${YELLOW}⚠ Found potential sensitive data patterns:${NC}"
    git diff --cached | grep -iE "$SENSITIVE_PATTERNS" | head -5 || true
    echo "  Review carefully before committing"
    # Don't fail, just warn
fi

# Check 5: Compilation (optional, disabled by default as it's slow)
if [ "$LADYBIRD_PRECOMMIT_BUILD" = "1" ]; then
    echo ""
    echo "🔨 Building project..."
    if ./Meta/ladybird.py build > /tmp/precommit-build.log 2>&1; then
        echo -e "${GREEN}✓ Build successful${NC}"
    else
        echo -e "${RED}✗ Build failed${NC}"
        tail -20 /tmp/precommit-build.log
        echo "  Full log: /tmp/precommit-build.log"
        CHECKS_PASSED=false
    fi
else
    echo ""
    echo -e "${YELLOW}⚠ Skipping build check (set LADYBIRD_PRECOMMIT_BUILD=1 to enable)${NC}"
fi

# Check 6: Tests (optional, disabled by default as it's slow)
if [ "$LADYBIRD_PRECOMMIT_TEST" = "1" ]; then
    echo ""
    echo "🧪 Running tests..."
    if ./Meta/ladybird.py test > /tmp/precommit-test.log 2>&1; then
        echo -e "${GREEN}✓ Tests passed${NC}"
    else
        echo -e "${RED}✗ Tests failed${NC}"
        tail -20 /tmp/precommit-test.log
        echo "  Full log: /tmp/precommit-test.log"
        CHECKS_PASSED=false
    fi
else
    echo ""
    echo -e "${YELLOW}⚠ Skipping tests (set LADYBIRD_PRECOMMIT_TEST=1 to enable)${NC}"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
if [ "$CHECKS_PASSED" = true ]; then
    echo -e "${GREEN}✓ All pre-commit checks passed${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    exit 0
else
    echo -e "${RED}✗ Some pre-commit checks failed${NC}"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "To bypass this check (not recommended):"
    echo "  git commit --no-verify"
    exit 1
fi
