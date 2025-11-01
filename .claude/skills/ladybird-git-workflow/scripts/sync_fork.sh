#!/bin/bash
# Sync fork with upstream Ladybird repository
# Usage: ./scripts/sync_fork.sh [--rebase]

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Default to merge strategy
STRATEGY="merge"
if [ "$1" = "--rebase" ]; then
    STRATEGY="rebase"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${BLUE}Syncing fork with upstream Ladybird${NC}"
echo "Strategy: $STRATEGY"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Check if on master branch
current_branch=$(git branch --show-current)
if [ "$current_branch" != "master" ]; then
    echo -e "${RED}ERROR: Not on master branch${NC}"
    echo "Current branch: $current_branch"
    echo "Switch to master: git checkout master"
    exit 1
fi

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}ERROR: Uncommitted changes detected${NC}"
    echo "Stash or commit changes before syncing"
    git status --short
    exit 1
fi

# Fetch upstream
echo ""
echo "📥 Fetching upstream..."
git fetch upstream

# Get commits behind/ahead
BEHIND=$(git rev-list --count HEAD..upstream/master)
AHEAD=$(git rev-list --count upstream/master..HEAD)

echo ""
echo "Status:"
echo "  Behind upstream: $BEHIND commits"
echo "  Ahead of upstream: $AHEAD commits (fork-specific changes)"

if [ "$BEHIND" -eq 0 ]; then
    echo -e "${GREEN}✓ Already up to date with upstream${NC}"
    exit 0
fi

# Show what will be merged/rebased
echo ""
echo "New commits from upstream:"
git log --oneline HEAD..upstream/master | head -10
if [ "$BEHIND" -gt 10 ]; then
    echo "... and $((BEHIND - 10)) more commits"
fi

echo ""
read -p "Continue with $STRATEGY? (y/N) " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted"
    exit 0
fi

# Sync based on strategy
if [ "$STRATEGY" = "merge" ]; then
    echo ""
    echo "🔀 Merging upstream/master..."

    # Create merge commit with summary
    FIRST_COMMIT=$(git log --oneline upstream/master..HEAD^1 -1 --format=%h 2>/dev/null || echo "N/A")
    LAST_COMMIT=$(git log --oneline HEAD..upstream/master -1 --format=%h)

    MERGE_MSG="Merge upstream/master: $(git log --oneline HEAD..upstream/master | head -1 | cut -d' ' -f2-)

Integrated $BEHIND commits from upstream Ladybird.

Notable changes:
$(git log --oneline HEAD..upstream/master | head -5 | sed 's/^/- /')
"

    if git merge upstream/master --no-ff -m "$MERGE_MSG"; then
        echo -e "${GREEN}✓ Merge successful${NC}"
    else
        echo -e "${RED}✗ Merge conflicts detected${NC}"
        echo ""
        echo "Resolve conflicts in:"
        git status --short | grep "^UU"
        echo ""
        echo "Then:"
        echo "  git add <resolved-files>"
        echo "  git merge --continue"
        exit 1
    fi

else  # rebase
    echo ""
    echo "🔄 Rebasing on upstream/master..."
    echo -e "${YELLOW}WARNING: This rewrites history!${NC}"

    if git rebase upstream/master; then
        echo -e "${GREEN}✓ Rebase successful${NC}"
        echo ""
        echo -e "${YELLOW}Force push required: git push --force-with-lease origin master${NC}"
    else
        echo -e "${RED}✗ Rebase conflicts detected${NC}"
        echo ""
        echo "Resolve conflicts in:"
        git status --short | grep "^UU"
        echo ""
        echo "Then:"
        echo "  git add <resolved-files>"
        echo "  git rebase --continue"
        echo ""
        echo "Or abort:"
        echo "  git rebase --abort"
        exit 1
    fi
fi

# Check if fork-specific code still compiles
echo ""
echo "🔨 Testing build after sync..."
if ./Meta/ladybird.py build > /tmp/sync-build.log 2>&1; then
    echo -e "${GREEN}✓ Build successful${NC}"
else
    echo -e "${RED}✗ Build failed - upstream changes may have broken fork code${NC}"
    tail -20 /tmp/sync-build.log
    echo ""
    echo "Full log: /tmp/sync-build.log"
    echo ""
    echo "Common issues:"
    echo "  - API changes in LibWeb/LibJS"
    echo "  - Signature changes in base classes"
    echo "  - Moved/renamed files"
    echo ""
    echo "Create a fix commit:"
    echo "  git checkout -b fix/upstream-api-changes"
    echo "  # Fix compilation errors"
    echo "  git commit -m 'fix: Update for upstream API changes'"
    echo "  git checkout master"
    echo "  git merge fix/upstream-api-changes"
    exit 1
fi

# Push to origin
echo ""
if [ "$STRATEGY" = "merge" ]; then
    echo "📤 Pushing to origin..."
    if git push origin master; then
        echo -e "${GREEN}✓ Pushed to origin${NC}"
    else
        echo -e "${RED}✗ Push failed${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠ Rebase complete - force push required:${NC}"
    echo "  git push --force-with-lease origin master"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✓ Fork sync complete${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Summary:"
echo "  Merged: $BEHIND commits from upstream"
echo "  Fork commits preserved: $AHEAD commits"
echo ""
echo "Next steps:"
echo "  - Test fork-specific features"
echo "  - Run full test suite: ./Meta/ladybird.py test"
echo "  - Check for deprecation warnings"
