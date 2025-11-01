---
name: bugfix
description: Comprehensive workflow for triaging, fixing, and verifying bug fixes in Ladybird
applies_to:
  - Crashes and segfaults
  - Rendering bugs (layout, paint, CSS)
  - Memory leaks and use-after-free
  - Spec violations (HTML/CSS/JS/WebAPI)
  - Performance regressions
  - IPC communication failures
agents:
  - debugger
  - reviewer
  - tester
  - security
skills:
  - ladybird-cpp-patterns
  - multi-process-architecture
  - ipc-security
  - fuzzing-workflow
---

# Bug Fix Workflow

## Purpose
Structured workflow for diagnosing, fixing, testing, and verifying bug fixes across all Ladybird components. Ensures bugs are properly understood before fixing, regressions are prevented, and fixes are thoroughly validated.

## Overview

Bug fixing follows a systematic 7-phase approach:

1. **Triage** - Reproduce and categorize the bug
2. **Diagnosis** - Use appropriate tools to understand the failure
3. **Root Cause Analysis** - Identify why the code fails
4. **Fix Implementation** - Minimal, targeted fix
5. **Testing** - Regression tests + existing test validation
6. **Verification** - Confirm fix without introducing new issues
7. **Documentation** - Clear commit message and code comments

## Phase 1: Triage

**Goal**: Reproduce the bug reliably and gather initial context.

### Steps

1. **Reproduce the Bug**
   ```bash
   # Build with Debug preset for better diagnostics
   BUILD_PRESET=Debug ./Meta/ladybird.py run

   # Or with sanitizers for memory issues
   BUILD_PRESET=Sanitizers ./Meta/ladybird.py run
   ```

2. **Identify Bug Category**
   - **Crash**: Segfault, assertion failure, abort
   - **Memory**: Leak, use-after-free, double-free, buffer overflow
   - **Rendering**: Incorrect layout, missing paint, CSS issues
   - **Spec Violation**: Behavior doesn't match web standards
   - **Performance**: Slowdown, freeze, high CPU/memory usage
   - **IPC**: Message handling failure, process communication breakdown

3. **Assess Severity**
   - **Critical**: Crash on common sites, security vulnerability, data loss
   - **High**: Broken core functionality, severe UX degradation
   - **Medium**: Feature partially broken, workaround available
   - **Low**: Edge case, cosmetic issue

4. **Gather Context**
   ```bash
   # Check console output
   WEBCONTENT_DEBUG=1 LIBWEB_DEBUG=1 ./Build/debug/bin/Ladybird

   # Save reproduction steps
   # - URL or test case
   # - User actions (click sequence, form input, etc.)
   # - Expected vs actual behavior
   # - Browser state (cookies, localStorage, etc.)
   ```

5. **Create Minimal Test Case**
   - Strip down reproduction to minimum HTML/JS
   - Remove dependencies on external resources
   - Save as `.html` file for regression testing

### Decision Tree

```
Is bug reproducible?
├─ YES → Continue to Phase 2
└─ NO → Is it intermittent?
    ├─ YES → Try with sanitizers, add instrumentation
    └─ NO → Request more info, close as unreproducible
```

## Phase 2: Diagnosis

**Goal**: Use appropriate diagnostic tools to understand the failure mechanism.

### For Crashes

```bash
# 1. Get stack trace with debugger
./Meta/ladybird.py debug ladybird

# In GDB/LLDB:
# (gdb) run
# [crash occurs]
# (gdb) bt full          # Full backtrace
# (gdb) info registers   # Register state
# (gdb) info locals      # Local variables
# (gdb) print *object    # Inspect object state

# 2. Check for null dereference
# Look for patterns like:
# - Accessing m_member on null pointer
# - Dereferencing uninitialized RefPtr/OwnPtr
# - Use-after-free (pointer looks valid but freed)

# 3. Enable core dumps
ulimit -c unlimited
./Build/debug/bin/Ladybird
# After crash:
gdb ./Build/debug/bin/Ladybird core

# 4. Run with ASAN for memory corruption
BUILD_PRESET=Sanitizers ./Meta/ladybird.py run
```

**Common Crash Patterns**:
- Null pointer dereference: Missing null check before accessing member
- Use-after-free: Object deleted but pointer still used
- Assertion failure: Violated invariant (check assertion message)
- Stack overflow: Infinite recursion or excessive stack usage

### For Memory Issues

```bash
# 1. ASAN (Address Sanitizer) - detects most memory errors
BUILD_PRESET=Sanitizers ./Meta/ladybird.py run
# Look for:
# - heap-use-after-free
# - heap-buffer-overflow
# - stack-use-after-scope
# - memory leaks

# 2. Valgrind for detailed leak detection
valgrind --leak-check=full --show-leak-kinds=all \
  ./Build/debug/bin/Ladybird

# 3. Check RefPtr/OwnPtr usage
# - Are objects properly owned?
# - Is reference counting correct?
# - Are circular references possible?

# 4. Inspect object lifecycle
# Add dbgln() in constructor/destructor:
dbgln("MyClass({}) constructed", this);
dbgln("MyClass({}) destroyed", this);
```

**Common Memory Patterns**:
- Leak: Object allocated but never freed (missing RefPtr adoption)
- UAF: Object freed but RefPtr still exists and dereferenced
- Double-free: Object freed twice (check ASAN output)
- Circular refs: Two objects holding RefPtr to each other

### For Rendering Issues

```bash
# 1. Enable LibWeb debug output
LIBWEB_DEBUG=1 ./Build/debug/bin/Ladybird

# 2. Inspect layout tree
# In PageClient.cpp or WebContent, add:
dbgln("Layout tree:\n{}", document.layout_node()->debug_dump());

# 3. Check paint operations
# Add debug output in paint methods:
dbgln("Painting {} at {}", node->debug_description(), rect);

# 4. Compare to reference browsers
# Open same page in Firefox/Chrome
# Inspect element properties in DevTools
# Compare computed styles

# 5. Check CSS property handling
# - Is property parsed correctly?
# - Is value computed properly?
# - Is layout algorithm using the value?

# 6. Use LibWeb test infrastructure
./Meta/ladybird.py run test-web -- -f Layout/input/test-case.html
# Compare expected vs actual layout tree
```

**Common Rendering Patterns**:
- Missing paint: Element not added to paint tree
- Incorrect layout: Wrong algorithm or missing style property
- Style not applied: CSS selector not matching or property not inherited
- Z-order wrong: Stacking context issue

### For Spec Violations

```bash
# 1. Find the relevant spec
# HTML: https://html.spec.whatwg.org/
# CSS: https://www.w3.org/Style/CSS/
# DOM: https://dom.spec.whatwg.org/
# WebAPI: https://developer.mozilla.org/

# 2. Compare implementation to spec algorithm
# Spec algorithms have numbered steps
# Implementation should match step-by-step

# 3. Run Web Platform Tests
./Meta/WPT.sh run --log results.log
./Meta/WPT.sh compare --log results.log expectations.log <area>

# 4. Check test262 for JS issues
cd Build/release && ctest -R test262

# 5. Test in reference browsers
# Does Firefox/Chrome match spec?
# Is our behavior different?
```

**Common Spec Violation Patterns**:
- Missing spec step: Algorithm incomplete
- Wrong spec interpretation: Misunderstood spec language
- Outdated spec: Following old version of spec
- Intentional deviation: Check for FIXME comments

### For Performance Regressions

```bash
# 1. Profile with callgrind
./Meta/ladybird.py profile ladybird
# Opens in KCachegrind or similar

# 2. Add manual timing
#include <AK/Time.h>
auto start = MonotonicTime::now();
// ... code to measure ...
auto elapsed = MonotonicTime::now() - start;
dbgln("Operation took {}ms", elapsed.to_milliseconds());

# 3. Check for algorithmic issues
# - O(n²) where O(n) expected?
# - Excessive allocations?
# - Redundant work (same calculation repeated)?

# 4. Compare git history
# Use git bisect to find regression commit
git bisect start
git bisect bad HEAD
git bisect good <known-good-commit>
# Test each revision
```

**Common Performance Patterns**:
- Excessive DOM queries: Cache results instead of re-querying
- Layout thrashing: Reading layout properties triggers reflow in loop
- Memory churn: Too many allocations/deallocations
- Synchronous IPC: Use async messages where possible

### For IPC Failures

```bash
# 1. Enable IPC debug logging
IPC_DEBUG=1 ./Build/debug/bin/Ladybird

# 2. Check message definitions
# Look at .ipc files:
# - Is message properly defined?
# - Are parameter types correct?
# - Is response expected (=>) or async (=|)?

# 3. Verify process lifecycle
ps aux | grep -E 'WebContent|RequestServer|ImageDecoder'
# Are processes spawning/dying unexpectedly?

# 4. Check for rate limiting or validation failures
# Fork features: ValidatedDecoder, rate limiting in IPC handlers

# 5. Attach debugger to both processes
# Terminal 1:
gdb -p $(pgrep WebContent)
# Terminal 2:
gdb -p $(pgrep RequestServer)
```

**Common IPC Patterns**:
- Deadlock: Both processes waiting for each other (sync messages)
- Process crash: Child process dying, parent not handling
- Message validation failure: Invalid parameters rejected
- Rate limiting: Too many messages sent too fast

## Phase 3: Root Cause Analysis

**Goal**: Understand WHY the code fails, not just WHERE it fails.

### Analysis Questions

1. **What invariant was violated?**
   - What assumption does the code make?
   - Why is that assumption false in this case?

2. **What code path leads to the failure?**
   - Trace execution from entry point to failure
   - Identify branch conditions that lead to bad state

3. **What is the scope of impact?**
   - Is this failure specific to one scenario?
   - Could other code paths trigger same issue?
   - Are there similar patterns elsewhere?

4. **Why wasn't this caught before?**
   - Is there a missing test case?
   - Should there be an assertion?
   - Is error handling insufficient?

### Root Cause Documentation

```markdown
## Root Cause

[One sentence summary]

### Context
[What the code is trying to do]

### Failure Mechanism
[Step-by-step explanation of how failure occurs]

### Why It Wasn't Caught
[Missing validation, test gap, etc.]

### Scope of Impact
[Affected scenarios, similar patterns elsewhere]
```

## Phase 4: Fix Implementation

**Goal**: Implement minimal, targeted fix that addresses root cause without introducing regressions.

### Fix Principles

1. **Minimal Change**
   - Fix only what's broken
   - Avoid "while we're here" refactoring
   - Defer cleanup to separate commits

2. **Defensive Programming**
   ```cpp
   // Add null checks
   if (!object) {
       dbgln("ERROR: Unexpected null object in context X");
       return Error::from_string_literal("Object is null");
   }

   // Add assertions for invariants
   VERIFY(m_state == State::Initialized);

   // Use TRY for error propagation
   auto result = TRY(fallible_operation());

   // Validate inputs
   if (index >= m_items.size()) {
       return Error::from_string_literal("Index out of bounds");
   }
   ```

3. **Follow Existing Patterns**
   - Match code style of surrounding code
   - Use same error handling approach
   - Follow RefPtr/OwnPtr ownership patterns

4. **Consider Edge Cases**
   - Empty collections
   - Null/undefined values
   - Boundary conditions (0, MAX, negative)
   - Concurrent access (if multi-threaded)

### Common Fix Patterns

#### Null Pointer Fix
```cpp
// BEFORE (crash):
auto* element = document.get_element_by_id(id);
return element->inner_text();

// AFTER (safe):
auto* element = document.get_element_by_id(id);
if (!element)
    return Error::from_string_literal("Element not found");
return element->inner_text();
```

#### Use-After-Free Fix
```cpp
// BEFORE (UAF):
void process_events() {
    for (auto& event : m_events) {
        // handle_event might delete 'event' from m_events
        handle_event(event);
    }
}

// AFTER (safe):
void process_events() {
    // Copy events to avoid iterator invalidation
    auto events_copy = m_events;
    for (auto& event : events_copy) {
        handle_event(event);
    }
}
```

#### Memory Leak Fix
```cpp
// BEFORE (leak):
auto* object = new MyObject();
// ... code that might return early ...
delete object;

// AFTER (RAII):
auto object = adopt_own(new MyObject());
// ... code that might return early ...
// object automatically deleted
```

#### Spec Violation Fix
```cpp
// Spec says: "If value is null, return empty string"
// BEFORE (wrong):
String get_attribute(String const& name) {
    return m_attributes.get(name).value();  // Crashes if null
}

// AFTER (matches spec):
String get_attribute(String const& name) {
    // Step 1: If value is null, return empty string
    auto value = m_attributes.get(name);
    if (!value.has_value())
        return String {};

    // Step 2: Return value
    return value.value();
}
```

### Fix Implementation Checklist

- [ ] Fix addresses root cause, not just symptom
- [ ] No regressions in existing functionality
- [ ] Error handling added where needed
- [ ] Assertions added for invariants
- [ ] Code follows project style (run `ninja check-style`)
- [ ] No compiler warnings introduced
- [ ] Comments explain WHY for non-obvious fixes

## Phase 5: Testing

**Goal**: Create regression test and validate fix doesn't break existing functionality.

### Regression Test Creation

```bash
# 1. Create LibWeb test for web-facing bugs
./Tests/LibWeb/add_libweb_test.py bug-12345-crash-on-null Text

# Edit Tests/LibWeb/Text/input/bug-12345-crash-on-null.html:
cat > Tests/LibWeb/Text/input/bug-12345-crash-on-null.html <<'EOF'
<!DOCTYPE html>
<script src="../include.js"></script>
<script>
    test(() => {
        // Reproduction case that used to crash
        const element = document.getElementById('nonexistent');
        // Should not crash, should handle null gracefully
        assert_equals(element, null);
        println("PASS: Handled null element without crashing");
    });
</script>
EOF

# 2. Run the test
./Meta/ladybird.py run test-web -- -f Text/input/bug-12345-crash-on-null.html

# 3. For C++ unit tests, add to appropriate Test*.cpp
# Example: Tests/LibWeb/TestDOM.cpp
TEST_CASE(null_element_access) {
    // Test case for bug #12345
    auto document = create_test_document();
    auto element = document->get_element_by_id("nonexistent");
    EXPECT_EQ(element, nullptr);
}
```

### Test Categories by Bug Type

**Crash Tests**:
```javascript
// Should not crash
test(() => {
    try {
        trigger_crash_condition();
        println("PASS: No crash");
    } catch (e) {
        println("FAIL: Threw exception: " + e);
    }
});
```

**Memory Leak Tests**:
```cpp
// Run with ASAN
TEST_CASE(no_memory_leak) {
    for (int i = 0; i < 1000; i++) {
        auto object = create_and_use_object();
        // ASAN will report if leaks occur
    }
}
```

**Rendering Tests**:
```bash
# Layout test compares layout tree
./Tests/LibWeb/add_libweb_test.py correct-flex-layout Layout

# Ref test compares screenshot to reference HTML
./Tests/LibWeb/add_libweb_test.py correct-gradient-rendering Ref
```

**Spec Compliance Tests**:
```javascript
// Follow spec algorithm step-by-step
test(() => {
    // Spec: https://html.spec.whatwg.org/#dom-document-getelementbyid

    // Step 1: If there is no element with ID, return null
    const result = document.getElementById('nonexistent');
    assert_equals(result, null);

    // Step 2: Otherwise, return the element
    const div = document.createElement('div');
    div.id = 'test';
    document.body.appendChild(div);
    assert_equals(document.getElementById('test'), div);
});
```

### Existing Test Validation

```bash
# 1. Run all tests
./Meta/ladybird.py test

# 2. Run affected subsystem tests
./Meta/ladybird.py test LibWeb
./Meta/ladybird.py test LibJS
./Meta/ladybird.py test AK

# 3. Run with sanitizers
BUILD_PRESET=Sanitizers ./Meta/ladybird.py test

# 4. Run Web Platform Tests if web-facing
./Meta/WPT.sh run --log results.log
./Meta/WPT.sh compare --log baseline.log results.log <area>

# 5. Verify no new failures
# Compare test results before/after fix
```

### Manual Testing

```bash
# 1. Test original reproduction case
# Follow saved reproduction steps from Phase 1

# 2. Test related scenarios
# - Different input values
# - Edge cases (empty, null, large values)
# - Related features

# 3. Test on real websites
# Load popular sites to ensure no regressions:
# - github.com
# - google.com
# - wikipedia.org
# - twitter.com
```

## Phase 6: Verification

**Goal**: Confirm fix resolves issue without introducing new problems.

### Verification Checklist

- [ ] **Original Bug Fixed**
  - [ ] Reproduction case no longer triggers bug
  - [ ] Expected behavior now occurs
  - [ ] No error messages or warnings

- [ ] **No New Bugs**
  - [ ] All existing tests pass
  - [ ] No new sanitizer warnings
  - [ ] No new crashes in manual testing
  - [ ] Popular websites still work

- [ ] **Performance Acceptable**
  - [ ] No significant slowdown (< 5% regression)
  - [ ] Memory usage unchanged or improved
  - [ ] No new allocations in hot paths

- [ ] **Code Quality**
  - [ ] Follows coding style (`ninja check-style`)
  - [ ] No compiler warnings
  - [ ] Clear variable names
  - [ ] Appropriate comments

- [ ] **IPC Safety** (if applicable)
  - [ ] Message validation present
  - [ ] No sync IPC deadlocks possible
  - [ ] Rate limiting respected

- [ ] **Security** (if applicable)
  - [ ] No new attack surface
  - [ ] Input validation comprehensive
  - [ ] No data leaks possible

### Pre-Commit Testing

```bash
# 1. Full test suite
./Meta/ladybird.py test

# 2. Style check
ninja -C Build/release check-style

# 3. Build all presets
./Meta/ladybird.py build --preset Release
./Meta/ladybird.py build --preset Debug
./Meta/ladybird.py build --preset Sanitizers

# 4. Run sanitizer tests
BUILD_PRESET=Sanitizers ./Meta/ladybird.py test

# 5. Manual smoke test
./Meta/ladybird.py run
# Load several websites, perform common actions
```

## Phase 7: Documentation

**Goal**: Document the fix for future developers and code reviewers.

### Commit Message Format

```
Category: Fix brief description of bug

Detailed explanation of root cause and fix.

The bug occurred because [root cause explanation].

This fix [what the fix does and why it works].

Fixes #12345
```

**Example**:
```
LibWeb: Fix crash when accessing non-existent element's innerHTML

The bug occurred because get_element_by_id() could return null, but
callers assumed it always returned a valid element pointer. When
accessing innerHTML on a null pointer, the browser would crash.

This fix adds a null check before accessing element properties and
returns an appropriate error when the element doesn't exist. This
matches the HTML spec behavior for getElementById.

A regression test has been added in LibWeb/Text to ensure this
case is handled correctly in the future.

Fixes #12345
```

### Code Comments

```cpp
// For non-obvious fixes, add comments explaining WHY

// BEFORE (no context):
if (!element)
    return Error::from_string_literal("Element not found");

// AFTER (explains why check is needed):
// getElementById can return null when no element has the given ID.
// We must check for null before accessing element properties to
// avoid null pointer dereference (bug #12345).
if (!element)
    return Error::from_string_literal("Element not found");
```

### Documentation Updates

```bash
# If fix changes public API or behavior:

# 1. Update API documentation
# Edit Documentation/LibWeb/API.md or similar

# 2. Update user-facing docs if behavior changes
# Edit Documentation/UserGuide.md

# 3. Update CHANGELOG if significant fix
# Add entry to CHANGELOG.md (or fork's docs/CHANGELOG.md)
```

## Bug-Type-Specific Guides

### Crash Debugging Workflow

```
1. Get stack trace with debugger
   ├─ Null pointer dereference?
   │  └─ Add null check before access
   ├─ Assertion failure?
   │  └─ Understand violated invariant, fix caller or assertion
   ├─ Use-after-free?
   │  └─ Fix object lifetime (RefPtr, ownership)
   └─ Stack overflow?
      └─ Fix recursion or reduce stack usage

2. Identify object state at crash
   └─ Inspect member variables, understand why state is invalid

3. Trace backwards to find where invalid state created
   └─ Add defensive checks at state transitions

4. Create regression test that triggers crash
   └─ Test should pass after fix

5. Verify fix with ASAN
   └─ BUILD_PRESET=Sanitizers ./Meta/ladybird.py test
```

### Memory Leak Workflow

```
1. Run with ASAN to identify leak
   └─ BUILD_PRESET=Sanitizers ./Meta/ladybird.py run

2. Identify leaked object type from ASAN output
   └─ Look for allocation stack trace

3. Check object ownership
   ├─ Should be RefPtr-managed?
   │  └─ Ensure proper adoption with adopt_ref/adopt_own
   ├─ Circular reference?
   │  └─ Break cycle with WeakPtr or different ownership
   └─ Missing cleanup?
      └─ Add cleanup in destructor or explicit cleanup method

4. Add lifecycle logging
   └─ dbgln in constructor/destructor to track creation/destruction

5. Create test that would leak without fix
   └─ Run test with ASAN to verify no leak
```

### Rendering Bug Workflow

```
1. Create minimal test case HTML
   └─ Strip to minimum HTML/CSS that reproduces issue

2. Inspect computed styles
   └─ Use browser DevTools or dbgln in LibWeb

3. Compare layout tree to expected
   └─ Use layout test infrastructure

4. Identify divergence point
   ├─ CSS parsing issue?
   │  └─ Check StyleProperties.cpp, CSSParser.cpp
   ├─ Style computation issue?
   │  └─ Check StyleComputer.cpp
   ├─ Layout algorithm issue?
   │  └─ Check LayoutBox.cpp, FormattingContext.cpp
   └─ Paint issue?
      └─ Check PaintableBox.cpp, painting code

5. Check spec for algorithm
   └─ Implement missing steps or fix incorrect steps

6. Create Layout or Ref test
   └─ Test expected vs actual rendering
```

### Spec Violation Workflow

```
1. Find relevant spec section
   └─ Search specs at https://spec.whatwg.org/

2. Read spec algorithm
   └─ Note all steps, especially edge cases

3. Compare implementation to spec
   └─ Identify missing or incorrect steps

4. Implement spec algorithm exactly
   └─ Match spec naming (e.g., "suffering_from_being_missing")

5. Test edge cases mentioned in spec
   └─ Null, undefined, empty, boundary values

6. Run Web Platform Tests
   └─ ./Meta/WPT.sh run <area>

7. Cross-check with reference browsers
   └─ Firefox, Chrome behavior
```

### Performance Regression Workflow

```
1. Use git bisect to find regression commit
   └─ git bisect start; git bisect bad HEAD; git bisect good <old>

2. Profile before and after regression
   └─ ./Meta/ladybird.py profile ladybird

3. Compare flame graphs
   └─ Identify hot paths that changed

4. Analyze algorithmic complexity
   ├─ Added O(n²) loop?
   ├─ Excessive allocations?
   ├─ Redundant work?
   └─ Synchronous IPC in loop?

5. Optimize hot path
   ├─ Cache results instead of recomputing
   ├─ Use faster data structure
   ├─ Reduce allocations (use TRY_APPEND vs append)
   └─ Batch IPC messages

6. Benchmark before and after fix
   └─ Ensure performance restored
```

### IPC Failure Workflow

```
1. Enable IPC debug logging
   └─ IPC_DEBUG=1 ./Meta/ladybird.py run

2. Identify failing message
   └─ Check logs for message type and parameters

3. Review .ipc definition
   ├─ Correct parameter types?
   ├─ Synchronous (=>) or async (=|)?
   └─ Response type correct?

4. Check message handler
   ├─ Proper parameter validation?
   ├─ Error handling present?
   └─ Async completion handled?

5. Verify process lifecycle
   ├─ Child process spawned correctly?
   ├─ Process crash handling working?
   └─ No deadlocks (sync IPC calling sync IPC)?

6. Test IPC under load
   └─ Rapid message sending, rate limiting working?
```

## Pre-Commit Checklist

Before committing your bug fix, verify:

### Build and Test
- [ ] `./Meta/ladybird.py build --preset Release` succeeds
- [ ] `./Meta/ladybird.py build --preset Debug` succeeds
- [ ] `./Meta/ladybird.py build --preset Sanitizers` succeeds
- [ ] `./Meta/ladybird.py test` passes all tests
- [ ] `BUILD_PRESET=Sanitizers ./Meta/ladybird.py test` passes
- [ ] New regression test added and passing
- [ ] `ninja -C Build/release check-style` passes

### Code Quality
- [ ] No compiler warnings introduced
- [ ] No sanitizer warnings (ASAN/UBSAN)
- [ ] Follows coding style (snake_case, m_ prefix, east const)
- [ ] Error handling uses TRY/ErrorOr pattern
- [ ] Appropriate null checks added
- [ ] Comments explain non-obvious fixes

### Bug Fix Validation
- [ ] Original bug reproduction case fixed
- [ ] Root cause addressed, not just symptom
- [ ] No regressions in existing tests
- [ ] Manual testing of related features passed
- [ ] Popular websites still work correctly

### Security (if applicable)
- [ ] No new attack surface introduced
- [ ] Input validation comprehensive
- [ ] IPC message validation present
- [ ] No data leaks possible

### Documentation
- [ ] Commit message explains root cause and fix
- [ ] References bug number (Fixes #XXXX)
- [ ] Code comments added for tricky fixes
- [ ] API documentation updated if needed

### IPC Changes (if applicable)
- [ ] .ipc file changes are minimal and necessary
- [ ] Message validation added
- [ ] No sync IPC deadlocks introduced
- [ ] Rate limiting respected

### Performance
- [ ] No significant performance regression (< 5%)
- [ ] Profiled if performance-critical code changed
- [ ] No excessive allocations in hot paths

## Common Pitfalls and Warnings

### Don't Fix Symptoms, Fix Root Causes
```cpp
// WRONG: Papering over the symptom
if (object && object->is_valid() && !object->is_deleted()) {
    object->do_something();  // Why so many checks? What's the real issue?
}

// RIGHT: Fix the root cause
// Root cause: Object can be in deleted state but still reachable
// Fix: Remove object from collection when deleted
void delete_object(Object* object) {
    m_objects.remove(object);
    delete object;
}
```

### Don't Ignore Error Conditions
```cpp
// WRONG: Ignoring potential failure
auto result = fallible_operation();
// Use result without checking

// RIGHT: Handle or propagate error
auto result = TRY(fallible_operation());
// Or check explicitly:
if (result.is_error()) {
    dbgln("Operation failed: {}", result.error());
    return result.release_error();
}
```

### Don't Assume Spec Knowledge
```cpp
// WRONG: Assuming without checking spec
// "I think getElementById should throw when not found"

// RIGHT: Check the spec
// HTML spec says: "getElementById returns null if no element found"
// https://html.spec.whatwg.org/#dom-document-getelementbyid
Element* getElementById(String const& id) {
    // Spec: Step 1: If there is no element with ID, return null
    auto* element = find_element_by_id(id);
    if (!element)
        return nullptr;
    return element;
}
```

### Don't Skip Regression Tests
```
Even if the fix is "obvious", always add a regression test.
Future refactoring might re-introduce the bug.
```

### Don't Commit Unrelated Changes
```
Fix only the bug. No "while we're here" refactoring.
Separate commits for separate concerns.
```

## Additional Resources

### Debugging Documentation
- `Documentation/Testing.md` - Testing infrastructure
- `Documentation/ProcessArchitecture.md` - Multi-process debugging
- `Documentation/LibWebFromLoadingToPainting.md` - Rendering pipeline

### Spec References
- HTML: https://html.spec.whatwg.org/
- CSS: https://www.w3.org/Style/CSS/
- DOM: https://dom.spec.whatwg.org/
- WebIDL: https://webidl.spec.whatwg.org/

### Tools
- GDB/LLDB: `./Meta/ladybird.py debug ladybird`
- ASAN/UBSAN: `BUILD_PRESET=Sanitizers`
- Valgrind: Memory leak detection
- Callgrind: Performance profiling
- Web Platform Tests: `./Meta/WPT.sh`

### Getting Help
- Check existing fixes for similar bugs
- Search GitHub issues and PRs
- Ask in Ladybird Discord/Matrix
- Read relevant spec sections thoroughly
