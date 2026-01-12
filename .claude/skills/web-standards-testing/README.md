# Web Standards Testing Skill

Comprehensive guide for Web Platform Tests (WPT) integration and LibWeb testing patterns in Ladybird browser development.

## Overview

This skill provides:
- Web Platform Tests (WPT) integration and workflows
- LibWeb test types (Text, Layout, Ref, Screenshot)
- Spec compliance patterns (WHATWG, W3C, TC39)
- Cross-browser compatibility testing
- Helper scripts for common testing workflows

## Quick Start

### Running WPT Tests

```bash
# Update WPT repository
./Meta/WPT.sh update

# Run specific category
./.claude/skills/web-standards-testing/scripts/run_wpt_subset.sh dom --baseline

# Compare results
./.claude/skills/web-standards-testing/scripts/compare_wpt_results.sh baseline.log current.log
```

### Creating LibWeb Tests

```bash
# Create Text test
./.claude/skills/web-standards-testing/scripts/create_libweb_test.sh my-feature Text --rebaseline

# Create Layout test
./Tests/LibWeb/add_libweb_test.py flexbox-test Layout

# Create Ref test
./Tests/LibWeb/add_libweb_test.py box-shadow Ref
```

### Rebaselining Tests

```bash
# Single test
./Meta/ladybird.py run test-web --rebaseline -f Text/input/my-test.html

# Multiple tests
./.claude/skills/web-standards-testing/scripts/rebaseline_tests.sh "Text/input/DOM-*.html"
```

## Directory Structure

```
web-standards-testing/
├── SKILL.md                    # Main skill documentation
├── README.md                   # This file
├── examples/                   # Test examples
│   ├── text-test-example.html
│   ├── text-test-async-example.html
│   ├── layout-test-example.html
│   ├── ref-test-example.html
│   ├── ref-test-reference.html
│   └── wpt-test-example.html
├── scripts/                    # Helper scripts
│   ├── run_wpt_subset.sh      # Run WPT categories
│   ├── compare_wpt_results.sh # Compare WPT runs
│   ├── create_libweb_test.sh  # Create test wrapper
│   └── rebaseline_tests.sh    # Batch rebaseline
└── references/                 # Reference documentation
    ├── wpt-guide.md           # WPT comprehensive guide
    ├── spec-references.md     # Web spec links
    ├── test-types.md          # When to use each test type
    └── compatibility-matrix.md # Browser compatibility

```

## Key Concepts

### Test Types

**Text Tests** - JavaScript API testing with text output
- Use: DOM APIs, event handling, JavaScript behavior
- Rebaseline: Easy (auto-generate)

**Layout Tests** - CSS layout tree structure
- Use: Flexbox, Grid, positioning, box model
- Rebaseline: Easy (auto-generate)

**Ref Tests** - Visual comparison against HTML reference
- Use: Box shadows, gradients, visual effects
- Rebaseline: Manual (create reference HTML)

**Screenshot Tests** - Visual comparison against image
- Use: Canvas, WebGL, complex SVG
- Rebaseline: Manual (generate screenshot)

### Spec Compliance

**Critical Rule**: Match spec algorithm names exactly

```cpp
// RIGHT
bool HTMLInputElement::suffering_from_being_missing();

// WRONG
bool HTMLInputElement::has_missing_constraint();
```

## Common Workflows

### 1. Implementing New Web Feature

```bash
# 1. Read spec
open https://dom.spec.whatwg.org/#dom-node-clonenode

# 2. Implement feature in LibWeb
vim Libraries/LibWeb/DOM/Node.cpp

# 3. Create test
./Tests/LibWeb/add_libweb_test.py node-clone Text

# 4. Write test matching spec examples
vim Tests/LibWeb/Text/input/node-clone.html

# 5. Rebaseline
./Meta/ladybird.py run test-web --rebaseline -f Text/input/node-clone.html

# 6. Run test
./Meta/ladybird.py run test-web -f Text/input/node-clone.html
```

### 2. WPT Workflow

```bash
# 1. Establish baseline
./Meta/WPT.sh run --log baseline.log css/css-color

# 2. Implement feature
# (make changes)

# 3. Compare results
./Meta/WPT.sh compare --log results.log baseline.log css/css-color

# 4. Import passing tests
./Meta/WPT.sh import css/css-color/color-rgb.html
```

### 3. Fixing Test Failures

```bash
# 1. Run failing test
./Meta/ladybird.py run test-web -f Text/input/failing-test.html

# 2. Debug with visible browser
BUILD_PRESET=Debug ./Meta/ladybird.py run ladybird \
    --layout-test-mode Tests/LibWeb/Text/input/failing-test.html

# 3. Check with sanitizers
BUILD_PRESET=Sanitizers ./Meta/ladybird.py run test-web \
    -f Text/input/failing-test.html

# 4. Fix implementation

# 5. Rebaseline if output changed intentionally
./Meta/ladybird.py run test-web --rebaseline -f Text/input/failing-test.html
```

## Examples

### Text Test Example

```html
<!DOCTYPE html>
<script src="include.js"></script>
<script>
test(() => {
    const div = document.createElement("div");
    div.setAttribute("id", "test");
    println(`ID: ${div.id}`);
});
</script>
```

### Layout Test Example

```html
<!DOCTYPE html>
<style>
.container {
    display: flex;
    width: 400px;
}
.item { flex: 1; }
</style>
<div class="container">
    <div class="item">A</div>
    <div class="item">B</div>
</div>
```

### Ref Test Example

```html
<!DOCTYPE html>
<link rel="match" href="../expected/shadow-ref.html" />
<style>
.box {
    width: 100px;
    height: 100px;
    box-shadow: 5px 5px 10px black;
}
</style>
<div class="box"></div>
```

## Resources

**In This Skill**:
- `SKILL.md` - Complete testing guide
- `references/wpt-guide.md` - WPT deep dive
- `references/spec-references.md` - Spec links
- `references/test-types.md` - Test type decision guide

**Ladybird Documentation**:
- `Documentation/Testing.md` - Testing overview
- `Documentation/LibWebPatterns.md` - LibWeb patterns
- `Tests/LibWeb/add_libweb_test.py` - Test creation

**External**:
- WPT: https://web-platform-tests.org/
- WHATWG HTML: https://html.spec.whatwg.org/
- W3C CSS: https://www.w3.org/Style/CSS/
- MDN: https://developer.mozilla.org/

## Scripts Usage

### run_wpt_subset.sh

```bash
# Run DOM tests with baseline
./scripts/run_wpt_subset.sh dom --baseline

# Run CSS tests with comparison
./scripts/run_wpt_subset.sh css --compare baseline-css.log

# Run with parallelism
./scripts/run_wpt_subset.sh html --parallel 4

# Debug mode
./scripts/run_wpt_subset.sh fetch --show-window --debug
```

### compare_wpt_results.sh

```bash
# Compare two log files
./scripts/compare_wpt_results.sh baseline.log current.log

# Shows:
# - Tests that now pass (improvements)
# - Tests that now fail (regressions)
# - Summary statistics
```

### create_libweb_test.sh

```bash
# Create and rebaseline
./scripts/create_libweb_test.sh my-test Text --rebaseline

# Create with spec reference
./scripts/create_libweb_test.sh my-test Text \
    --spec https://dom.spec.whatwg.org/#dom-node-clonenode

# Create and open in editor
./scripts/create_libweb_test.sh my-test Layout --edit
```

### rebaseline_tests.sh

```bash
# Rebaseline by pattern
./scripts/rebaseline_tests.sh "Text/input/DOM-*.html"

# Specific type
./scripts/rebaseline_tests.sh --type Layout "flex-*.html"

# Interactive mode
./scripts/rebaseline_tests.sh --interactive "Text/input/element-*.html"

# List matching tests
./scripts/rebaseline_tests.sh --list "Layout/input/grid-*.html"
```

## Tips

### 1. Test Naming

✅ Good: `element-get-bounding-client-rect.html`
❌ Bad: `test1.html`

### 2. Test One Thing

✅ Good: One feature per test
❌ Bad: Multiple unrelated features in one test

### 3. Spec References

Always include spec links in tests:
```html
<!-- Spec: https://dom.spec.whatwg.org/#dom-node-clonenode -->
```

### 4. Rebaseline Carefully

Only rebaseline when:
- You intentionally changed output
- You fixed a bug
- You updated expected behavior

Never rebaseline to hide failures!

### 5. Cross-Browser Check

When spec is ambiguous:
- Check WPT dashboard for other browsers
- Test in Chrome/Firefox/Safari
- Ask on Ladybird Discord

## Getting Help

- **Ladybird Discord**: https://discord.gg/ladybird
- **GitHub Issues**: https://github.com/LadybirdBrowser/ladybird/issues
- **Documentation**: `Documentation/Testing.md`
- **WPT Docs**: https://web-platform-tests.org/

## Contributing

When adding tests:
1. Follow existing patterns
2. Add spec references
3. Test edge cases
4. Document why the test exists
5. Commit test with implementation
