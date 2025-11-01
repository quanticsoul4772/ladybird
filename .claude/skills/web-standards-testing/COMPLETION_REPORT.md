# Web Standards Testing Skill - Completion Report

## Summary

Created comprehensive "web-standards-testing" skill for Ladybird browser project, covering Web Platform Tests (WPT) integration, LibWeb testing patterns, and standards compliance.

## Deliverables

### 1. Main Documentation

**SKILL.md** (1,132 lines)
- Web standards overview (WHATWG, W3C, TC39)
- Web Platform Tests integration (complete workflow)
- LibWeb test types (Text, Layout, Ref, Screenshot)
- Spec compliance patterns
- Cross-browser compatibility
- Test creation, execution, and debugging
- Best practices and common patterns

### 2. Example Tests (6 files)

**examples/text-test-example.html**
- Synchronous Text test
- DOM API testing pattern
- Spec reference included

**examples/text-test-async-example.html**
- Async Text test
- Microtask/promise testing
- Execution order verification

**examples/layout-test-example.html**
- Layout test structure
- Flexbox layout testing
- Layout tree dump example

**examples/ref-test-example.html**
- Ref test with box-shadow
- Visual comparison pattern

**examples/ref-test-reference.html**
- Reference page for Ref test
- Simplified equivalent rendering

**examples/wpt-test-example.html**
- WPT test harness structure
- testharness.js patterns
- Multiple test cases

### 3. Helper Scripts (4 files)

**scripts/run_wpt_subset.sh**
- Run WPT by category (dom, html, css, etc.)
- Baseline and comparison modes
- Parallel execution support
- Debug options

**scripts/compare_wpt_results.sh**
- Compare two WPT log files
- Show improvements and regressions
- Summary statistics
- Color-coded output

**scripts/create_libweb_test.sh**
- Wrapper for add_libweb_test.py
- Auto-rebaseline option
- Spec reference injection
- Editor integration

**scripts/rebaseline_tests.sh**
- Batch rebaseline by pattern
- Interactive mode
- Test filtering by type
- List-only mode

All scripts are executable (chmod +x applied).

### 4. Reference Documentation (4 files)

**references/wpt-guide.md**
- Comprehensive WPT documentation
- Test structure and assertions
- Running, comparing, importing tests
- Debugging patterns
- Common failure causes

**references/spec-references.md**
- Quick reference for web specifications
- WHATWG standards (HTML, DOM, Fetch, URL)
- W3C CSS specifications
- ECMAScript (TC39)
- Web APIs (WebGL, Web Audio, etc.)
- Reading specs effectively

**references/test-types.md**
- Decision tree for test type selection
- Detailed comparison of Text/Layout/Ref/Screenshot
- When to use each type
- Common mistakes and correct approaches
- Quick decision chart

**references/compatibility-matrix.md**
- Browser feature support matrix
- Feature detection patterns
- Cross-browser testing strategies
- Compatibility checking resources
- Browser-specific quirks

### 5. README.md

- Quick start guide
- Directory structure
- Key concepts
- Common workflows
- Script usage examples
- Tips and resources

## File Structure

```
.claude/skills/web-standards-testing/
├── SKILL.md                        # Main documentation (1,132 lines)
├── README.md                       # Quick reference
├── COMPLETION_REPORT.md           # This file
├── examples/                       # Test examples (6 files)
│   ├── text-test-example.html
│   ├── text-test-async-example.html
│   ├── layout-test-example.html
│   ├── ref-test-example.html
│   ├── ref-test-reference.html
│   └── wpt-test-example.html
├── scripts/                        # Helper scripts (4 files)
│   ├── run_wpt_subset.sh          # Executable
│   ├── compare_wpt_results.sh     # Executable
│   ├── create_libweb_test.sh      # Executable
│   └── rebaseline_tests.sh        # Executable
└── references/                     # Reference docs (4 files)
    ├── wpt-guide.md
    ├── spec-references.md
    ├── test-types.md
    └── compatibility-matrix.md

Total: 16 files
```

## Key Patterns Documented

### 1. WPT Integration

- **Running WPT**: `./Meta/WPT.sh run --log results.log css`
- **Comparing results**: `./Meta/WPT.sh compare --log new.log baseline.log css`
- **Importing tests**: `./Meta/WPT.sh import html/dom/test.html`
- **Parallel execution**: `--parallel-instances 0` for auto-detect
- **Debugging**: `--show-window` and `--debug-process WebContent`

### 2. LibWeb Test Types

**Text Tests** (API behavior):
```javascript
test(() => {
    const div = document.createElement("div");
    div.setAttribute("id", "test");
    println(`ID: ${div.id}`);
});
```

**Layout Tests** (box tree):
```html
<style>
    .container { display: flex; width: 400px; }
    .item { flex: 1; }
</style>
```

**Ref Tests** (visual with HTML reference):
```html
<link rel="match" href="../expected/shadow-ref.html" />
<style>
    .box { box-shadow: 5px 5px 10px black; }
</style>
```

**Screenshot Tests** (visual with image reference):
```html
<canvas id="c"></canvas>
<script>
    const ctx = c.getContext("2d");
    ctx.fillRect(0, 0, 100, 100);
</script>
```

### 3. Spec Compliance

**Critical Pattern**: Match spec algorithm names exactly

```cpp
// RIGHT - matches spec
bool HTMLInputElement::suffering_from_being_missing();

// WRONG - arbitrary naming
bool HTMLInputElement::has_missing_constraint();
```

**Algorithm Implementation**:
```cpp
// "To fire an event named e at target..."
void fire_an_event(EventTarget& target, FlyString const& event_name)
{
    // Step 1: Let event be a new event.
    auto event = Event::create(event_name);

    // Step 2: Initialize event's type to e
    // (handled in Event::create)

    // Step 3: Dispatch event at target
    target.dispatch_event(event);
}
```

### 4. Test Creation Workflow

```bash
# 1. Create test
./Tests/LibWeb/add_libweb_test.py my-feature Text

# 2. Edit test
vim Tests/LibWeb/Text/input/my-feature.html

# 3. Rebaseline
./Meta/ladybird.py run test-web --rebaseline -f Text/input/my-feature.html

# 4. Run test
./Meta/ladybird.py run test-web -f Text/input/my-feature.html
```

### 5. WPT Workflow

```bash
# 1. Baseline
./Meta/WPT.sh run --log baseline.log css

# 2. Make changes
git checkout feature-branch

# 3. Compare
./Meta/WPT.sh compare --log results.log baseline.log css

# 4. Import passing tests
./Meta/WPT.sh import css/css-color/color-rgb.html
```

## Challenges Encountered

### 1. Comprehensive Scope

**Challenge**: Web standards testing covers vast territory (WPT, LibWeb tests, 4 test types, multiple specs)

**Solution**: Organized into clear sections with decision trees and quick references. Created separate reference docs for deep dives.

### 2. Test Type Selection

**Challenge**: Choosing between Text/Layout/Ref/Screenshot is often unclear

**Solution**: Created detailed decision tree and comparison table in `references/test-types.md` with concrete examples.

### 3. Script Complexity

**Challenge**: WPT.sh has many options, easy to get wrong

**Solution**: Created wrapper scripts with sensible defaults and common use cases pre-configured.

### 4. Spec Compliance

**Challenge**: Matching spec naming isn't obvious to newcomers

**Solution**: Emphasized with examples, marked as "Critical Rule" in multiple places, included spec reading guide.

## Integration with Existing Skills

This skill complements existing Ladybird skills:

- **ladybird-cpp-patterns**: Provides C++ patterns, this adds web testing
- **browser-performance**: Performance testing, this adds correctness testing
- **fuzzing-workflow**: Security testing, this adds standards compliance
- **ipc-security**: IPC patterns, this adds web API testing

## Usage Examples

### Example 1: Implementing New DOM API

```bash
# Developer wants to implement Element.toggleAttribute()

# 1. Read spec
# https://dom.spec.whatwg.org/#dom-element-toggleattribute

# 2. Create test
./.claude/skills/web-standards-testing/scripts/create_libweb_test.sh \
    element-toggle-attribute Text \
    --spec https://dom.spec.whatwg.org/#dom-element-toggleattribute

# 3. Implement feature
vim Libraries/LibWeb/DOM/Element.cpp

# 4. Rebaseline test
./Meta/ladybird.py run test-web --rebaseline -f Text/input/element-toggle-attribute.html
```

### Example 2: Checking CSS Compatibility

```bash
# Developer implementing CSS Grid wants to check WPT status

# 1. Run WPT tests
./.claude/skills/web-standards-testing/scripts/run_wpt_subset.sh css --baseline

# 2. Check compatibility matrix
# references/compatibility-matrix.md shows Grid is partial in Ladybird

# 3. Compare with upstream
./Meta/WPT.sh run --log grid.log css/css-grid

# 4. Check WPT dashboard
# https://wpt.fyi/results/css/css-grid
```

### Example 3: Debugging Test Failure

```bash
# Test failing, need to debug

# 1. Run with visible browser
BUILD_PRESET=Debug ./Meta/ladybird.py run ladybird \
    --layout-test-mode Tests/LibWeb/Text/input/failing.html

# 2. Check with sanitizers
BUILD_PRESET=Sanitizers ./Meta/ladybird.py run test-web \
    -f Text/input/failing.html

# 3. Compare with spec
# references/spec-references.md has all spec links

# 4. Fix and rebaseline
./Meta/ladybird.py run test-web --rebaseline -f Text/input/failing.html
```

## Validation

### Documentation Completeness

✅ WPT integration (run, compare, import, bisect)
✅ LibWeb test types (all 4 types with examples)
✅ Spec compliance patterns (naming, algorithm structure)
✅ Cross-browser compatibility (feature detection, testing)
✅ Test creation workflows (step-by-step)
✅ Debugging strategies (sanitizers, visible browser, GDB)
✅ Helper scripts (4 scripts covering common tasks)
✅ Reference docs (specs, test types, compatibility)

### Script Functionality

✅ All scripts executable
✅ Error handling and usage messages
✅ Color-coded output
✅ Support for common options
✅ Integration with existing Ladybird tools

### Example Quality

✅ All test types represented
✅ Sync and async variants
✅ Spec references included
✅ WPT test harness example
✅ Production-ready patterns

## Skill Location

**Path**: `/home/rbsmith4/ladybird/.claude/skills/web-standards-testing/`

**Main Entry Point**: `SKILL.md`

**Quick Reference**: `README.md`

## Next Steps for Users

1. **Read SKILL.md** for comprehensive guide
2. **Check README.md** for quick start
3. **Use scripts/** for common workflows
4. **Reference examples/** when writing tests
5. **Consult references/** for deep dives

## Metrics

- **Main documentation**: 1,132 lines
- **Total files**: 16
- **Example tests**: 6
- **Helper scripts**: 4
- **Reference docs**: 4
- **Spec references**: 50+ web specifications documented
- **Test patterns**: 30+ documented patterns
- **Code examples**: 100+ code snippets

## Conclusion

The web-standards-testing skill provides comprehensive coverage of web standards testing in Ladybird, from WPT integration to LibWeb test creation, with practical scripts and extensive reference documentation. It follows the established pattern of other Ladybird skills while filling a critical gap in web standards compliance testing.

The skill is production-ready and can be immediately used by developers implementing web features in Ladybird.
