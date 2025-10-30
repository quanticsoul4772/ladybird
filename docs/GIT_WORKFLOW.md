# Git Workflow for Ladybird Fork

**Fork**: quanticsoul4772/ladybird
**Upstream**: LadybirdBrowser/ladybird
**Policy**: Pull from upstream, NEVER push to upstream

---

## Remote Configuration

```bash
# Your fork (read/write)
origin  https://github.com/quanticsoul4772/ladybird.git

# Upstream official repo (read-only)
upstream  https://github.com/LadybirdBrowser/ladybird.git (fetch)
upstream  no_push (push) ← Disabled to prevent accidents
```

### Verify Configuration
```bash
git remote -v
# Should show:
# origin   https://github.com/quanticsoul4772/ladybird.git (fetch)
# origin   https://github.com/quanticsoul4772/ladybird.git (push)
# upstream https://github.com/LadybirdBrowser/ladybird.git (fetch)
# upstream no_push (push)
```

---

## Standard Workflow

### 1. Sync with Upstream (Before Starting Work)
```bash
# Switch to master
git checkout master

# Fetch latest from upstream
git fetch upstream

# Merge upstream changes into your local master
git merge upstream/master

# Push updated master to your fork
git push origin master
```

### 2. Create Feature Branch
```bash
# Create and switch to new branch
git checkout -b feature-name

# Example for current work:
git checkout -b sentinel-phase5-security-fixes
```

### 3. Make Changes and Commit
```bash
# Stage changes
git add Services/RequestServer/Quarantine.cpp
git add Services/Sentinel/PolicyGraph.cpp
git add Services/Sentinel/SentinelServer.cpp

# Commit with descriptive message
git commit -m "Sentinel Phase 5: Fix critical security vulnerabilities

- Fix SQL injection in PolicyGraph
- Fix arbitrary file read in SentinelServer
- Fix path traversal in Quarantine
- Add quarantine ID validation

🤖 Generated with [Claude Code](https://claude.com/claude-code)

Co-Authored-By: Claude <noreply@anthropic.com>"
```

### 4. Push to Your Fork
```bash
# Push feature branch to origin (your fork)
git push origin feature-name

# Example:
git push origin sentinel-phase5-security-fixes
```

### 5. Create Pull Request (Optional)
If you want to contribute back to upstream:
1. Go to https://github.com/quanticsoul4772/ladybird
2. Click "Compare & pull request"
3. Select base: LadybirdBrowser/ladybird:master
4. Select compare: quanticsoul4772/ladybird:feature-name
5. Fill in PR details and submit

---

## Common Operations

### Update Feature Branch from Upstream
```bash
# While on your feature branch
git fetch upstream
git rebase upstream/master

# Or use merge if you prefer:
git merge upstream/master
```

### Undo Accidental Commit to Master
```bash
# If you committed to master instead of a feature branch
git checkout master
git reset --soft HEAD~1  # Undo commit, keep changes
git checkout -b correct-feature-branch  # Create proper branch
git add .
git commit -m "Your message"
```

### Check What Would Be Pushed
```bash
# See commits that would be pushed
git log origin/feature-name..feature-name

# See diff of changes
git diff origin/feature-name..feature-name
```

---

## Safety Rules

### ✅ ALWAYS DO
- Pull from `upstream` to stay in sync
- Push to `origin` (your fork) only
- Work in feature branches, not master
- Use descriptive branch names
- Write clear commit messages

### ❌ NEVER DO
- Push to `upstream` (protected by `no_push` setting)
- Force push to master: `git push -f origin master` (breaks sync)
- Commit directly to master (use feature branches)
- Delete `.git` directory (loses all history)

---

## Verification Commands

### Check Current Branch
```bash
git branch
# * indicates current branch
```

### Check Remote Tracking
```bash
git branch -vv
# Shows which remote branch each local branch tracks
```

### Check Uncommitted Changes
```bash
git status
# Shows modified/staged/untracked files
```

### Check Commit History
```bash
git log --oneline --graph --all -10
# Shows last 10 commits with branch visualization
```

### Check Remote URL Configuration
```bash
git remote get-url origin     # Should be your fork
git remote get-url upstream   # Should be official repo
git remote get-url --push upstream  # Should be "no_push"
```

---

## Branch Naming Conventions

### Feature Branches
```
sentinel-phase5-security-fixes
sentinel-phase6-ml-integration
feature-tor-bridge-support
feature-ipfs-caching
```

### Bug Fix Branches
```
fix-quarantine-memory-leak
fix-sentinel-crash-on-startup
hotfix-security-cve-2025-1234
```

### Documentation Branches
```
docs-sentinel-setup-guide
docs-api-reference-update
```

---

## Troubleshooting

### "Permission denied" when pushing
- You're trying to push to `upstream` - use `origin` instead
- Check with: `git remote -v`

### "Your branch is behind"
```bash
# Update your local branch
git pull origin your-branch-name
# Or if working from master:
git pull upstream master
```

### "Merge conflict"
```bash
# See conflicting files
git status

# Edit files to resolve conflicts
# Then:
git add resolved-file.cpp
git commit
```

### "Detached HEAD state"
```bash
# Create a branch to save your work
git checkout -b recovery-branch
```

---

## Sentinel Phase 5 Specific Workflow

### Current Feature Branch
```bash
sentinel-phase5-security-fixes
```

### Files Being Modified
```
Services/RequestServer/Quarantine.cpp
Services/Sentinel/PolicyGraph.cpp
Services/Sentinel/SentinelServer.cpp
Services/Sentinel/SentinelServer.h
Services/RequestServer/ConnectionFromClient.cpp
Services/WebContent/ConnectionFromClient.cpp
UI/Qt/SecurityNotificationBanner.cpp
UI/Qt/Tab.cpp
```

### Documentation Being Added
```
docs/SENTINEL_DAY29_MORNING_COMPLETE.md
docs/SENTINEL_DAY29_MERGE_COMPLETE.md
docs/SENTINEL_DAY29_TASK1_IMPLEMENTED.md
docs/SENTINEL_DAY29_TASK2_IMPLEMENTED.md
docs/SENTINEL_DAY29_TASK3_IMPLEMENTED.md
docs/SENTINEL_DAY29_TASK4_IMPLEMENTED.md
docs/GIT_WORKFLOW.md
```

### Commit Strategy
Create logical commits grouping related changes:

1. **Commit 1: Security fixes**
   ```bash
   git add Services/RequestServer/Quarantine.cpp \
           Services/Sentinel/PolicyGraph.cpp \
           Services/Sentinel/SentinelServer.cpp
   git commit -m "Sentinel Phase 5 Day 29: Fix 4 critical security vulnerabilities"
   ```

2. **Commit 2: Documentation**
   ```bash
   git add docs/SENTINEL_DAY29_*.md \
           docs/GIT_WORKFLOW.md
   git commit -m "Sentinel Phase 5: Add Day 29 implementation documentation"
   ```

3. **Commit 3: Tests** (when implemented)
   ```bash
   git add Tests/Sentinel/TestQuarantinePathTraversal.cpp \
           Tests/Sentinel/TestPolicyGraphSQLInjection.cpp
   git commit -m "Sentinel Phase 5: Add security vulnerability tests"
   ```

---

## Quick Reference Card

| Task | Command |
|------|---------|
| Sync with upstream | `git fetch upstream && git merge upstream/master` |
| Create feature branch | `git checkout -b branch-name` |
| Check status | `git status` |
| Stage changes | `git add file.cpp` |
| Commit | `git commit -m "message"` |
| Push to fork | `git push origin branch-name` |
| Switch branch | `git checkout branch-name` |
| List branches | `git branch -a` |
| View remotes | `git remote -v` |
| View log | `git log --oneline -10` |

---

## References

- **Official Git Documentation**: https://git-scm.com/doc
- **GitHub Fork Workflow**: https://docs.github.com/en/get-started/quickstart/fork-a-repo
- **Ladybird Contribution Guide**: https://github.com/LadybirdBrowser/ladybird/blob/master/CONTRIBUTING.md

---

**Document Version**: 1.0
**Last Updated**: 2025-10-30
**Applies To**: Sentinel Phase 5 and all future development
