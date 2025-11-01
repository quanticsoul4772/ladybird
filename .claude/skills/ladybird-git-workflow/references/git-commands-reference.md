# Git Commands Reference

Quick reference for common git operations in Ladybird development.

## Daily Workflow

### Starting New Work

```bash
# Update master
git checkout master
git fetch upstream
git merge upstream/master
git push origin master

# Create feature branch
git checkout -b feature/my-feature

# Or fix branch
git checkout -b fix/memory-leak
```

### Making Changes

```bash
# Check status
git status

# View changes
git diff
git diff --cached  # Staged changes

# Stage files
git add file.cpp
git add Libraries/LibWeb/  # Whole directory
git add -p  # Interactive staging

# Unstage files
git restore --staged file.cpp

# Discard working changes
git restore file.cpp
```

### Committing

```bash
# Commit with message
git commit -m "Category: Description"

# Commit with editor (for body)
git commit

# Amend last commit
git commit --amend
git commit --amend --no-edit  # Keep message

# Fixup commit for later squashing
git commit --fixup=abc1234
```

### Viewing History

```bash
# Recent commits
git log --oneline -20

# With graph
git log --graph --oneline --all

# Specific file history
git log -- path/to/file.cpp
git log --follow -- path/to/file.cpp  # Follow renames

# Search commits
git log --grep="fingerprint"
git log --author="rbsmith4"
git log --since="2 weeks ago"

# Show specific commit
git show abc1234
git show HEAD~3  # 3 commits back
```

## Branch Management

### Creating Branches

```bash
# Create and switch
git checkout -b feature/name

# Create from specific commit
git checkout -b fix/bug abc1234

# Create from remote branch
git checkout -b local-name origin/remote-name
```

### Switching Branches

```bash
# Switch to existing branch
git checkout master
git checkout feature/name

# Switch to previous branch
git checkout -
```

### Listing Branches

```bash
# Local branches
git branch

# All branches (including remote)
git branch -a

# Branches with last commit
git branch -v

# Merged branches
git branch --merged
git branch --no-merged
```

### Deleting Branches

```bash
# Delete local branch (safe)
git branch -d feature/name

# Force delete (even if not merged)
git branch -D feature/name

# Delete remote branch
git push origin --delete feature/name
```

## Syncing with Remote

### Fetching

```bash
# Fetch from all remotes
git fetch --all

# Fetch from specific remote
git fetch upstream
git fetch origin

# Fetch and prune deleted remote branches
git fetch --prune
```

### Pulling

```bash
# Pull (fetch + merge)
git pull upstream master

# Pull with rebase
git pull --rebase upstream master
```

### Pushing

```bash
# Push to origin
git push origin feature/name

# Push and set upstream
git push -u origin feature/name

# Force push (dangerous!)
git push --force origin feature/name

# Force push (safer)
git push --force-with-lease origin feature/name
```

## Rebasing

### Basic Rebase

```bash
# Rebase on master
git rebase upstream/master

# Continue after resolving conflicts
git add resolved-file.cpp
git rebase --continue

# Skip current commit
git rebase --skip

# Abort rebase
git rebase --abort
```

### Interactive Rebase

```bash
# Rebase last 5 commits
git rebase -i HEAD~5

# Rebase all commits since master
git rebase -i upstream/master

# Autosquash fixup commits
git rebase -i --autosquash upstream/master
```

## Merging

### Basic Merge

```bash
# Merge branch into current
git merge feature/name

# Merge with no fast-forward (create merge commit)
git merge --no-ff feature/name

# Merge with squash
git merge --squash feature/name
```

### Resolving Conflicts

```bash
# See conflicted files
git status

# After resolving conflicts
git add resolved-file.cpp
git merge --continue

# Abort merge
git merge --abort

# Use theirs/ours for conflicts
git checkout --theirs file.cpp
git checkout --ours file.cpp
```

## Stashing

### Basic Stashing

```bash
# Stash changes
git stash
git stash push -m "WIP: feature description"

# Stash including untracked files
git stash -u

# List stashes
git stash list

# Apply most recent stash
git stash pop

# Apply without removing from stash
git stash apply

# Apply specific stash
git stash apply stash@{2}

# Drop stash
git stash drop stash@{1}

# Clear all stashes
git stash clear
```

## Cherry-Picking

```bash
# Cherry-pick single commit
git cherry-pick abc1234

# Cherry-pick range
git cherry-pick abc1234..def5678

# Cherry-pick without committing
git cherry-pick -n abc1234

# Cherry-pick with note
git cherry-pick -x abc1234  # Adds "(cherry picked from...)"

# Abort cherry-pick
git cherry-pick --abort
```

## Undoing Changes

### Unstage Changes

```bash
# Unstage file
git restore --staged file.cpp

# Unstage all
git restore --staged .
```

### Discard Working Changes

```bash
# Discard changes to file
git restore file.cpp

# Discard all changes
git restore .

# Discard untracked files
git clean -fd
git clean -fdn  # Dry run first
```

### Undo Commits

```bash
# Undo last commit (keep changes)
git reset --soft HEAD~1

# Undo last commit (discard changes)
git reset --hard HEAD~1

# Undo commits back to specific commit
git reset --hard abc1234

# Revert commit (create new commit)
git revert abc1234
git revert HEAD
```

## Advanced Operations

### Bisect (Find Bug-Introducing Commit)

```bash
# Start bisect
git bisect start

# Mark current as bad
git bisect bad

# Mark known good commit
git bisect good abc1234

# Git checks out middle commit
# Test it, then mark:
git bisect good  # or
git bisect bad

# Repeat until found
# Git will output: "X is the first bad commit"

# End bisect
git bisect reset
```

### Reflog (Recover Lost Commits)

```bash
# View reflog
git reflog

# Restore from reflog
git reset --hard HEAD@{5}
git cherry-pick abc1234
```

### Worktrees (Multiple Working Directories)

```bash
# Create worktree
git worktree add ../ladybird-feature2 feature/feature2

# List worktrees
git worktree list

# Remove worktree
git worktree remove ../ladybird-feature2
```

### Submodules (Not Used in Ladybird)

```bash
# If submodules are added in future:
git submodule update --init --recursive
git submodule update --remote
```

## Blame and History

### Blame (Find Who Changed Line)

```bash
# Show who changed each line
git blame file.cpp

# Blame with commit messages
git blame -s file.cpp

# Blame specific line range
git blame -L 10,20 file.cpp

# Follow renames
git blame -C file.cpp
```

### Show File at Commit

```bash
# Show file contents at commit
git show abc1234:path/to/file.cpp

# Show file at previous commit
git show HEAD~3:file.cpp
```

### Diff Between Commits

```bash
# Diff between commits
git diff abc1234..def5678

# Diff between branches
git diff master..feature/name

# Diff specific file
git diff abc1234 def5678 -- file.cpp

# Diff stats only
git diff --stat master..feature/name
```

## Tags

### Creating Tags

```bash
# Lightweight tag
git tag v1.0.0

# Annotated tag
git tag -a v1.0.0 -m "Release 1.0.0"

# Tag specific commit
git tag v1.0.0 abc1234
```

### Listing and Viewing Tags

```bash
# List tags
git tag

# List tags matching pattern
git tag -l "v1.*"

# Show tag details
git show v1.0.0
```

### Pushing Tags

```bash
# Push specific tag
git push origin v1.0.0

# Push all tags
git push --tags
```

### Deleting Tags

```bash
# Delete local tag
git tag -d v1.0.0

# Delete remote tag
git push origin --delete v1.0.0
```

## Configuration

### User Configuration

```bash
# Set name and email
git config --global user.name "Your Name"
git config --global user.email "your@email.com"

# Set local (repo-specific) name
git config --local user.name "Your Name"

# View configuration
git config --list
git config user.name
```

### Aliases

```bash
# Set aliases
git config --global alias.st status
git config --global alias.co checkout
git config --global alias.br branch
git config --global alias.ci commit
git config --global alias.lg "log --graph --oneline --all"

# Use aliases
git st
git lg
```

### Commit Template

```bash
# Set commit template
git config --local commit.template .claude/skills/ladybird-git-workflow/templates/commit-message-template.txt
```

### Editor

```bash
# Set default editor
git config --global core.editor vim
git config --global core.editor "code --wait"  # VS Code
```

## Remote Management

### Viewing Remotes

```bash
# List remotes
git remote -v

# Show remote details
git remote show origin
git remote show upstream
```

### Adding Remotes

```bash
# Add remote
git remote add upstream https://github.com/LadybirdBrowser/ladybird.git

# Add fork
git remote add origin https://github.com/quanticsoul4772/ladybird.git
```

### Updating Remotes

```bash
# Change remote URL
git remote set-url origin https://new-url.git

# Rename remote
git remote rename origin fork

# Remove remote
git remote remove upstream
```

## Searching

### Grep in Repository

```bash
# Search in working directory
git grep "pattern"

# Search in specific commit
git grep "pattern" abc1234

# Search with context
git grep -C 3 "pattern"

# Search for regex
git grep -E "regex.*pattern"
```

### Find Files

```bash
# List all files
git ls-files

# Find files matching pattern
git ls-files | grep fingerprint

# Show files in commit
git show abc1234 --name-only
```

## Performance and Maintenance

### Repository Maintenance

```bash
# Garbage collection
git gc

# Aggressive garbage collection
git gc --aggressive

# Prune unreachable objects
git prune

# Verify repository integrity
git fsck
```

### Repository Statistics

```bash
# Repository size
du -sh .git/

# Count objects
git count-objects -vH

# Top contributors
git shortlog -sn

# Commit count
git rev-list --count HEAD
```

## Ladybird-Specific Patterns

### Sync Fork with Upstream

```bash
git checkout master
git fetch upstream
git merge upstream/master
git push origin master
```

### Create Feature Branch

```bash
git checkout master
git pull upstream master
git checkout -b feature/css-grid-support
# Work...
git commit -m "LibWeb: Add CSS grid support"
git push -u origin feature/css-grid-support
```

### Rebase Feature Branch

```bash
git checkout feature/my-feature
git fetch upstream
git rebase upstream/master
git push --force-with-lease origin feature/my-feature
```

### Clean Up Commits Before PR

```bash
git rebase -i upstream/master
# Interactive rebase in editor
git push --force-with-lease origin feature/my-feature
```

### Squash Multiple Commits

```bash
# Squash last 3 commits
git reset --soft HEAD~3
git commit -m "LibWeb: Add complete CSS grid implementation"
git push --force-with-lease origin feature/my-feature
```

## Emergency Commands

### Oh No, I Messed Up!

```bash
# Undo uncommitted changes
git restore .

# Undo last commit (keep changes)
git reset --soft HEAD~1

# Undo everything back to upstream
git reset --hard upstream/master

# Recover deleted branch (find in reflog)
git reflog
git checkout -b recovered-branch abc1234

# Fix pushed bad commit
git revert abc1234
git push origin master
```

## Quick Reference Card

```bash
# Status and changes
git status                    # Current state
git diff                      # Working changes
git log --oneline -10         # Recent commits

# Branching
git checkout -b name          # New branch
git checkout master           # Switch branch
git branch -d name            # Delete branch

# Committing
git add file.cpp              # Stage file
git commit -m "Cat: Desc"     # Commit
git commit --amend            # Fix last commit

# Syncing
git fetch upstream            # Fetch upstream
git merge upstream/master     # Merge changes
git rebase upstream/master    # Rebase on upstream
git push origin branch        # Push to fork

# Undoing
git restore file.cpp          # Discard changes
git reset --soft HEAD~1       # Undo commit
git revert abc1234            # Revert commit

# Help
git help command              # Command help
git command --help            # Same as above
```
