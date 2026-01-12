# Merge vs Rebase Strategy

Guide for choosing between merge and rebase in Ladybird development.

## Quick Decision Tree

```
Are you updating a feature branch?
└─ YES → Rebase

Are you integrating upstream changes to your fork's master?
└─ YES → Merge (or Rebase if you prefer linear history)

Are you completing a pull request?
└─ YES → Let GitHub handle it (usually squash merge)

Are you on a shared branch with other developers?
└─ YES → Merge (don't rewrite shared history)

Are you cleaning up commits before review?
└─ YES → Interactive rebase

Default for personal work?
└─ Rebase (keeps history clean)
```

## When to Use Merge

### 1. Integrating Upstream to Fork Master

**Use**: Merge
**Reason**: Preserves fork development history

```bash
git checkout master
git fetch upstream
git merge upstream/master
git push origin master
```

**Why Merge**:
- ✅ Preserves all fork commits
- ✅ Shows when upstream was integrated
- ✅ Clear merge points in history
- ✅ Easy to see fork-specific changes
- ❌ Non-linear history (but that's OK for forks)

**Example History**:
```
*   Merge upstream/master: prettier upgrades
|\
| * (upstream) Meta: Upgrade prettier
| * (upstream) LibWeb: Add CSS grid
* | Sentinel: Add fingerprinting detection
* | Sentinel: Improve YARA rules
|/
*   Previous merge point
```

### 2. Completing Feature Development

**Use**: Merge (with --no-ff)
**Reason**: Preserves feature development history

```bash
git checkout master
git merge --no-ff feature/css-grid
```

**Why Merge**:
- ✅ Shows feature was developed separately
- ✅ Easy to revert entire feature if needed
- ✅ Clear feature boundaries
- ❌ Clutters history if overused

**When to Squash Instead**:
- Feature has messy commit history
- Commits aren't individually meaningful
- Want one commit per feature

### 3. Shared Branches

**Use**: Merge
**Reason**: Don't rewrite history others depend on

```bash
git checkout shared-branch
git merge origin/shared-branch
```

**Why Merge**:
- ✅ Safe for multiple developers
- ✅ No history rewriting
- ✅ No force pushes needed
- ❌ Can create merge commit noise

**Never Rebase**:
- master branch
- release branches
- Any branch others are working on

### 4. Release Branches

**Use**: Merge (or Cherry-Pick)
**Reason**: Preserve release history

```bash
git checkout release/1.0.0
git merge --no-ff hotfix/security-fix
```

**Why Merge**:
- ✅ Clear release history
- ✅ Easy to see what was included
- ✅ Safe for production branches

## When to Use Rebase

### 1. Updating Feature Branch

**Use**: Rebase
**Reason**: Keep linear history, avoid merge commits

```bash
git checkout feature/my-feature
git fetch upstream
git rebase upstream/master
```

**Why Rebase**:
- ✅ Linear history
- ✅ No merge commits
- ✅ Commits appear on top of master
- ✅ Easier to review
- ✅ Cleaner git log
- ❌ Rewrites history (requires force push)

**Example History (Before)**:
```
      C---D  feature/my-feature
     /
A---B---E---F  master
```

**Example History (After Rebase)**:
```
              C'--D'  feature/my-feature
             /
A---B---E---F  master
```

### 2. Cleaning Up Commits

**Use**: Interactive Rebase
**Reason**: Polish commits before review

```bash
git rebase -i upstream/master
```

**Operations**:
- `pick` - Keep commit
- `reword` - Change commit message
- `edit` - Modify commit contents
- `squash` - Merge into previous (keep message)
- `fixup` - Merge into previous (discard message)
- `drop` - Remove commit

**Example**:
```bash
# Before: messy history
git log --oneline
abc1234 LibWeb: Add canvas API
def5678 fix typo
ghi9012 forgot file
jkl3456 Add tests

# Interactive rebase
git rebase -i HEAD~4

# In editor, change to:
pick abc1234 LibWeb: Add canvas API
fixup ghi9012 forgot file
fixup def5678 fix typo
pick jkl3456 LibWeb: Add canvas tests

# After: clean history
git log --oneline
abc1234 LibWeb: Add canvas API
jkl3456 LibWeb: Add canvas tests
```

### 3. Squashing Multiple Commits

**Use**: Rebase or Reset+Commit
**Reason**: Combine related commits

```bash
# Option 1: Interactive rebase
git rebase -i HEAD~3

# Option 2: Reset and re-commit
git reset --soft HEAD~3
git commit -m "LibWeb: Add complete CSS grid implementation"
```

**Why Squash**:
- ✅ One logical change = one commit
- ✅ Easier to review
- ✅ Easier to revert if needed
- ✅ Cleaner history

### 4. Autosquashing Fixup Commits

**Use**: Rebase with --autosquash
**Reason**: Automatically combine fixup commits

```bash
# During development
git commit -m "LibWeb: Add canvas API"
# ... later find bug ...
git commit --fixup=abc1234
git commit --fixup=abc1234

# Before PR, squash all fixups
git rebase -i --autosquash upstream/master
```

**Why Autosquash**:
- ✅ Easy to fix commits during development
- ✅ Automatic cleanup before PR
- ✅ No manual rebase editing needed

## Comparison Table

| Scenario | Merge | Rebase | Why |
|----------|-------|--------|-----|
| Update feature branch | ❌ | ✅ | Linear history, easier review |
| Sync fork master | ✅ | ⚠️ | Preserve fork history |
| Shared branch | ✅ | ❌ | Don't rewrite shared history |
| Clean up commits | ❌ | ✅ | Polish before review |
| Integrate upstream | ✅ | ⚠️ | Show integration points |
| Complete PR | ⚠️ | ⚠️ | Let GitHub handle it |
| Release branch | ✅ | ❌ | Preserve release history |
| Fix merge conflicts | ✅ | ⚠️ | Simpler to resolve |

Legend:
- ✅ Recommended
- ⚠️ Use with caution
- ❌ Avoid

## Ladybird Project Preferences

### Upstream Ladybird (Main Project)

**Feature Branches**:
- Rebase on master before PR
- Interactive rebase to clean up commits
- Squash merge in GitHub (usually)

**Master Branch**:
- Never rebase (public branch)
- Merge or squash merge from PRs

**Release Branches**:
- Merge only (preserve history)
- Cherry-pick for backports

### This Fork (Personal Learning Fork)

**Feature Branches**:
- Rebase on upstream master regularly
- Interactive rebase for cleanup
- Keep commits atomic and well-formatted

**Fork Master**:
- Merge from upstream (preserve fork history)
- Can rebase if you prefer linear history (your fork, your choice)

**Milestone Branches**:
- Merge when complete (preserve milestone history)
- Rebase during development to stay current

## Common Scenarios

### Scenario 1: Feature Branch Falling Behind

**Problem**: Your feature branch is 20 commits behind master

**Solution**: Rebase

```bash
git checkout feature/my-feature
git fetch upstream
git rebase upstream/master
# Resolve conflicts if any
git push --force-with-lease origin feature/my-feature
```

**Why**: Linear history, up-to-date with master, easier to review

### Scenario 2: Messy Commit History

**Problem**: Your branch has 15 commits, many are "fix typo" or "oops"

**Solution**: Interactive rebase

```bash
git rebase -i upstream/master
# Squash/fixup commits in editor
git push --force-with-lease origin feature/my-feature
```

**Why**: Clean, reviewable history with atomic commits

### Scenario 3: Fork Master Out of Date

**Problem**: Upstream has 50 new commits

**Solution**: Merge (or rebase if you prefer)

```bash
# Option 1: Merge (recommended for forks)
git checkout master
git fetch upstream
git merge upstream/master
git push origin master

# Option 2: Rebase (if you want linear history)
git checkout master
git fetch upstream
git rebase upstream/master
git push --force-with-lease origin master  # Careful!
```

**Why**: Preserve fork development context (merge) or maintain linear history (rebase)

### Scenario 4: Upstream Broke Fork Code

**Problem**: Upstream changes broke Sentinel compilation

**Solution**: Merge, then fix

```bash
git checkout master
git fetch upstream
git merge upstream/master
# Compilation errors occur
git checkout -b fix/upstream-api-changes
# Fix errors
git commit -m "fix: Update for upstream API changes"
git checkout master
git merge fix/upstream-api-changes
git push origin master
```

**Why**: Preserve history of what broke and how it was fixed

### Scenario 5: PR Review Feedback

**Problem**: Reviewer wants changes to commits

**Solution**: Amend or interactive rebase

```bash
# Option 1: Amend last commit
git add fixed-file.cpp
git commit --amend --no-edit
git push --force-with-lease origin feature/my-feature

# Option 2: Fixup specific commit
git add fixed-file.cpp
git commit --fixup=abc1234
git rebase -i --autosquash upstream/master
git push --force-with-lease origin feature/my-feature
```

**Why**: Maintain clean commit history as requested

### Scenario 6: Multiple Features in Progress

**Problem**: Working on 3 features simultaneously

**Solution**: Separate branches, rebase each independently

```bash
# Feature 1
git checkout feature/css-grid
git rebase upstream/master

# Feature 2
git checkout feature/canvas-api
git rebase upstream/master

# Feature 3
git checkout feature/webgl2
git rebase upstream/master
```

**Why**: Each feature stays current with master independently

## Merge Conflict Resolution

### During Merge

```bash
git merge upstream/master
# CONFLICT in file.cpp

# Resolve conflicts in editor
# Look for <<<<<<, ======, >>>>>> markers

git add file.cpp
git merge --continue
```

### During Rebase

```bash
git rebase upstream/master
# CONFLICT in file.cpp

# Resolve conflicts in editor

git add file.cpp
git rebase --continue

# If multiple commits conflict, repeat for each
```

**Rebase Conflicts**: May occur multiple times (once per commit)
**Merge Conflicts**: Occur once (for all changes)

**Tip**: If rebase conflicts are too painful, abort and use merge instead:
```bash
git rebase --abort
git merge upstream/master
```

## Force Push Safety

### Always Use --force-with-lease

```bash
# ❌ Dangerous
git push --force origin feature/name

# ✅ Safe
git push --force-with-lease origin feature/name
```

**Why**: `--force-with-lease` checks that remote hasn't changed before force pushing.

### When Force Push is Needed

After:
- Rebase
- Interactive rebase
- Commit amending
- History rewriting

### When Force Push is NEVER Allowed

Never force push to:
- master branch
- release branches
- Shared branches
- Upstream (you can't anyway, no push access)

## Best Practices Summary

### Do ✅

- Rebase feature branches before PR
- Use interactive rebase to clean commits
- Merge from upstream to fork master
- Keep commits atomic
- Test after every rebase/merge
- Use `--force-with-lease` for force push
- Communicate when rewriting shared history

### Don't ❌

- Rebase public/shared branches
- Force push to master
- Merge feature branches during development (rebase instead)
- Create empty merge commits unnecessarily
- Rewrite history after PR approval
- Rebase when merge conflicts are complex

### Ladybird-Specific

- Feature branches: Rebase on master
- Fork master: Merge from upstream
- Before PR: Interactive rebase for cleanup
- After review: Amend commits or fixup+rebase
- Never: Rebase master or release branches

## Tools and Commands

### View History

```bash
# See graph of commits
git log --graph --oneline --all

# See merge commits only
git log --merges

# See non-merge commits only
git log --no-merges
```

### Check Branch Status

```bash
# Commits ahead/behind
git rev-list --count HEAD..upstream/master  # Behind
git rev-list --count upstream/master..HEAD  # Ahead

# Divergence check
git log --oneline HEAD..upstream/master  # Commits behind
git log --oneline upstream/master..HEAD  # Commits ahead
```

### Find Merge Base

```bash
# Find common ancestor
git merge-base feature/name master

# Show commits since divergence
git log $(git merge-base feature/name master)..feature/name
```

## Further Reading

- **Git Documentation**: https://git-scm.com/book/en/v2/Git-Branching-Rebasing
- **Atlassian Guide**: https://www.atlassian.com/git/tutorials/merging-vs-rebasing
- **Ladybird Contributing**: Documentation/GettingStartedContributing.md
- **Git Commands**: .claude/skills/ladybird-git-workflow/references/git-commands-reference.md
