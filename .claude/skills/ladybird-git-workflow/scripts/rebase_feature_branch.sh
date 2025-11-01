#!/bin/bash
# Interactive rebase helper for feature branches
# Usage: ./scripts/rebase_feature_branch.sh [branch-name]

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Get branch to rebase (or use current)
if [ -n "$1" ]; then
    BRANCH="$1"
    current_branch=$(git branch --show-current)
    if [ "$current_branch" != "$BRANCH" ]; then
        echo "Switching to branch: $BRANCH"
        git checkout "$BRANCH"
    fi
else
    BRANCH=$(git branch --show-current)
fi

if [ "$BRANCH" = "master" ]; then
    echo -e "${RED}ERROR: Cannot rebase master branch${NC}"
    exit 1
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${BLUE}Interactive Rebase for: $BRANCH${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Check for uncommitted changes
if ! git diff-index --quiet HEAD --; then
    echo -e "${RED}ERROR: Uncommitted changes detected${NC}"
    echo "Stash or commit changes before rebasing"
    git status --short
    exit 1
fi

# Fetch upstream
echo ""
echo "📥 Fetching upstream..."
git fetch upstream

# Get commit count
NUM_COMMITS=$(git rev-list --count upstream/master..HEAD)
echo ""
echo "Commits in branch: $NUM_COMMITS"

if [ "$NUM_COMMITS" -eq 0 ]; then
    echo -e "${GREEN}No commits to rebase${NC}"
    exit 0
fi

# Show current commits
echo ""
echo "Current commits:"
git log --oneline upstream/master..HEAD

# Show rebase options
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "Rebase options:"
echo "  1) Rebase on upstream/master (update branch)"
echo "  2) Interactive rebase (clean up commits)"
echo "  3) Both: rebase on master + interactive cleanup"
echo "  4) Autosquash fixup commits"
echo "  5) Cancel"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
read -p "Choose option: " OPTION

case "$OPTION" in
    1)
        echo ""
        echo "🔄 Rebasing on upstream/master..."
        if git rebase upstream/master; then
            echo -e "${GREEN}✓ Rebase successful${NC}"
            echo ""
            echo "Updated commits:"
            git log --oneline upstream/master..HEAD
        else
            echo -e "${RED}✗ Rebase conflicts detected${NC}"
            echo ""
            echo "Resolve conflicts then:"
            echo "  git add <resolved-files>"
            echo "  git rebase --continue"
            exit 1
        fi
        ;;

    2)
        echo ""
        echo "🔄 Starting interactive rebase..."
        echo ""
        echo "In the editor:"
        echo "  pick   - Keep commit as-is"
        echo "  reword - Change commit message"
        echo "  edit   - Modify commit contents"
        echo "  squash - Merge into previous commit (keep message)"
        echo "  fixup  - Merge into previous commit (discard message)"
        echo "  drop   - Remove commit"
        echo ""
        sleep 2
        git rebase -i upstream/master
        ;;

    3)
        echo ""
        echo "🔄 Rebasing on upstream/master..."
        if ! git rebase upstream/master; then
            echo -e "${RED}✗ Rebase conflicts detected${NC}"
            echo "Resolve conflicts first, then run interactive rebase manually"
            exit 1
        fi

        echo ""
        echo "🔄 Starting interactive rebase..."
        sleep 1
        git rebase -i upstream/master
        ;;

    4)
        echo ""
        echo "🔄 Autosquashing fixup commits..."
        if git rebase -i --autosquash upstream/master; then
            echo -e "${GREEN}✓ Autosquash successful${NC}"
        else
            echo -e "${RED}✗ Rebase conflicts detected${NC}"
            exit 1
        fi
        ;;

    5)
        echo "Cancelled"
        exit 0
        ;;

    *)
        echo -e "${RED}Invalid option${NC}"
        exit 1
        ;;
esac

# After successful rebase
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✓ Rebase complete${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

# Show final commits
echo ""
echo "Final commits:"
git log --oneline upstream/master..HEAD

# Check if build still works
echo ""
read -p "Test build after rebase? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "🔨 Building..."
    if ./Meta/ladybird.py build > /tmp/rebase-build.log 2>&1; then
        echo -e "${GREEN}✓ Build successful${NC}"
    else
        echo -e "${RED}✗ Build failed${NC}"
        tail -20 /tmp/rebase-build.log
        echo "Full log: /tmp/rebase-build.log"
        exit 1
    fi
fi

# Push with force-with-lease
echo ""
echo -e "${YELLOW}⚠ Force push required (history rewritten)${NC}"
read -p "Push to origin? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo "📤 Pushing..."
    git push --force-with-lease origin "$BRANCH"
    echo -e "${GREEN}✓ Pushed to origin${NC}"
else
    echo ""
    echo "To push later:"
    echo "  git push --force-with-lease origin $BRANCH"
fi

echo ""
echo "Rebase complete!"
