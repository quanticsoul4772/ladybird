# Memory Safety Debugging Workflow

Step-by-step guide for debugging memory safety issues in Ladybird.

## Quick Start

```bash
# 1. Build with sanitizers
cmake --preset Sanitizers && cmake --build Build/sanitizers

# 2. Run and collect errors
ASAN_OPTIONS=halt_on_error=0:log_path=errors.log ./Build/sanitizers/bin/Ladybird

# 3. Analyze
./scripts/analyze_sanitizer_report.sh errors.log.*
```

## Workflow Overview

```
┌─────────────┐
│   Detect    │ ← Build with sanitizers, run tests
└──────┬──────┘
       ↓
┌─────────────┐
│  Reproduce  │ ← Create minimal test case
└──────┬──────┘
       ↓
┌─────────────┐
│  Diagnose   │ ← Analyze stack traces, understand bug
└──────┬──────┘
       ↓
┌─────────────┐
│     Fix     │ ← Apply appropriate pattern
└──────┬──────┘
       ↓
┌─────────────┐
│   Verify    │ ← Run sanitizers again, add regression test
└─────────────┘
```

## Step 1: Detect Errors

### 1.1 Build with Sanitizers

```bash
# Use Sanitizers preset (ASAN + UBSAN)
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Verify sanitizers are enabled
ldd Build/sanitizers/bin/Ladybird | grep -E 'asan|ubsan'
# Should show: libasan.so and libubsan.so
```

### 1.2 Run and Collect Errors

```bash
# Collect all errors (don't halt on first)
ASAN_OPTIONS="\
halt_on_error=0:\
detect_leaks=1:\
log_path=all_errors.log" \
./Build/sanitizers/bin/Ladybird

# Or use helper script
./scripts/run_with_asan.sh -l all_errors.log

# Run all tests
ctest --preset Sanitizers
```

### 1.3 Analyze Error Summary

```bash
# Quick summary
./scripts/analyze_sanitizer_report.sh -s all_errors.log.*

# Detailed analysis
./scripts/analyze_sanitizer_report.sh all_errors.log.*

# Filter specific error type
./scripts/analyze_sanitizer_report.sh -f use-after-free all_errors.log.*
```

**Expected Output:**
```
=== ASAN Error Summary ===
Total errors: 5

  Use-after-free:        2
  Heap buffer overflow:  1
  Stack buffer overflow: 1
  Memory leaks:          1
```

## Step 2: Reproduce Error

### 2.1 Isolate Failing Test

```bash
# If error in tests, run specific test
ASAN_OPTIONS="halt_on_error=1" ./Build/sanitizers/bin/TestIPC

# If in browser, find minimal reproduction steps
# Example: Load specific URL, click specific element
```

### 2.2 Create Minimal Test Case

```cpp
// Tests/LibWeb/TestUseAfterFree.cpp
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/HTMLElement.h>
#include <LibTest/TestCase.h>

TEST_CASE(minimal_use_after_free)
{
    // Minimal reproduction of the bug
    auto document = Web::DOM::Document::create();
    auto element = document->create_element("div");
    auto parent = document->create_element("div");

    parent->append_child(element);
    parent->remove_child(element);  // This should not crash

    // Bug: accessing element after removal
    element->update();
}
```

### 2.3 Verify Reproduction

```bash
# Build test
cmake --build Build/sanitizers --target TestUseAfterFree

# Run with ASAN
ASAN_OPTIONS="halt_on_error=1" ./Build/sanitizers/bin/TestUseAfterFree

# Should reliably reproduce the error
```

## Step 3: Diagnose Error

### 3.1 Analyze Stack Trace

**ASAN Output Example:**
```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x60700000eff8
READ of size 8 at 0x60700000eff8

    #0 Web::DOM::Node::parent() LibWeb/DOM/Node.cpp:142
    #1 Web::DOM::Element::remove() LibWeb/DOM/Element.cpp:89
    #2 Web::HTML::HTMLElement::click() LibWeb/HTML/HTMLElement.cpp:45

freed by thread T0 here:
    #0 operator delete(void*)
    #1 Web::DOM::Node::~Node() LibWeb/DOM/Node.cpp:67
    #2 Web::DOM::Document::remove_from_tree() LibWeb/DOM/Document.cpp:234
```

**Analysis Questions:**
1. **Where was it accessed?** `Node::parent()` at line 142
2. **What was accessed?** `m_parent` member (8 bytes into object)
3. **Where was it freed?** `Node::~Node()` at line 67
4. **What triggered the free?** `Document::remove_from_tree()` at line 234
5. **Time between free and use?** Same call stack (immediate UAF)

### 3.2 Examine Source Code

```bash
# View source at error location
cat -n LibWeb/DOM/Node.cpp | sed -n '135,150p'

# View with context
grep -n -A 10 -B 10 "parent()" LibWeb/DOM/Node.cpp
```

**Example Code:**
```cpp
// LibWeb/DOM/Node.cpp:142
Node* Node::parent() const {
    return m_parent.ptr();  // ← Accessing freed object's m_parent
}

// LibWeb/DOM/Element.cpp:89
void Element::remove() {
    if (auto* parent_node = parent()) {  // ← Calls parent()
        parent_node->remove_child(this);  // May delete 'this'
    }
}
```

### 3.3 Understand Ownership

**Questions to ask:**
1. Who owns this object? (RefPtr? OwnPtr? Raw pointer?)
2. When is it deleted? (RefCount hits 0? Explicit delete?)
3. Who holds references? (Parent? Children? Event listeners?)
4. Are there weak references that should prevent deletion?

**Example Analysis:**
- `Node` is `RefCounted`
- `parent()` returns raw `Node*`
- `remove_child()` releases `RefPtr`, may delete
- After `remove_child()`, `this` might be deleted
- But we still try to use `this` after the call

### 3.4 Use GDB for Investigation

```bash
# Run with GDB
gdb --args ./Build/sanitizers/bin/Ladybird

# Break on ASAN error
(gdb) break __asan_report_error
(gdb) run

# When error fires
(gdb) bt              # Backtrace
(gdb) frame 1         # Go to Node::parent()
(gdb) print this      # Inspect 'this' pointer
(gdb) print m_parent  # Try to access member (will fail)
(gdb) info locals    # See local variables
```

## Step 4: Fix the Bug

### 4.1 Identify Fix Pattern

Based on error type, choose appropriate fix:

| Error Type | Fix Pattern |
|------------|-------------|
| Use-after-free (DOM) | Use `RefPtr`/`NonnullRefPtr` to keep alive |
| Use-after-free (callback) | Capture `NonnullRefPtr` in lambda |
| Buffer overflow | Use `Vector<T>`, add bounds check |
| Integer overflow | Use `AK::checked_add/mul` |
| Memory leak | Use `OwnPtr`/`RefPtr` |
| Reference cycle | Use `WeakPtr` for back-reference |
| Null pointer | Check `has_value()` or add null check |
| Race condition | Use `Threading::Mutex` or `Atomic<T>` |

### 4.2 Apply Fix

**Before (WRONG):**
```cpp
void Element::remove() {
    if (auto* parent_node = parent()) {
        parent_node->remove_child(this);  // May delete 'this'
        this->notify_removed();           // USE-AFTER-FREE
    }
}
```

**After (CORRECT):**
```cpp
void Element::remove() {
    // Keep 'this' alive during removal
    NonnullRefPtr<Element> protected_this = *this;

    if (auto* parent_node = parent()) {
        parent_node->remove_child(protected_this);
        protected_this->notify_removed();  // Safe
    }
}
```

### 4.3 Review Related Code

Check for similar patterns elsewhere:

```bash
# Find similar code
grep -r "remove_child" LibWeb/DOM/ | grep -v "RefPtr"

# Check all uses of the fixed function
grep -r "\.remove()" LibWeb/HTML/
```

## Step 5: Verify Fix

### 5.1 Run Sanitizers Again

```bash
# Run specific test
ASAN_OPTIONS="halt_on_error=1" ./Build/sanitizers/bin/TestUseAfterFree
# Should pass now

# Run all tests
ctest --preset Sanitizers
# No new errors

# Run browser
./Build/sanitizers/bin/Ladybird
# No errors during manual testing
```

### 5.2 Add Regression Test

```cpp
// Tests/LibWeb/TestDOMSafety.cpp
TEST_CASE(element_remove_does_not_crash)
{
    // Regression test for use-after-free in Element::remove()
    auto document = Web::DOM::Document::create();
    auto parent = document->create_element("div");
    auto child = document->create_element("span");

    parent->append_child(child);

    // This should not crash
    child->remove();
    child->update();  // Safe with NonnullRefPtr protection
}
```

### 5.3 Test Edge Cases

```bash
# Test with different sanitizer configurations

# Strict mode
ASAN_OPTIONS="strict_string_checks=1:detect_stack_use_after_return=1" \
./Build/sanitizers/bin/Ladybird

# With Valgrind (different tool, may catch different issues)
./scripts/run_with_valgrind.sh TestUseAfterFree
```

## Common Debugging Scenarios

### Scenario 1: Use-After-Free in Tests

```bash
# 1. Test fails with ASAN
$ ctest --preset Sanitizers -R TestIPC
==12345==ERROR: AddressSanitizer: heap-use-after-free

# 2. Run test with halt-on-error
$ ASAN_OPTIONS="halt_on_error=1" ./Build/sanitizers/bin/TestIPC

# 3. Analyze stack trace
# Identify: Decoder accessing freed Connection

# 4. Fix: Keep Connection alive
NonnullOwnPtr<Connection> protected_conn = move(m_connection);
decoder.decode();  // Safe

# 5. Verify
$ ./Build/sanitizers/bin/TestIPC
# Test passes
```

### Scenario 2: Memory Leak in Browser

```bash
# 1. Run with leak detection
$ ASAN_OPTIONS="detect_leaks=1" ./Build/sanitizers/bin/Ladybird
# Use browser, close
==12345==ERROR: LeakSanitizer: detected memory leaks

# 2. Analyze leak report
# Shows: ResourceLoader* leaked in Document::load_resource()

# 3. Fix: Use OwnPtr
OwnPtr<ResourceLoader> m_loader;
m_loader = make<ResourceLoader>();

# 4. Verify
$ ASAN_OPTIONS="detect_leaks=1" ./Build/sanitizers/bin/Ladybird
# No leaks
```

### Scenario 3: Integer Overflow in IPC

```bash
# 1. UBSAN detects overflow
$ ./Build/sanitizers/bin/RequestServer
runtime error: signed integer overflow: 2147483647 + 1

# 2. Locate in IPC handler
# IPC::Decoder::decode<int>() at line 45

# 3. Fix: Use checked arithmetic
auto result = AK::checked_add(value, 1);
if (result.has_overflow())
    return Error::from_string_literal("Overflow");

# 4. Test with fuzzing
$ ./Build/sanitizers/bin/FuzzIPCMessages corpus/
# No overflows
```

### Scenario 4: Race Condition

```bash
# 1. Build with TSAN
$ cmake -B Build/tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread"
$ cmake --build Build/tsan

# 2. Run with TSAN
$ ./Build/tsan/bin/Ladybird
WARNING: ThreadSanitizer: data race

# 3. Analyze race report
# Shows: Write in ResourceLoader::did_load, Read in is_loading()

# 4. Fix: Add mutex
Threading::Mutex m_mutex;
Threading::MutexLocker lock(m_mutex);

# 5. Verify
$ ./Build/tsan/bin/Ladybird
# No races
```

## Debugging Checklist

- [ ] Build with Sanitizers preset
- [ ] Reproduce error reliably
- [ ] Create minimal test case
- [ ] Analyze stack trace (use, free, alloc sites)
- [ ] Examine source code at error locations
- [ ] Understand object ownership
- [ ] Identify appropriate fix pattern
- [ ] Apply fix
- [ ] Run sanitizers again
- [ ] Add regression test
- [ ] Check for similar issues in codebase
- [ ] Document the fix (if non-obvious)

## Tips and Tricks

### Fast Iteration

```bash
# Build only changed files
cmake --build Build/sanitizers --target TestIPC

# Run single test quickly
./Build/sanitizers/bin/TestIPC --test=specific_test
```

### Better Stack Traces

```bash
# More stack frames
export ASAN_OPTIONS="malloc_context_size=50"

# Slower but more accurate unwinding
export ASAN_OPTIONS="fast_unwind_on_malloc=0"
```

### Debugging in CI

```bash
# Add -DCMAKE_BUILD_TYPE=RelWithDebInfo to CI
# Keeps symbols for better stack traces

# Save ASAN logs as artifacts
ASAN_OPTIONS="log_path=ci_asan.log" ctest
# Upload ci_asan.log.* files
```

### When Sanitizers Disagree

```bash
# ASAN reports leak, but code looks correct
# → Check for reference cycles (ASAN can't detect)
# → Use heap profiler

# Valgrind reports leak, ASAN doesn't
# → Valgrind more sensitive to "reachable" memory
# → Check if intentional (global cache, etc.)

# TSAN reports race, but mutex is used
# → Check lock coverage (lock too late?)
# → Check for missing locks on read path
```

## Advanced Techniques

### Heap Profiling for Leaks

```bash
# Use massif to find growing memory
./scripts/run_with_valgrind.sh -t massif Ladybird

# Visualize
ms_print massif.out.* > heap_growth.txt
```

### Performance Profiling for Hotspots

```bash
# Use callgrind
./scripts/run_with_valgrind.sh -t callgrind Ladybird

# Visualize with kcachegrind
kcachegrind callgrind.out.*
```

### Core Dump Analysis

```bash
# Enable core dumps
ulimit -c unlimited

# Run until crash
./Build/sanitizers/bin/Ladybird

# Analyze core
gdb ./Build/sanitizers/bin/Ladybird core
(gdb) bt
(gdb) info locals
```

## Resources

- See `common-errors.md` for error catalog
- See `sanitizer-options.md` for configuration
- See `ci-sanitizer-integration.md` for CI setup
- See examples/ directory for annotated reports
