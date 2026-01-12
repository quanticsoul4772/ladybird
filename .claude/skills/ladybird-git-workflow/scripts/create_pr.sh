#!/bin/bash
# Create pull request with proper formatting
# Usage: ./scripts/create_pr.sh [--draft]

set -e

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Check if gh CLI is installed
if ! command -v gh &> /dev/null; then
    echo -e "${RED}ERROR: GitHub CLI (gh) not installed${NC}"
    echo "Install from: https://cli.github.com/"
    exit 1
fi

# Check if authenticated
if ! gh auth status &> /dev/null; then
    echo -e "${RED}ERROR: Not authenticated with GitHub${NC}"
    echo "Run: gh auth login"
    exit 1
fi

# Get current branch
current_branch=$(git branch --show-current)
if [ "$current_branch" = "master" ]; then
    echo -e "${RED}ERROR: Cannot create PR from master branch${NC}"
    exit 1
fi

echo -e "${BLUE}Creating PR from branch: $current_branch${NC}"
echo ""

# Check if branch is pushed to remote
if ! git ls-remote --heads origin "$current_branch" | grep -q "$current_branch"; then
    echo "Branch not pushed to origin. Pushing..."
    git push -u origin "$current_branch"
fi

# Check if branch is up to date with master
echo "Checking if branch is up to date with master..."
git fetch upstream master
BEHIND=$(git rev-list --count HEAD..upstream/master)
if [ "$BEHIND" -gt 0 ]; then
    echo -e "${YELLOW}WARNING: Branch is $BEHIND commits behind upstream/master${NC}"
    echo "Consider rebasing: git rebase upstream/master"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Get PR title (use first commit message if single commit, or ask)
NUM_COMMITS=$(git log --oneline upstream/master..HEAD | wc -l)
if [ "$NUM_COMMITS" -eq 1 ]; then
    DEFAULT_TITLE=$(git log --oneline upstream/master..HEAD | head -n1 | cut -d' ' -f2-)
else
    DEFAULT_TITLE=""
fi

echo ""
echo "Number of commits: $NUM_COMMITS"
if [ -n "$DEFAULT_TITLE" ]; then
    echo "Suggested title: $DEFAULT_TITLE"
fi
echo ""

read -p "PR Title (leave empty to use suggestion): " PR_TITLE
if [ -z "$PR_TITLE" ] && [ -n "$DEFAULT_TITLE" ]; then
    PR_TITLE="$DEFAULT_TITLE"
fi

if [ -z "$PR_TITLE" ]; then
    echo -e "${RED}ERROR: PR title required${NC}"
    exit 1
fi

# Validate title format
if ! echo "$PR_TITLE" | grep -qE '^[A-Za-z0-9_/+-]+: [A-Z]'; then
    echo -e "${YELLOW}WARNING: PR title doesn't follow format 'Category: Description'${NC}"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

# Generate PR body
echo ""
echo "Generating PR description..."

# Get list of changed files
CHANGED_FILES=$(git diff --name-only upstream/master..HEAD | head -10)
if [ $(git diff --name-only upstream/master..HEAD | wc -l) -gt 10 ]; then
    CHANGED_FILES="$CHANGED_FILES"$'\n'"... and more"
fi

# Create PR body
PR_BODY=$(cat <<EOF
## Summary

[Brief description of what this PR accomplishes]

## Changes

$(git log --oneline upstream/master..HEAD | sed 's/^/- /')

## Files Changed

\`\`\`
$CHANGED_FILES
\`\`\`

## Testing

- [ ] Unit tests added/updated
- [ ] Browser testing performed
- [ ] All existing tests pass
- [ ] Verified with sanitizers

## Related Issues

Fixes #
EOF
)

# Save PR body to temp file for editing
TEMP_FILE=$(mktemp)
echo "$PR_BODY" > "$TEMP_FILE"

# Open editor to customize PR body
EDITOR="${EDITOR:-vim}"
echo "Opening editor to customize PR description..."
sleep 1
$EDITOR "$TEMP_FILE"

# Read final PR body
FINAL_PR_BODY=$(cat "$TEMP_FILE")
rm "$TEMP_FILE"

# Check for draft flag
DRAFT_FLAG=""
if [ "$1" = "--draft" ]; then
    DRAFT_FLAG="--draft"
    echo -e "${YELLOW}Creating draft PR${NC}"
fi

# Create PR
echo ""
echo "Creating pull request..."
echo "  Title: $PR_TITLE"
echo "  Base: master"
echo "  Head: $current_branch"
echo ""

gh pr create \
    --title "$PR_TITLE" \
    --body "$FINAL_PR_BODY" \
    --base master \
    --head "$current_branch" \
    $DRAFT_FLAG

echo ""
echo -e "${GREEN}✓ Pull request created successfully!${NC}"
echo ""
echo "View PR: $(gh pr view --web)"
echo ""
echo "Next steps:"
echo "  - Wait for CI checks to complete"
echo "  - Address any review feedback"
echo "  - Update PR with: git commit --amend && git push --force-with-lease"
