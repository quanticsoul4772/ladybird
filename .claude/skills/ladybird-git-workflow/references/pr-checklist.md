# Pull Request Checklist

Complete checklist to ensure high-quality pull requests.

## Before Creating PR

### Code Quality

- [ ] **Code compiles without errors**
  ```bash
  ./Meta/ladybird.py build
  ```

- [ ] **All tests pass**
  ```bash
  ./Meta/ladybird.py test
  ```

- [ ] **Code formatting is correct**
  ```bash
  ninja -C Build/release check-style
  ```

- [ ] **No sanitizer errors**
  ```bash
  cmake --preset Sanitizers
  cmake --build Build/sanitizers
  ctest --preset Sanitizers
  ```

- [ ] **No compiler warnings** (at least no new ones)

- [ ] **Code follows Ladybird coding style**
  - CamelCase for classes/structs
  - snake_case for functions/variables
  - `m_` prefix for member variables
  - East const style
  - Comments explain WHY, not WHAT

### Commits

- [ ] **All commits follow message format**
  - Format: `Category: Brief imperative description`
  - First line ≤ 72 characters
  - Imperative mood ("Add" not "Added")
  - No period at end

- [ ] **Commits are atomic** (one logical change per commit)

- [ ] **Commit history is clean**
  - No merge commits (use rebase)
  - No fixup/squash commits (use `git rebase -i --autosquash`)
  - No "WIP" or "debug" commits

- [ ] **Branch rebased on latest master**
  ```bash
  git fetch upstream
  git rebase upstream/master
  ```

### Testing

- [ ] **Unit tests added for new functionality**
  - C++ tests in `Services/*/Test*.cpp` or `Tests/`
  - LibWeb text tests in `Tests/LibWeb/Text/input/`

- [ ] **Browser tested manually**
  - Test with real websites when applicable
  - Test edge cases and error conditions
  - Test on different platforms if possible

- [ ] **Regression tests added for bug fixes**

- [ ] **Existing tests updated if behavior changed**

### Documentation

- [ ] **Spec links added for web standards**
  - Algorithm comments link to spec sections
  - Function names match spec exactly

- [ ] **Documentation updated**
  - `Documentation/` files if APIs changed
  - `CLAUDE.md` if workflow changed (fork)
  - `docs/` for fork-specific features

- [ ] **Comments added for complex code**
  - Explain WHY, not WHAT
  - Reference spec sections
  - Mark FIXME for incomplete implementations

### Code Review Self-Check

- [ ] **No debug code left in**
  - No `console.log()` or excessive `dbgln()`
  - No `TODO: REMOVE` comments
  - No commented-out code blocks

- [ ] **No sensitive data**
  - No hardcoded passwords/keys
  - No API tokens
  - No private URLs

- [ ] **No large files** (>1MB)
  - Consider Git LFS if needed
  - Binary files should be in separate commits

- [ ] **Error handling is robust**
  - Use `WebIDL::ExceptionOr<T>` for web APIs
  - Use `ErrorOr<T>` for internal code
  - Handle OOM conditions with `TRY()`

- [ ] **Memory management is correct**
  - All GC pointers visited in `visit_edges()`
  - No raw pointers to GC objects stored
  - No memory leaks (tested with sanitizers)

## Creating the PR

### PR Title

- [ ] **Title follows commit message format**
  - `Category: Brief imperative description`
  - Single commit: use that commit's message
  - Multiple commits: summarize overall change

### PR Description

- [ ] **Summary explains WHAT and WHY**
  - What does this PR do?
  - Why is this change needed?
  - What problem does it solve?

- [ ] **Changes listed**
  - Major changes enumerated
  - API additions/changes highlighted
  - Breaking changes noted

- [ ] **Testing described**
  - How was this tested?
  - What test cases were covered?
  - What websites were tested?

- [ ] **Spec compliance noted** (for web standards)
  - Link to specification
  - Sections implemented
  - Known limitations/omissions

- [ ] **Related issues referenced**
  - `Fixes #123` for bug fixes
  - `Implements #456` for features
  - `Part of #789` for partial implementations

### PR Metadata

- [ ] **Appropriate labels added** (if available)
  - Component: LibWeb, LibJS, etc.
  - Type: bug, feature, refactor, etc.
  - Priority: if applicable

- [ ] **Reviewers assigned** (if known)

- [ ] **Milestone set** (if applicable)

## After Creating PR

### CI Checks

- [ ] **All CI checks pass**
  - Build succeeds on all platforms
  - Tests pass on all platforms
  - Sanitizers show no issues
  - Format check passes
  - Lint checks pass

- [ ] **Fix any CI failures immediately**

### Code Review

- [ ] **Respond to reviewer feedback promptly**

- [ ] **Address all review comments**
  - Fix issues raised
  - Explain why changes weren't made (if applicable)
  - Mark conversations as resolved

- [ ] **Update PR with fixes**
  ```bash
  # Option 1: Amend commit
  git add .
  git commit --amend
  git push --force-with-lease origin feature/name

  # Option 2: Fixup commit + squash
  git add .
  git commit --fixup=abc1234
  git rebase -i --autosquash upstream/master
  git push --force-with-lease origin feature/name
  ```

- [ ] **Request re-review after major changes**

### Final Checks

- [ ] **All conversations resolved**

- [ ] **Final approval received**

- [ ] **CI checks still passing** (after latest push)

- [ ] **Merge conflicts resolved**
  ```bash
  git fetch upstream
  git rebase upstream/master
  git push --force-with-lease origin feature/name
  ```

- [ ] **Ready to merge!**

## Web Standards Implementation Checklist

Additional checks when implementing web standards:

### Spec Compliance

- [ ] **Read entire spec section**
  - Understand all requirements
  - Understand edge cases
  - Understand error conditions

- [ ] **All algorithms implemented**
  - Algorithm steps numbered in comments
  - Spec links in comments
  - Function names match spec exactly

- [ ] **IDL interface matches spec**
  - Attributes match spec
  - Methods match spec
  - Exception types match spec
  - Extended attributes correct

- [ ] **Error handling matches spec**
  - Correct exception types thrown
  - Error messages helpful
  - Edge cases handled

- [ ] **Web Platform Tests pass** (if available)
  ```bash
  ./Meta/WPT.sh run --log results.log
  ```

### Browser Compatibility

- [ ] **Feature works with real websites**
  - Tested on popular sites using feature
  - No regressions on existing sites

- [ ] **Polyfills/fallbacks considered**
  - Graceful degradation if partial implementation

- [ ] **Developer tools support** (if applicable)
  - Feature visible in inspector
  - Useful error messages in console

## Security Feature Checklist (Fork)

Additional checks for Sentinel security features:

### Architecture

- [ ] **Core logic in `Services/Sentinel/`**
  - No LibWeb dependencies in detector
  - Unit tests included

- [ ] **Integration point in WebContent/RequestServer**
  - Owns detector instance
  - Handles IPC to UI

- [ ] **Minimal hooks in LibWeb**
  - Just calls into PageClient
  - No business logic

### Security Considerations

- [ ] **Threat model documented**
  - What attacks are prevented?
  - What is the security boundary?

- [ ] **False positive rate acceptable**
  - Tested against benign sites
  - Not too annoying for users

- [ ] **Performance impact acceptable**
  - Measured overhead < 10ms per operation
  - No blocking of UI thread

- [ ] **Privacy implications considered**
  - No data leakage
  - No PII collected

### Graceful Degradation

- [ ] **Fails gracefully on initialization error**
  - Browser continues to work
  - Error logged but not thrown

- [ ] **Null checks before use**
  ```cpp
  if (m_detector) {
      m_detector->analyze();
  }
  ```

- [ ] **Never blocks page load**
  - Detection happens asynchronously
  - No user-visible delays

### Testing

- [ ] **Unit tests for detector**
  - All detection algorithms tested
  - Edge cases covered

- [ ] **Browser integration tests**
  - Real attack patterns tested
  - Benign patterns don't trigger

- [ ] **PolicyGraph integration tested**
  - Policies created correctly
  - Policies evaluated correctly

- [ ] **Real-world testing**
  - Known malicious sites tested
  - Known benign sites tested

### Documentation

- [ ] **Architecture documented**
  - Component diagram
  - Data flow diagram
  - Integration points

- [ ] **User guide updated**
  - How to use feature
  - How to configure
  - What to expect

- [ ] **CHANGELOG updated**

## Common Mistakes to Avoid

### ❌ Don't

- Push directly to master
- Create PR from master branch
- Include merge commits in PR
- Leave debug code in commits
- Use "Fixed" instead of "Fix"
- Forget spec links for web standards
- Skip testing on real websites
- Ignore CI failures
- Force push without `--force-with-lease`
- Amend commits after others have reviewed
- Mix unrelated changes in one PR
- Use C-style casts
- Forget to visit GC pointers in `visit_edges()`
- Mutate layout tree
- Use `RefCounted` in LibWeb (use GC)

### ✅ Do

- Rebase on latest master before PR
- Test thoroughly before pushing
- Write atomic commits
- Follow commit message format
- Link to specs for web standards
- Add tests for all changes
- Respond promptly to reviews
- Keep PR focused on one thing
- Use `--force-with-lease` for force push
- Communicate in PR comments
- Request help when stuck
- Run sanitizers before pushing
- Check for memory leaks
- Match spec naming exactly

## Quick Pre-PR Command Sequence

```bash
# 1. Ensure branch is clean
git status

# 2. Rebase on latest master
git fetch upstream
git rebase upstream/master

# 3. Run format check
ninja -C Build/release check-style

# 4. Build
./Meta/ladybird.py build

# 5. Run tests
./Meta/ladybird.py test

# 6. Run sanitizers (if time permits)
cmake --preset Sanitizers
cmake --build Build/sanitizers
ctest --preset Sanitizers

# 7. Check commit messages
git log --oneline upstream/master..HEAD

# 8. Push
git push -u origin feature/my-feature

# 9. Create PR
gh pr create --title "Category: Description" --body "$(cat pr-body.md)"
```

## After Merge Cleanup

- [ ] **Delete local branch**
  ```bash
  git checkout master
  git branch -d feature/name
  ```

- [ ] **Delete remote branch**
  ```bash
  git push origin --delete feature/name
  ```

- [ ] **Update master**
  ```bash
  git pull upstream master
  git push origin master
  ```

- [ ] **Close related issues** (if not auto-closed)

## Resources

- **Coding Style**: Documentation/CodingStyle.md
- **Testing Guide**: Documentation/Testing.md
- **LibWeb Patterns**: Documentation/LibWebPatterns.md
- **Git Workflow**: .claude/skills/ladybird-git-workflow/SKILL.md
- **Contributing Guide**: Documentation/GettingStartedContributing.md
