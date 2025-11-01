---
name: ladybird-git-workflow
description: Git workflow conventions for Ladybird browser development, including commit messages, branching, PRs, and fork synchronization
use-when: Working with git in Ladybird - commits, branches, PRs, rebasing, merging, or syncing with upstream
allowed-tools:
  - Read
  - Write
  - Edit
  - Grep
  - Glob
  - Bash
---

# Ladybird Git Workflow

## Overview

Ladybird browser development follows strict git conventions to maintain a clean, navigable history across a large codebase with many contributors. This skill documents commit message format, branching strategy, PR process, and fork-specific workflows.

### Key Principles

1. **Imperative Commit Messages**: Use command form ("Add feature", not "Added feature")
2. **Category Prefixes**: Every commit starts with component category
3. **Clean History**: Rebase feature branches, avoid merge commits
4. **Atomic Commits**: One logical change per commit
5. **Spec Links**: Reference specifications in commit messages when implementing web standards

## Commit Message Format

### Strict Format

```
Category: Brief imperative description

Optional detailed explanation.
Can span multiple paragraphs.

Specification link if relevant:
https://html.spec.whatwg.org/#the-foo-element
```

### Rules

1. **First Line**: `Category: Brief imperative description`
   - Maximum 72 characters
   - Category is CamelCase or snake_case component name
   - Description starts with capital letter
   - No period at the end
   - Use imperative mood ("Add", "Fix", "Remove", not "Added", "Fixed", "Removed")

2. **Body** (optional):
   - Blank line after first line
   - Wrap at 72 characters
   - Explain WHY, not WHAT (code shows what)
   - Link to specs for web standards implementation
   - Link to issues if fixing bugs

3. **Examples**:
   ```
   LibWeb: Add CSS grid support

   LibJS: Fix memory leak in running_execution_context()

   RequestServer: Integrate phishing detection into ConnectionFromClient

   This adds URLSecurityAnalyzer checks before processing HTTP requests,
   preventing navigation to known phishing domains.

   Sentinel: Improve YARA rule matching performance

   docs: Add fingerprinting detection documentation and test infrastructure
   ```

### Common Categories

Based on git log analysis, most common categories:

- **LibWeb**: HTML/CSS/DOM rendering engine (`Libraries/LibWeb/`)
- **LibJS**: JavaScript engine (`Libraries/LibJS/`)
- **LibGfx**: 2D graphics, fonts, images (`Libraries/LibGfx/`)
- **AK**: Application Kit (data structures, error handling)
- **LibCore**: Event loop, file I/O, OS abstraction
- **RequestServer**: HTTP/HTTPS request handling
- **WebContent**: WebContent process (rendering, JS execution)
- **ImageDecoder**: Image decoding process
- **UI/Qt**: Qt UI implementation
- **UI/AppKit**: macOS AppKit UI
- **Meta**: Build system, tooling, CI/CD
- **Tests**: Test infrastructure changes
- **docs**: Documentation (lowercase)
- **CMake**: Build system configuration

**Fork-Specific Categories**:
- **Sentinel**: Security detection components
- **RequestServer+Sentinel**: Cross-component changes
- **fix**: Bug fixes (lowercase for personal fork)
- **feat**: New features (lowercase for personal fork)

### Category Selection Guide

```
Libraries/LibWeb/HTML/HTMLInputElement.cpp
→ LibWeb: Fix input validation for email type

Libraries/LibJS/Runtime/VM.cpp
→ LibJS: Add support for async generators

Services/RequestServer/ConnectionFromClient.cpp
→ RequestServer: Implement connection pooling

Services/Sentinel/FingerprintingDetector.cpp
→ Sentinel: Add WebGL fingerprinting detection

UI/Qt/MainWindow.cpp
→ UI/Qt: Add dark mode toggle

Meta/CMake/utils.cmake
→ Meta: Upgrade prettier to version 3.6.2

Tests/LibWeb/Text/input/canvas-api.html
→ Tests: Add canvas fingerprinting test cases

Documentation/Testing.md
→ docs: Update testing guide with new patterns

Multiple components:
→ RequestServer+Sentinel: Fix critical performance issues
```

## Branching Strategy

### Branch Naming Conventions

**Feature Branches**:
```
feature/css-grid-support
feature/webgl2-implementation
feature/sentinel-fingerprinting-detection
```

**Bug Fix Branches**:
```
fix/memory-leak-in-gc
fix/crash-on-invalid-css
fix/security-yara-rules
```

**Milestone Branches** (fork-specific):
```
sentinel-milestone-0.4-phase-1
sentinel-phase5-day29-security-fixes
sentinel-phase6-form-anomaly
```

**Experimental/Research**:
```
experiment/tor-per-tab-circuits
research/ml-malware-detection
```

**Release Branches** (upstream):
```
release/1.0.0
hotfix/1.0.1
```

### Branch Lifecycle

1. **Create from latest master**:
   ```bash
   git checkout master
   git pull upstream master
   git checkout -b feature/my-feature
   ```

2. **Regular rebasing** (keep up with master):
   ```bash
   git fetch upstream
   git rebase upstream/master
   ```

3. **Interactive rebase** (clean up commits):
   ```bash
   git rebase -i upstream/master
   ```

4. **Delete after merge**:
   ```bash
   git branch -d feature/my-feature
   git push origin --delete feature/my-feature
   ```

## Pull Request Process

### Pre-PR Checklist

Before creating a PR:

- [ ] All commits follow commit message format
- [ ] Branch rebased on latest master (no merge commits)
- [ ] Code formatted (`ninja -C Build/release check-style`)
- [ ] All tests pass (`./Meta/ladybird.py test`)
- [ ] New tests added for new features
- [ ] Documentation updated if needed
- [ ] No debug/temporary code left in
- [ ] Spec links added for web standards implementation
- [ ] Build succeeds on all platforms (CI will check)

### PR Title Format

Use same format as commit messages:
```
Category: Brief imperative description
```

If PR is a single commit, use that commit's message.
If multiple commits, summarize the overall change.

### PR Description Template

See `templates/pr-template.md` for full template:

```markdown
## Summary

Brief description of what this PR does and why.

## Changes

- List of major changes
- One per line
- Be specific

## Testing

How was this tested?
- [ ] Unit tests added
- [ ] Browser testing with X, Y, Z
- [ ] Verified with sanitizers

## Spec Compliance

For web standards implementation:
- Spec link: https://...
- Sections implemented: X.Y.Z

## Related Issues

Fixes #123
Related to #456
```

### PR Review Process

1. **Create PR**: Push branch and use `gh pr create`
2. **CI Checks**: Wait for all CI checks to pass (build, tests, sanitizers)
3. **Code Review**: Address reviewer feedback
4. **Update PR**: Amend commits or add fixup commits, force push
5. **Final Approval**: Get approval from maintainers
6. **Merge**: Squash if needed, merge to master
7. **Cleanup**: Delete branch after merge

### Updating PR

**Option 1: Amend commits** (clean history):
```bash
# Make changes
git add .
git commit --amend
git push --force-with-lease origin feature/my-feature
```

**Option 2: Fixup commits** (easier for review):
```bash
# Make changes
git add .
git commit --fixup=<commit-hash>
git push origin feature/my-feature

# Before merge, squash fixups
git rebase -i --autosquash upstream/master
git push --force-with-lease origin feature/my-feature
```

## Rebasing Strategy

### When to Rebase

**Always Rebase**:
- Feature branches before creating PR
- Feature branches after PR feedback
- Feature branches when master has advanced

**Never Rebase**:
- master or release branches
- Public branches others depend on
- Commits already pushed to upstream

### Interactive Rebase

Clean up commit history:

```bash
git rebase -i upstream/master
```

Common operations:
- `pick` - Keep commit as-is
- `reword` - Change commit message
- `edit` - Modify commit contents
- `squash` - Merge into previous commit
- `fixup` - Merge into previous, discard message
- `drop` - Remove commit

Example rebase session:
```
pick abc1234 LibWeb: Add canvas API
pick def5678 fix typo
pick ghi9012 LibWeb: Add more canvas tests
pick jkl3456 oops forgot file

# Rewrite to:
pick abc1234 LibWeb: Add canvas API
fixup jkl3456 oops forgot file
fixup def5678 fix typo
pick ghi9012 LibWeb: Add more canvas tests
```

### Resolving Conflicts

```bash
# During rebase
git rebase upstream/master

# If conflicts occur:
# 1. Fix conflicts in files
# 2. Stage resolved files
git add path/to/resolved/file.cpp

# 3. Continue rebase
git rebase --continue

# Or abort if needed
git rebase --abort
```

## Cherry-Picking

### Security Fixes

When backporting security fixes to release branches:

```bash
# On release branch
git checkout release/1.0.0

# Cherry-pick the fix
git cherry-pick <commit-hash>

# Or create new commit
git cherry-pick -x <commit-hash>  # Adds "(cherry picked from...)" note

# Push to release branch
git push origin release/1.0.0
```

### Selective Feature Porting

```bash
# Pick specific commits from feature branch
git cherry-pick feature/my-feature~3  # Third commit from tip
git cherry-pick abc1234..def5678      # Range of commits
```

## Fork Synchronization

### This Fork Setup

```
origin:   https://github.com/quanticsoul4772/ladybird.git (personal fork)
upstream: https://github.com/LadybirdBrowser/ladybird.git (upstream, no push)
```

### Syncing with Upstream

**Option 1: Merge** (preserves fork commits):
```bash
git checkout master
git fetch upstream
git merge upstream/master
git push origin master
```

**Option 2: Rebase** (cleaner but rewrites history):
```bash
git checkout master
git fetch upstream
git rebase upstream/master
git push --force-with-lease origin master
```

**Recommended for this fork**: Use merge to preserve Sentinel development history.

### Handling Upstream API Changes

When upstream changes break fork code:

1. **Fetch upstream**:
   ```bash
   git fetch upstream
   ```

2. **Create fix branch**:
   ```bash
   git checkout -b fix/upstream-api-changes upstream/master
   ```

3. **Fix compilation errors**:
   ```bash
   ./Meta/ladybird.py build
   # Fix errors in fork code
   ```

4. **Commit fixes**:
   ```bash
   git add .
   git commit -m "fix: Update for upstream API changes

   Upstream commit abc1234 changed LibWeb::Node API.
   Updated Sentinel integration points to match new signatures."
   ```

5. **Merge to master**:
   ```bash
   git checkout master
   git merge fix/upstream-api-changes
   ```

### Tracking Upstream Commits

Add notes to merge commits:
```bash
git checkout master
git merge upstream/master -m "Merge upstream/master: prettier upgrades and line length changes

Integrated commits:
- 583164e Meta: Upgrade prettier to version 3.6.2
- 019c529 Meta: Increase the line length enforced by prettier to 120"
```

## Git Hooks for Automation

### Pre-Commit Hook

Automatically check format and build:

```bash
# .git/hooks/pre-commit
#!/bin/bash
set -e

echo "Running pre-commit checks..."

# Check code format
if ! ninja -C Build/release check-style 2>/dev/null; then
    echo "ERROR: Code formatting check failed"
    echo "Run: ninja -C Build/release check-style"
    exit 1
fi

# Optional: Build to catch compilation errors
# Commented out by default (too slow for every commit)
# ./Meta/ladybird.py build

echo "Pre-commit checks passed"
```

### Commit Message Hook

Validate commit message format:

```bash
# .git/hooks/commit-msg
#!/bin/bash

commit_msg_file=$1
commit_msg=$(cat "$commit_msg_file")

# Check format: "Category: Description"
if ! echo "$commit_msg" | grep -qE '^[A-Za-z0-9_/+-]+: [A-Z]'; then
    echo "ERROR: Commit message must follow format:"
    echo "Category: Brief imperative description"
    echo ""
    echo "Examples:"
    echo "  LibWeb: Add CSS grid support"
    echo "  Sentinel: Fix memory leak in detector"
    echo ""
    echo "Your message:"
    echo "$commit_msg"
    exit 1
fi

# Check message length (first line <= 72 chars)
first_line=$(echo "$commit_msg" | head -n1)
if [ ${#first_line} -gt 72 ]; then
    echo "ERROR: First line too long (${#first_line} chars, max 72)"
    echo "$first_line"
    exit 1
fi
```

### Installing Hooks

```bash
# Copy to .git/hooks/
cp scripts/prepare_commit.sh .git/hooks/pre-commit
cp scripts/check_commit_messages.sh .git/hooks/commit-msg
chmod +x .git/hooks/*
```

## Common Git Operations

### Starting New Work

```bash
# Sync with upstream
git checkout master
git fetch upstream
git merge upstream/master  # or rebase
git push origin master

# Create feature branch
git checkout -b feature/my-feature

# Make changes, commit
git add .
git commit -m "Category: Description"
```

### Updating Feature Branch

```bash
# Rebase on latest master
git fetch upstream
git rebase upstream/master

# If conflicts, resolve and continue
git add resolved-file.cpp
git rebase --continue
```

### Creating Pull Request

```bash
# Push branch
git push -u origin feature/my-feature

# Create PR with gh CLI
gh pr create --title "Category: Description" --body "$(cat pr-description.md)"

# Or use GitHub web UI
open "https://github.com/quanticsoul4772/ladybird/compare/master...feature/my-feature"
```

### Viewing History

```bash
# Compact log
git log --oneline -20

# With graph
git log --graph --oneline --all -20

# Files changed
git log --stat -5

# Specific file history
git log --follow -- path/to/file.cpp

# Search commits
git log --grep="fingerprint"
git log --author="rbsmith4"
```

### Undoing Changes

```bash
# Unstage file
git restore --staged file.cpp

# Discard working changes
git restore file.cpp

# Undo last commit (keep changes)
git reset --soft HEAD~1

# Undo last commit (discard changes)
git reset --hard HEAD~1

# Undo pushed commit (create revert)
git revert <commit-hash>
```

### Stashing Work

```bash
# Stash changes
git stash push -m "WIP: canvas fingerprinting"

# List stashes
git stash list

# Apply stash
git stash pop

# Apply specific stash
git stash apply stash@{1}
```

## Merge vs Rebase Strategy

### When to Merge

**Use Merge For**:
- Integrating upstream changes to fork master
- Completing pull requests (GitHub does this)
- Preserving feature development history
- Multi-developer branches

```bash
git checkout master
git merge upstream/master
git merge feature/completed-feature
```

### When to Rebase

**Use Rebase For**:
- Updating feature branches
- Cleaning commit history before PR
- Maintaining linear history
- Single-developer branches

```bash
git checkout feature/my-feature
git rebase upstream/master
git rebase -i upstream/master  # Interactive cleanup
```

### Squash Merging

For PRs with messy history:

```bash
# On GitHub: "Squash and merge" button

# Manual squash merge:
git checkout master
git merge --squash feature/my-feature
git commit -m "Category: Summarized description

All changes from feature branch squashed into single commit."
```

## Handling Merge Conflicts

### Conflict Resolution Process

1. **Identify conflicts**:
   ```bash
   git rebase upstream/master
   # CONFLICT (content): Merge conflict in file.cpp
   ```

2. **Open conflicted files**, look for markers:
   ```cpp
   <<<<<<< HEAD
   // Your changes
   =======
   // Upstream changes
   >>>>>>> upstream/master
   ```

3. **Resolve manually**:
   - Decide which changes to keep
   - Combine if both needed
   - Test that code compiles

4. **Mark resolved**:
   ```bash
   git add file.cpp
   ```

5. **Continue**:
   ```bash
   git rebase --continue
   ```

### Conflict Prevention

- Rebase frequently to catch conflicts early
- Coordinate with team on overlapping work
- Keep changes focused and atomic
- Communicate before large refactorings

## Advanced Git Techniques

### Bisect for Bug Finding

Find commit that introduced a bug:

```bash
# Start bisect
git bisect start

# Mark current as bad
git bisect bad

# Mark known good commit
git bisect good abc1234

# Git checks out middle commit, test it
./Meta/ladybird.py test

# Mark result
git bisect good  # or bad

# Repeat until found
# Git will tell you: "X is the first bad commit"

# Exit bisect
git bisect reset
```

### Worktrees for Parallel Work

Work on multiple branches simultaneously:

```bash
# Create worktree
git worktree add ../ladybird-feature2 feature/feature2

# Now you have:
# /home/rbsmith4/ladybird/ (master)
# /home/rbsmith4/ladybird-feature2/ (feature/feature2)

# Can build/test both simultaneously
cd ../ladybird-feature2
./Meta/ladybird.py build

# Remove when done
git worktree remove ../ladybird-feature2
```

### Reflog for Recovery

Recover lost commits:

```bash
# View all ref changes
git reflog

# Find lost commit
git reflog | grep "commit message"

# Restore
git cherry-pick <commit-hash>
# or
git reset --hard <commit-hash>
```

## Browser Development-Specific Workflows

### Large Codebase Navigation

```bash
# Find all files matching pattern
git ls-files | grep -i fingerprint

# Search commit messages
git log --all --grep="CSS grid"

# Find who changed line
git blame Libraries/LibWeb/HTML/HTMLCanvasElement.cpp

# Show file at specific commit
git show abc1234:Libraries/LibWeb/CSS/Parser.cpp
```

### Multi-Process Architecture Changes

When changes span processes:

```bash
# Example: Fingerprinting detection touches multiple processes
git add Services/Sentinel/FingerprintingDetector.cpp
git add Services/WebContent/PageClient.cpp
git add Libraries/LibWeb/HTML/HTMLCanvasElement.cpp

# Single commit for coherent feature
git commit -m "Sentinel: Add canvas fingerprinting detection

Implemented across three components:
1. FingerprintingDetector (core logic)
2. PageClient (integration point)
3. HTMLCanvasElement (API hook)

This enables real-time detection of canvas fingerprinting attempts."
```

### Spec Implementation Commits

```bash
git commit -m "LibWeb: Implement HTMLInputElement.checkValidity()

Implements constraint validation API from HTML spec:
https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#dom-cva-checkvalidity

Added algorithms:
- checkValidity()
- suffering_from_being_missing()
- suffering_from_type_mismatch()

Tests: Tests/LibWeb/Text/input/constraint-validation.html"
```

## Troubleshooting

### "Detached HEAD" State

```bash
# If accidentally in detached HEAD
git checkout master
# or create branch from current state
git checkout -b feature/my-work
```

### Force Push Safety

```bash
# Always use --force-with-lease, never --force
git push --force-with-lease origin feature/my-branch

# This protects against overwriting others' work
```

### Large File Issues

```bash
# If accidentally committed large file
git rm --cached large-file.bin
echo "large-file.bin" >> .gitignore
git commit --amend

# If already pushed, need to rewrite history (dangerous!)
git filter-branch --tree-filter 'rm -f large-file.bin' HEAD
git push --force-with-lease origin master
```

### Submodule Issues

Ladybird doesn't use submodules, but if they appear:

```bash
# Update all submodules
git submodule update --init --recursive

# Update specific submodule
git submodule update --remote path/to/submodule
```

## Quick Reference

### Essential Commands

```bash
# Daily workflow
git status                           # Check current state
git pull upstream master             # Sync with upstream
git checkout -b feature/name         # New branch
git add file.cpp                     # Stage changes
git commit -m "Category: Desc"       # Commit
git rebase upstream/master           # Update branch
git push -u origin feature/name      # Push to fork

# Code review
git log --oneline -10                # Recent commits
git diff upstream/master             # Changes vs master
git show abc1234                     # View specific commit

# Cleanup
git branch -d feature/name           # Delete local branch
git push origin --delete feat/name   # Delete remote branch
git fetch --prune                    # Remove stale remote refs
```

### Aliases (Add to ~/.gitconfig)

```ini
[alias]
    st = status
    co = checkout
    br = branch
    ci = commit
    cp = cherry-pick
    rb = rebase
    lg = log --graph --oneline --all -20
    last = log -1 HEAD
    unstage = restore --staged
    amend = commit --amend --no-edit
    pushf = push --force-with-lease
    sync = !git fetch upstream && git merge upstream/master
```

## Resources

- **Ladybird Contributing Guide**: Documentation/GettingStartedContributing.md
- **Coding Style**: Documentation/CodingStyle.md (includes commit format)
- **Testing Guide**: Documentation/Testing.md
- **Process Architecture**: Documentation/ProcessArchitecture.md
- **Git Documentation**: https://git-scm.com/doc
- **GitHub CLI**: https://cli.github.com/

## Fork-Specific Notes

This is a **personal learning fork** with experimental features. Git workflow differs slightly:

- **Upstream sync**: Regular merges from LadybirdBrowser/ladybird
- **No upstream PRs**: Fork changes not intended for upstream contribution
- **Milestone branches**: Longer-lived branches for major features (Sentinel milestones)
- **Relaxed commit format**: Lowercase categories (fix, feat, docs) acceptable for personal work
- **Combined commits**: Cross-component changes in single commits (e.g., RequestServer+Sentinel)
- **API compatibility**: Regular fixes when upstream changes break fork code

### Sentinel Development Workflow

Typical milestone development:

```bash
# Start new milestone
git checkout -b sentinel-milestone-0.5-network-privacy

# Work over multiple days
git add Services/Sentinel/TorIntegration.cpp
git commit -m "Sentinel Milestone 0.5 Phase 1: Tor circuit management"

# Regular upstream syncs
git fetch upstream
git merge upstream/master
# Fix any conflicts with fork code

# Complete milestone
git checkout master
git merge sentinel-milestone-0.5-network-privacy
git push origin master

# Tag milestone
git tag v0.5-network-privacy
git push origin v0.5-network-privacy
```

## Related Skills

### Code Quality Before Commit
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Follow C++ patterns before committing. Use spec-matching names in commit messages.
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Ensure code builds before committing. Run Meta/ladybird.py build.

### Pre-Commit Testing
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Run tests before creating PR. Ensure all tests pass with Meta/ladybird.py test.
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Run sanitizer builds before committing. Check for memory issues with Sanitizer preset.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Run fuzzer regression tests on changed code.

### Documentation in Commits
- **[documentation-generation](../documentation-generation/SKILL.md)**: Update documentation when committing API changes. Link to spec sections.
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Include spec URLs in commits for web standards implementation.
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Link to ECMAScript spec in JavaScript feature commits.

### CI Integration
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Ensure CI passes before merging. Fix build failures and test regressions.

### Collaborative Development
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Commit cross-process changes atomically. Document IPC protocol changes.
- **[ipc-security](../ipc-security/SKILL.md)**: Review security implications before committing IPC changes.

### Performance Considerations
- **[browser-performance](../browser-performance/SKILL.md)**: Run benchmarks before committing performance-sensitive changes.
