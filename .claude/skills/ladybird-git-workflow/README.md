# Ladybird Git Workflow Skill

Comprehensive git workflow guide for Ladybird browser development.

## Contents

### Main Documentation

- **SKILL.md** (960 lines) - Complete git workflow guide
  - Commit message format and rules
  - Branching strategy and naming conventions
  - Pull request process
  - Rebasing and merge strategies
  - Cherry-picking techniques
  - Fork synchronization workflows
  - Git hooks for automation
  - Common operations and troubleshooting

### Examples (examples/)

- **good-commit-messages.txt** - Properly formatted commit examples
- **bad-commit-messages.txt** - Anti-patterns to avoid
- **branch-naming-examples.txt** - Good and bad branch names
- **pr-description-template.md** - Comprehensive PR templates

### Templates (templates/)

- **commit-message-template.txt** - Git commit template (install with git config)
- **pr-template.md** - Basic PR description template
- **security-fix-pr-template.md** - Template for security patches

### Scripts (scripts/)

All scripts are executable and ready to use:

- **prepare_commit.sh** - Pre-commit checks (format, build, tests)
- **check_commit_messages.sh** - Validate commit message format
- **create_pr.sh** - Automated PR creation with gh CLI
- **sync_fork.sh** - Sync fork with upstream Ladybird
- **rebase_feature_branch.sh** - Interactive rebase helper

### References (references/)

- **commit-categories.md** - All commit category prefixes (LibWeb, LibJS, etc.)
- **git-commands-reference.md** - Common git operations quick reference
- **pr-checklist.md** - Comprehensive pre-PR submission checklist
- **merge-strategy.md** - When to merge vs rebase (with examples)

## Quick Start

### Install Commit Template

```bash
git config --local commit.template .claude/skills/ladybird-git-workflow/templates/commit-message-template.txt
```

### Install Git Hooks

```bash
# Pre-commit checks
cp .claude/skills/ladybird-git-workflow/scripts/prepare_commit.sh .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

# Commit message validation
cp .claude/skills/ladybird-git-workflow/scripts/check_commit_messages.sh .git/hooks/commit-msg
chmod +x .git/hooks/commit-msg
```

### Use Helper Scripts

```bash
# Sync fork with upstream
./.claude/skills/ladybird-git-workflow/scripts/sync_fork.sh

# Create PR
./.claude/skills/ladybird-git-workflow/scripts/create_pr.sh

# Rebase feature branch
./.claude/skills/ladybird-git-workflow/scripts/rebase_feature_branch.sh
```

## Key Patterns Documented

### Commit Messages

**Format**: `Category: Brief imperative description`

**Examples**:
- `LibWeb: Add CSS grid support`
- `LibJS: Fix memory leak in running_execution_context()`
- `Sentinel: Improve YARA rule matching performance`
- `docs: Add fingerprinting detection documentation`

**Rules**:
- Imperative mood ("Add" not "Added")
- Max 72 characters on first line
- Category is CamelCase component name
- Description starts with capital letter
- No period at end

### Branch Naming

**Format**: `{type}/{descriptive-name}`

**Examples**:
- `feature/css-grid-support`
- `fix/memory-leak-in-gc`
- `sentinel/fingerprinting-detection`
- `docs/update-testing-guide`

### Workflow

1. **Create feature branch** from latest master
2. **Work and commit** with proper messages
3. **Rebase regularly** on upstream/master
4. **Interactive rebase** to clean commits before PR
5. **Create PR** with detailed description
6. **Address feedback** via amend or fixup commits
7. **Final rebase** before merge

### Fork Synchronization

**Recommended**: Merge from upstream to preserve fork history

```bash
git checkout master
git fetch upstream
git merge upstream/master
git push origin master
```

## Statistics

- **Total files**: 17
- **Main skill**: 960 lines
- **Examples**: 4 files with good/bad patterns
- **Templates**: 3 ready-to-use templates
- **Scripts**: 5 automation scripts
- **References**: 4 comprehensive guides

## Use Cases

This skill helps with:

- ✅ Writing proper commit messages
- ✅ Choosing branch names
- ✅ Creating pull requests
- ✅ Rebasing vs merging decisions
- ✅ Syncing fork with upstream
- ✅ Handling merge conflicts
- ✅ Cleaning up commit history
- ✅ Automating git workflows
- ✅ Following Ladybird conventions
- ✅ Fork-specific workflows

## Challenges Encountered

### 1. Commit Category Analysis

**Challenge**: Determine most common commit categories from git history

**Solution**: Analyzed 100 recent commits with:
```bash
git log --format='%s' -100 | grep -oE '^[^:]+:' | sort | uniq -c
```

**Finding**: Top categories are LibWeb, LibJS, docs, with fork-specific categories like Sentinel and RequestServer+Sentinel for multi-component changes.

### 2. Fork vs Upstream Workflow

**Challenge**: This is a personal fork, workflow differs from upstream

**Solution**: Documented both:
- Upstream workflow (rebase feature branches, squash merge PRs)
- Fork workflow (merge from upstream, longer-lived milestone branches)

### 3. Commit Message Validation

**Challenge**: Automatically validate commit messages

**Solution**: Created `check_commit_messages.sh` hook that:
- Validates format (Category: Description)
- Checks first line length (≤72 chars)
- Detects imperative mood violations
- Warns about common mistakes

### 4. Real-World Examples

**Challenge**: Provide realistic examples from actual commits

**Solution**: Extracted patterns from git log:
- Good commits: `LibWeb: Implement WebGL2's getBufferSubData()`
- Multi-component: `RequestServer+Sentinel: Fix critical performance issues`
- Milestone commits: `Sentinel Milestone 0.4 Phase 1: ML-based malware detection`

### 5. Script Portability

**Challenge**: Scripts need to work across platforms

**Solution**:
- Used bash for compatibility
- Color codes with fallback
- Checks for required tools (gh CLI, ninja)
- Graceful degradation when tools missing

## Integration with Other Skills

This skill complements:

- **ladybird-cpp-patterns**: Coding style in commits
- **libweb-patterns**: Web standards commit messages
- **fuzzing-workflow**: Security fix commit format
- **web-standards-testing**: Test commit patterns

## Future Enhancements

Potential additions:

- [ ] GitHub Actions workflow for commit validation
- [ ] Automated changelog generation
- [ ] Commit message templates for specific categories
- [ ] Integration with issue tracker
- [ ] PR size analysis tool
- [ ] Merge conflict prediction

## References

- **Ladybird Coding Style**: /home/rbsmith4/ladybird/Documentation/CodingStyle.md
- **Git Log Analysis**: Analyzed 100+ commits
- **CLAUDE.md**: /home/rbsmith4/ladybird/CLAUDE.md (commit format section)
- **Fork Status**: Personal learning fork with experimental features
