# Playwright Automation Skill

Comprehensive browser automation testing for Ladybird using Playwright MCP tools.

## Overview

This skill provides complete documentation, examples, templates, and helpers for writing Playwright-based browser automation tests for the Ladybird browser project.

## Contents

### 📘 SKILL.md
Main skill documentation covering:
- Complete Playwright MCP tools reference
- Test structure and organization patterns
- 10+ common test patterns (navigation, forms, visual regression, accessibility, etc.)
- Ladybird fork-specific security tests
- Debugging strategies
- CI/CD integration
- Best practices

### 📂 examples/
Ready-to-run test examples:
- **basic-navigation-test.js** - Simple page navigation and assertions
- **form-interaction-test.js** - Form filling and submission
- **visual-regression-test.js** - Screenshot comparison testing
- **accessibility-test.js** - ARIA compliance and keyboard navigation
- **multi-tab-test.js** - Multi-tab workflows and isolation
- **error-handling-test.js** - Error conditions (404, timeouts, JS errors)

### 📋 templates/
Starter templates for new tests:
- **test-template.js** - Basic test skeleton
- **fixture-template.js** - Reusable setup/teardown fixture
- **page-object-template.js** - Page Object pattern implementation

### 🔧 helpers/
Utility libraries:
- **visual-diff-helper.js** - Screenshot comparison and baseline management
- **accessibility-helper.js** - A11y validation and WCAG compliance checking
- **wait-helpers.js** - Smart waiting strategies (network idle, element appearance, etc.)
- **assertion-helpers.js** - Custom assertion utilities with clear error messages

### 📚 references/
Quick reference guides:
- **playwright-mcp-reference.md** - Complete MCP tools API reference
- **test-patterns-reference.md** - 25+ common test patterns quick reference
- **debugging-guide.md** - Troubleshooting failed tests
- **ci-integration-guide.md** - GitHub Actions integration and CI/CD patterns

## Quick Start

### 1. Run an Example Test

```bash
cd /home/rbsmith4/ladybird/.claude/skills/playwright-automation

# Run basic navigation test
node examples/basic-navigation-test.js

# Run form interaction test
node examples/form-interaction-test.js

# Run accessibility test
node examples/accessibility-test.js
```

### 2. Create a New Test from Template

```bash
# Copy template
cp templates/test-template.js ../../../Tests/Playwright/my-new-test.js

# Edit the template
# Replace placeholders:
# - [Test Name]
# - [URL]
# - [Expected content]
# - Add your test logic

# Run your test
node ../../../Tests/Playwright/my-new-test.js
```

### 3. Use Helpers in Your Tests

```javascript
// Import helpers
const VisualDiffHelper = require('./.claude/skills/playwright-automation/helpers/visual-diff-helper');
const AccessibilityHelper = require('./.claude/skills/playwright-automation/helpers/accessibility-helper');
const WaitHelpers = require('./.claude/skills/playwright-automation/helpers/wait-helpers');
const { AssertionHelpers } = require('./.claude/skills/playwright-automation/helpers/assertion-helpers');

// Use in tests
const visualDiff = new VisualDiffHelper();
const result = await visualDiff.compareWithBaseline('my-test', 'screenshot.png');

const a11yHelper = new AccessibilityHelper();
const violations = a11yHelper.checkSnapshot(snapshot);

await WaitHelpers.waitForText('Expected content', 10000);

await AssertionHelpers.assertSnapshotIncludes('Expected text');
```

## Common Use Cases

### Testing User Flows
```javascript
// See: examples/basic-navigation-test.js
// Pattern: Navigate → Assert → Interact → Verify
```

### Form Testing
```javascript
// See: examples/form-interaction-test.js
// Pattern: Fill → Submit → Validate result
```

### Visual Regression
```javascript
// See: examples/visual-regression-test.js
// Pattern: Capture → Compare → Report differences
```

### Accessibility Validation
```javascript
// See: examples/accessibility-test.js
// Pattern: Snapshot → Check violations → Test keyboard nav
```

### Security Feature Testing (Ladybird Fork)
```javascript
// Test fingerprinting detection
// Test credential protection
// Test phishing detection
// See: SKILL.md sections 8-10 for patterns
```

## Integration with Ladybird Testing

### File Organization

Recommended structure for Ladybird project:

```
Tests/Playwright/
├── e2e/                    # End-to-end user flows
├── visual/                 # Visual regression tests
├── accessibility/          # A11y compliance tests
├── security/               # Fork-specific security tests
├── fixtures/               # Test data and page objects
├── helpers/                # Copied from this skill
├── baselines/              # Visual regression baselines
├── screenshots/            # Test screenshots
├── reports/                # Test reports
└── run-all-tests.js        # Master test runner
```

### CI/CD Integration

Add to `.github/workflows/playwright-tests.yml`:

```yaml
name: Playwright Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build Ladybird
        run: cmake --build Build/release
      - name: Run Playwright tests
        run: node Tests/Playwright/run-all-tests.js
      - name: Upload screenshots on failure
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: screenshots
          path: Tests/Playwright/screenshots/
```

See `references/ci-integration-guide.md` for complete CI/CD setup.

## Documentation Structure

- **SKILL.md** - Start here for comprehensive overview
- **examples/** - Copy-paste ready examples
- **templates/** - Starting points for new tests
- **helpers/** - Reusable utilities
- **references/** - Quick reference guides

## Key Features

✅ **Complete MCP Tool Coverage** - All Playwright MCP tools documented
✅ **10+ Test Patterns** - Common patterns ready to use
✅ **6 Working Examples** - Copy-paste ready test files
✅ **3 Templates** - Scaffolding for new tests
✅ **4 Helper Libraries** - Visual diff, A11y, Wait, Assertions
✅ **4 Reference Guides** - Quick lookup documentation
✅ **Ladybird-Specific** - Fork security feature tests
✅ **CI/CD Ready** - GitHub Actions integration
✅ **Debugging Support** - Comprehensive troubleshooting guide

## Best Practices

1. **Use browser_snapshot over screenshots** for automation
2. **Wait for conditions, not arbitrary times**
3. **Capture evidence on failure** (screenshots, logs, snapshots)
4. **Make tests independent** - no shared state
5. **Use Page Objects** for complex pages
6. **Use descriptive element names** for debugging
7. **Add context to assertions** for clear failures

## Related Skills

- **web-standards-testing** - WPT and LibWeb tests
- **fuzzing-workflow** - Fuzz testing for browser components
- **ci-cd-patterns** - General CI/CD patterns
- **multi-process-architecture** - Understanding Ladybird's architecture

## Support

For questions or issues:
1. Check **debugging-guide.md** for common problems
2. Review **test-patterns-reference.md** for quick examples
3. Consult **playwright-mcp-reference.md** for API details
4. See **SKILL.md** for comprehensive documentation

## Contributing

When adding new patterns or examples:
1. Follow existing file structure
2. Include working examples with comments
3. Update SKILL.md with new patterns
4. Add to test-patterns-reference.md if generally useful

## License

This skill follows the Ladybird project license. See project LICENSE file.
