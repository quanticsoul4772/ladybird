# Pull Request Description Template

## Basic PR Template

```markdown
## Summary

Brief description of what this PR accomplishes and why it's needed.

## Changes

- Major change 1
- Major change 2
- Major change 3

## Testing

Description of how this was tested:
- [ ] Unit tests added/updated
- [ ] Browser testing performed
- [ ] Tested with real websites: example.com, test.org
- [ ] Verified with sanitizers (ASAN/UBSAN)
- [ ] All existing tests pass

## Related Issues

Fixes #123
Closes #456
Related to #789
```

---

## Web Standards Implementation PR

```markdown
## Summary

Implements [Feature Name] from the [Spec Name] specification.

## Specification

- Spec: https://html.spec.whatwg.org/multipage/...
- Section: X.Y.Z Feature Name
- Compliance: Full / Partial (list missing features)

## Changes

- Implemented algorithms:
  - algorithm_one()
  - algorithm_two()
  - helper_function()
- Added WebIDL interface: InterfaceName
- CSS properties: property-one, property-two
- DOM methods: method1(), method2()

## Implementation Notes

Key design decisions:
- Why approach X was chosen over Y
- Performance considerations
- Browser compatibility notes
- Known limitations

## Testing

- [ ] Text tests: Tests/LibWeb/Text/input/feature-name.html
- [ ] Layout tests: Tests/LibWeb/Layout/feature-name.html
- [ ] Ref tests: Tests/LibWeb/Ref/feature-name.html
- [ ] Unit tests: Libraries/LibWeb/HTML/TestFeature.cpp
- [ ] Tested against spec test suite
- [ ] Verified with real websites using this feature

## Examples Tested

- https://developer.mozilla.org/en-US/docs/Web/API/FeatureName
- https://codepen.io/example/feature-test
- Real-world usage on popular sites

## Spec Compliance Checklist

- [ ] All algorithms implemented
- [ ] Error handling matches spec
- [ ] Edge cases covered
- [ ] Naming matches spec exactly
- [ ] Comments reference spec sections

## Related Issues

Implements #123
Part of #456
```

---

## Bug Fix PR

```markdown
## Summary

Fixes [brief description of bug] that caused [impact].

## Problem

Detailed description of the bug:
- What was happening
- Why it was wrong
- How it was discovered

## Root Cause

Technical explanation of the underlying issue:
- Which component was affected
- Why the code was incorrect
- How it led to the observed behavior

## Solution

Explanation of the fix:
- What was changed
- Why this approach was chosen
- Alternative approaches considered

## Testing

- [ ] Added regression test: path/to/test.cpp
- [ ] Verified fix resolves original issue
- [ ] Tested related functionality still works
- [ ] No new regressions introduced
- [ ] Sanitizers show no new issues

## Reproduction

Steps to reproduce the original bug:
1. Load page with X
2. Click on Y
3. Observe crash/incorrect behavior

## Related Issues

Fixes #123
Regression from #456
```

---

## Fork-Specific Security Feature PR

```markdown
## Summary

Implements [Security Feature] for [Component] as part of Sentinel Milestone X.Y.

## Motivation

Why this security feature is needed:
- Threat model addressed
- Attack vectors mitigated
- User protection goals

## Architecture

Component design:
- Core detection logic: Services/Sentinel/FeatureDetector.{h,cpp}
- Integration point: Services/WebContent/Component.cpp
- LibWeb hooks: Libraries/LibWeb/Module/Class.cpp
- PolicyGraph integration: Yes/No

## Detection Algorithm

High-level description of how detection works:
1. Monitor API calls
2. Calculate risk score
3. Check against policy
4. Alert user if needed

## Changes

- Added classes:
  - FeatureDetector (core logic)
  - Integration in PageClient
  - Hooks in LibWeb APIs
- Database schema changes (if applicable)
- IPC messages added (if applicable)

## Security Considerations

- False positive rate: X%
- Performance impact: Y ms overhead
- Privacy implications: Z
- Graceful degradation: Fails open/closed

## Testing

- [ ] Unit tests: Services/Sentinel/TestFeature.cpp
- [ ] Browser tests: Tests/LibWeb/Text/input/security-*.html
- [ ] Tested against real attack sites
- [ ] Verified benign sites not flagged
- [ ] Tested graceful degradation on initialization failure

## Test Sites

Known malicious/fingerprinting sites tested:
- https://browserleaks.com/canvas
- https://amiunique.org/fingerprint
- [other test sites]

Benign sites verified:
- https://github.com
- https://example.com
- [other benign sites]

## Documentation

- [ ] Updated: docs/FEATURE_NAME.md
- [ ] Updated: docs/CHANGELOG.md
- [ ] Updated: CLAUDE.md (if workflow changes)
- [ ] User guide section added/updated

## Performance

Benchmark results:
- Overhead: X ms per operation
- Memory usage: Y KB additional
- Comparison to baseline: Z% increase

## Milestone Progress

- [x] Phase 1: Core detection
- [x] Phase 2: Integration
- [ ] Phase 3: UI alerts (planned)
- [ ] Phase 4: Policy management (planned)

## Related Issues

Part of Milestone 0.4 Phase X
Related to #123
Depends on #456
```

---

## Performance Improvement PR

```markdown
## Summary

Optimizes [Component] to improve [Metric] by X%.

## Problem

Current performance issue:
- Operation takes X ms
- Blocks UI thread
- Causes poor user experience on Y

## Analysis

Profiling results:
- Bottleneck identified: Function X
- Cause: Algorithm complexity O(n²)
- Profile data: [screenshot or data]

## Solution

Optimization approach:
- Changed algorithm from X to Y
- Added caching for Z
- Reduced complexity to O(n log n)

## Results

Benchmark improvements:
- Before: X ms
- After: Y ms
- Improvement: Z% faster

Test cases:
- Small input (100 items): A ms → B ms
- Medium input (1000 items): C ms → D ms
- Large input (10000 items): E ms → F ms

## Trade-offs

- Memory usage: +/- X KB
- Code complexity: Increased/Decreased/Same
- Maintainability: Notes

## Testing

- [ ] All existing tests pass
- [ ] Performance benchmarks added
- [ ] Tested on real-world workloads
- [ ] No regressions in functionality
- [ ] Verified with sanitizers

## Related Issues

Fixes #123 (slow performance report)
Improves #456
```

---

## Refactoring PR

```markdown
## Summary

Refactors [Component] to [Goal: improve maintainability/prepare for feature/etc].

## Motivation

Why this refactoring is needed:
- Current code is difficult to understand
- Preparing for upcoming feature X
- Reducing duplication across Y and Z

## Changes

- Extracted class A from B
- Moved functionality from X to Y
- Renamed functions to match spec terminology
- Simplified complex conditionals

## Impact

- Lines added: +X
- Lines removed: -Y
- Net change: Z
- Files affected: N

## Verification

This is a pure refactoring with no functional changes:
- [ ] All tests pass without modification
- [ ] No behavior changes
- [ ] Performance characteristics unchanged
- [ ] Binary diff shows equivalent code

## Testing

- [ ] All existing tests pass
- [ ] No new tests needed (pure refactoring)
- [ ] Verified no performance regression
- [ ] Manual testing of affected features

## Related Issues

Prepares for #123
Improves maintainability for #456
```

---

## Documentation PR

```markdown
## Summary

[Adds/Updates/Fixes] documentation for [Topic].

## Changes

- Added: docs/NEW_DOCUMENT.md
- Updated: docs/EXISTING_DOCUMENT.md
- Fixed: Typos and broken links in X

## Content

Brief overview of documentation changes:
- New sections added
- Clarifications made
- Examples added/improved

## Verification

- [ ] Links tested and working
- [ ] Code examples compile and run
- [ ] Technical accuracy verified
- [ ] Spelling/grammar checked
- [ ] Formatting consistent with existing docs

## Related Issues

Closes #123 (documentation request)
Documents #456 (feature added previously)
```

---

## Multi-Commit PR Checklist

When PR contains multiple commits:

- [ ] Each commit is atomic and buildable
- [ ] Commit messages follow format
- [ ] Commits in logical order
- [ ] No fixup/squash commits (use rebase -i)
- [ ] No merge commits (use rebase)
- [ ] Each commit tested individually (if possible)
- [ ] Clean history ready for review
