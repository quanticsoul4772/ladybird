---
name: performance-optimization
description: Systematic workflow for performance profiling and optimization
applies_to:
  - Page load performance
  - JavaScript execution
  - Rendering pipeline
  - Memory usage
  - IPC overhead
  - Parser performance
  - Layout computation
agents:
  - performance
  - reviewer
skills:
  - browser-performance
  - libweb-patterns
  - libjs-patterns
  - ladybird-cpp-patterns
---

# Performance Optimization Workflow

## Overview

Systematic methodology for identifying, analyzing, and resolving performance bottlenecks in Ladybird browser. This workflow ensures data-driven optimization with verification at each step.

**Core Principle**: Always profile before and after changes. Never optimize based on assumptions.

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Profile    │ → │   Analyze    │ → │   Optimize   │ → │    Verify    │
│ (CPU/Memory) │    │  (Hotspots)  │    │   (Change)   │    │ (Benchmark)  │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
        ↑                                                             ↓
        └─────────────────────────────────────────────────────────────┘
                    (Insufficient improvement? Re-profile)
```

## Prerequisites

Before starting performance optimization:

### 1. Establish Baseline
```bash
# Build optimized release build
cmake --preset Release
cmake --build Build/release

# Measure baseline performance
perf record -F 99 -g -- ./Build/release/bin/Ladybird https://example.com
perf report --stdio > baseline_profile.txt

# Save baseline data
cp perf.data baseline.perf.data
```

### 2. Create Reproducible Test Case
```bash
# For page load issues
echo "https://slow-page.com" > test_urls.txt

# For JS performance issues
cat > test_script.html << 'EOF'
<!DOCTYPE html>
<html>
<script>
// Reproducible JS workload
for (let i = 0; i < 100000; i++) {
    document.createElement('div');
}
</script>
</html>
EOF
```

### 3. Ensure Clean Environment
```bash
# Clear caches
rm -rf ~/.cache/Ladybird

# Rebuild from clean state
rm -rf Build/release
cmake --preset Release
cmake --build Build/release
```

### 4. Document Performance Goals
```
Target Metrics:
- Page load time: < 2 seconds (currently 5s)
- First paint: < 500ms (currently 1.2s)
- JavaScript execution: < 100ms (currently 350ms)
- Memory usage: < 200MB (currently 450MB)
```

## Phase 1: Profiling

### 1.1 Choose Profiling Tool

Select tool based on issue type:

**CPU Performance Issues:**
```bash
# Linux: perf (low overhead, production-safe)
perf record -F 99 -g -- ./Build/release/bin/Ladybird

# macOS: Instruments
xcrun xctrace record --template 'Time Profiler' \
    --launch ./Build/release/bin/Ladybird.app \
    --output ladybird.trace

# Detailed call graph: Valgrind Callgrind (high overhead)
valgrind --tool=callgrind ./Build/release/bin/Ladybird
```

**Memory Issues:**
```bash
# Heap allocations: Heaptrack
heaptrack ./Build/release/bin/Ladybird

# Memory snapshots: Valgrind Massif
valgrind --tool=massif ./Build/release/bin/Ladybird

# Memory leaks: ASAN
cmake --preset Sanitizers
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird
```

### 1.2 Collect Profiling Data

**Page Load Profiling:**
```bash
# Profile complete page load cycle
perf record -F 999 -g --call-graph=dwarf \
    -- ./Build/release/bin/Ladybird https://test-page.com

# Wait for page load to complete (observe UI)
# Close browser when loaded

# Verify data collected
perf report --stdio | head -50
```

**Targeted Profiling:**
```bash
# Profile specific process (WebContent)
# Launch Ladybird first, then:
perf record -F 999 -g -p $(pgrep WebContent)

# Profile for specific duration
timeout 30 perf record -F 999 -g -p $(pgrep WebContent)
```

**Memory Profiling:**
```bash
# Track all allocations
heaptrack ./Build/release/bin/Ladybird

# This creates: heaptrack.ladybird.XXXXX.gz
# Open GUI when done:
heaptrack_gui heaptrack.ladybird.*.gz
```

### 1.3 Generate Visualizations

**Flame Graphs (CPU):**
```bash
# Install FlameGraph tools
git clone https://github.com/brendangregg/FlameGraph
export PATH=$PATH:$PWD/FlameGraph

# Generate SVG flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg

# Open in browser
firefox flame.svg
```

**Call Graph (Detailed):**
```bash
# After callgrind profiling
kcachegrind callgrind.out
```

**Memory Timeline:**
```bash
# After massif profiling
ms_print massif.out | less

# Or GUI:
massif-visualizer massif.out
```

## Phase 2: Analysis

### 2.1 Identify Hotspots

**CPU Hotspots:**
```bash
# Find top 20 functions by CPU time
perf report --stdio --sort comm,dso,symbol | head -40

# Look for:
# - Functions consuming >5% CPU
# - Unexpected library calls
# - Repeated identical functions
```

**Flame Graph Analysis:**
- **Wide flames**: Functions consuming significant CPU time
- **Deep stacks**: Excessive recursion or call depth
- **Plateaus**: Tight loops or repeated operations
- **Unexpected colors**: Third-party library overhead

**Memory Hotspots:**
```bash
# Heaptrack GUI shows:
# - Peak heap usage
# - Allocation count (high count = performance issue)
# - Temporary allocations (optimization candidates)
# - Top allocating functions
```

### 2.2 Understand Bottlenecks

**Common Browser Bottlenecks:**

| Symptom | Likely Cause | Tool |
|---------|--------------|------|
| Slow page load | Network, parser, or layout | perf + flame graph |
| High CPU usage | Hot loop, inefficient algorithm | perf + annotation |
| High memory | Memory leaks, excessive allocations | Heaptrack |
| UI freezing | Synchronous operations on main thread | perf + timeline |
| Slow JS execution | Interpreter overhead, no JIT | perf + JS profiling |
| Slow rendering | Layout thrashing, excessive repaints | perf + paint profiling |

**Root Cause Analysis:**
```bash
# Annotate hot function to see expensive instructions
perf annotate <function_name>

# Check if bottleneck is:
# - Algorithm complexity (O(n²) loops)
# - String allocations
# - HashMap lookups
# - Virtual function calls
# - Cache misses
# - Branch mispredictions
```

### 2.3 Prioritize Optimization Targets

Use Amdahl's Law to estimate maximum speedup:

```
Speedup = 1 / ((1 - P) + P/S)

P = Proportion of time in target code
S = Speedup factor for target code
```

**Example Prioritization:**
```
Function A: 40% total time → Max 1.67x speedup even with 100% optimization
Function B: 10% total time → Max 1.11x speedup even with 100% optimization
Function C: 5% total time  → Max 1.05x speedup even with 100% optimization

Priority: Optimize A first
```

### 2.4 Estimate Impact

**Before Optimization:**
```bash
# Create benchmark for hot function
# Tests/Benchmarks/DOMBenchmark.cpp

BENCHMARK_CASE(dom_query_selector) {
    auto document = create_document_with_1000_elements();

    BENCHMARK_LOOP {
        auto element = document->query_selector(".test-class");
        MUST(element);
    }
}

# Run benchmark
./Build/release/bin/BenchmarkRunner --filter=dom_query_selector
# Output: 150ms average
```

**Estimate:**
- Current: 150ms per operation
- Target: 50ms per operation (3x speedup)
- Impact: 100ms saved per operation
- Overall page impact: If called 10 times, saves 1 second

## Phase 3: Hypothesis

### 3.1 Form Optimization Hypothesis

Document your hypothesis before making changes:

```markdown
## Optimization Hypothesis

**Problem**: HTMLElement::query_selector is slow (150ms for 1000 elements)

**Root Cause**: Linear search through all elements for each selector

**Proposed Solution**: Cache selector results in HashMap keyed by selector string

**Expected Impact**: 3x speedup (150ms → 50ms)

**Risk Assessment**: Low - caching is transparent to callers

**Tradeoffs**: Increased memory usage (~1KB per cached selector)
```

### 3.2 Identify Optimization Technique

Choose appropriate technique:

**Algorithmic Optimization:**
- Change O(n²) → O(n log n) or O(n)
- Example: Sort + binary search instead of linear search

**Caching:**
- Compute once, reuse result
- Example: Cache compiled selectors, parsed CSS, bytecode

**Lazy Evaluation:**
- Defer computation until actually needed
- Example: Lazy layout, lazy image loading

**Batching:**
- Group operations to reduce overhead
- Example: Batch IPC messages, batch DOM updates

**Parallelization:**
- Use multiple cores
- Example: Parallel stylesheet parsing, parallel image decoding

**Memory Optimization:**
- Reduce allocations, use arena allocators
- Example: StringBuilder instead of string concatenation

### 3.3 Estimate Difficulty and Risk

**Difficulty Levels:**
- **Low**: Simple caching, reserve vector capacity (1-2 hours)
- **Medium**: Algorithm change, refactor hot path (1-2 days)
- **High**: Architectural change, parallelization (1-2 weeks)

**Risk Levels:**
- **Low**: Local optimization, no API changes
- **Medium**: Changes internal implementation
- **High**: Changes public API, multi-process coordination

## Phase 4: Implementation

### 4.1 Apply Optimization

**Example: String Allocation Optimization**

Before (inefficient):
```cpp
String build_html() {
    String result;
    for (size_t i = 0; i < 1000; ++i) {
        result = result + "<div>" + String::number(i) + "</div>";  // Reallocates every iteration
    }
    return result;
}
```

After (optimized):
```cpp
String build_html() {
    StringBuilder builder;
    builder.ensure_capacity(1000 * 20);  // Pre-allocate
    for (size_t i = 0; i < 1000; ++i) {
        builder.append("<div>"sv);
        builder.append(String::number(i));
        builder.append("</div>"sv);
    }
    return builder.to_string();
}
```

**Example: Algorithm Optimization**

Before (O(n²)):
```cpp
Vector<Element*> find_matching_elements(StringView selector) {
    Vector<Element*> matches;
    for (auto* element : all_elements()) {
        if (matches_selector(element, selector)) {  // O(n)
            matches.append(element);
        }
    }
    return matches;
}
```

After (O(n) with caching):
```cpp
class SelectorCache {
    HashMap<String, Vector<Element*>> m_cache;

public:
    Vector<Element*> find_matching_elements(StringView selector) {
        if (auto cached = m_cache.get(selector))
            return cached.value();

        Vector<Element*> matches;
        for (auto* element : all_elements()) {
            if (matches_selector(element, selector)) {
                matches.append(element);
            }
        }

        m_cache.set(selector, matches);
        return matches;
    }

    void invalidate() { m_cache.clear(); }
};
```

### 4.2 Maintain Correctness

**Verification Checklist:**
- [ ] All tests pass (run full test suite)
- [ ] No new ASAN/UBSAN errors
- [ ] No memory leaks (check with ASAN)
- [ ] Behavior unchanged (verify with LibWeb tests)
- [ ] Edge cases handled (null checks, empty inputs)

```bash
# Run all tests
./Meta/ladybird.py test

# Run with sanitizers
cmake --preset Sanitizers
cmake --build Build/sanitizers
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/TestSuite
```

### 4.3 Follow Coding Patterns

**Ladybird C++ Patterns:**
```cpp
// Use ErrorOr for fallible operations
ErrorOr<NonnullOwnPtr<Cache>> create_cache() {
    auto cache = TRY(Cache::create());
    return cache;
}

// Use TRY for error propagation
ErrorOr<void> process() {
    auto result = TRY(fallible_operation());
    return {};
}

// Reserve vector capacity
Vector<Element*> elements;
elements.ensure_capacity(1000);  // Avoid reallocations

// Use StringView for temporaries
void process_string(StringView str);  // No allocation
process_string("literal"sv);

// East const style
Element const& get_element() const;

// Member variable prefix
class Cache {
    HashMap<String, Value> m_cache;  // m_ prefix
    size_t m_size { 0 };              // Initialize at definition
};
```

### 4.4 Document Tradeoffs

Add comments explaining optimization decisions:

```cpp
// OPTIMIZATION: Cache selector results to avoid repeated queries.
// Tradeoff: Uses ~1KB memory per unique selector.
// Invalidated on DOM mutation to ensure correctness.
class SelectorCache {
    // Cache maps selector string → matching elements
    HashMap<String, Vector<Element*>> m_cache;

public:
    // Fast path: O(1) cache lookup instead of O(n) scan
    Vector<Element*> find_matching_elements(StringView selector) {
        if (auto cached = m_cache.get(selector))
            return cached.value();

        // Slow path: Compute and cache
        auto matches = compute_matches(selector);
        m_cache.set(selector, matches);
        return matches;
    }

    // Must be called when DOM mutates
    void invalidate() { m_cache.clear(); }
};
```

## Phase 5: Measurement

### 5.1 Re-Profile with Same Methodology

**Critical**: Use identical profiling methodology for before/after comparison.

```bash
# After optimization, profile again with SAME command
perf record -F 99 -g -- ./Build/release/bin/Ladybird https://test-page.com

# Save new data
mv perf.data optimized.perf.data
```

### 5.2 Compare Before/After

**Generate Differential Flame Graph:**
```bash
# Baseline
perf script -i baseline.perf.data | stackcollapse-perf.pl > baseline.folded

# Optimized
perf script -i optimized.perf.data | stackcollapse-perf.pl > optimized.folded

# Differential (shows what got faster/slower)
difffolded.pl baseline.folded optimized.folded | flamegraph.pl > diff.svg

# Red = slower, blue = faster
firefox diff.svg
```

**Numeric Comparison:**
```bash
# Compare profiles
perf diff baseline.perf.data optimized.perf.data

# Output shows:
# +5.00%  function_that_got_slower
# -10.00% function_that_got_faster (our target!)
```

### 5.3 Measure Improvement

**Benchmark Comparison:**
```bash
# Run benchmark suite
./Build/release/bin/BenchmarkRunner --json > before.json

# Apply optimization, rebuild
# ...

./Build/release/bin/BenchmarkRunner --json > after.json

# Compare results
./scripts/compare_benchmarks.py before.json after.json
```

**Manual Timing:**
```cpp
// Add timing instrumentation
auto start = MonotonicTime::now();
perform_operation();
auto end = MonotonicTime::now();
auto duration = end - start;
dbgln("Operation took {}ms", duration.to_milliseconds());
```

### 5.4 Check for Regressions

**Performance Regressions:**
```bash
# Check if any other functions got slower
perf diff baseline.perf.data optimized.perf.data | grep "^+[0-9]"

# If unrelated functions are slower, investigate why
```

**Memory Regressions:**
```bash
# Compare memory usage
heaptrack_print heaptrack.before.gz | grep "peak heap memory"
heaptrack_print heaptrack.after.gz | grep "peak heap memory"

# Should not increase significantly
```

## Phase 6: Validation

### 6.1 Run All Tests

```bash
# Full test suite
./Meta/ladybird.py test

# LibWeb tests (most comprehensive)
./Meta/ladybird.py run test-web

# Specific test patterns
./Meta/ladybird.py test ".*CSS.*"
./Meta/ladybird.py test LibJS
```

### 6.2 Verify Correctness

**Automated Verification:**
```bash
# Run with sanitizers to catch bugs
cmake --preset Sanitizers
cmake --build Build/sanitizers

ASAN_OPTIONS=detect_leaks=1 \
UBSAN_OPTIONS=print_stacktrace=1 \
    ./Build/sanitizers/bin/Ladybird
```

**Manual Verification:**
1. Load test pages that exercise optimized code path
2. Verify visual correctness (screenshots, ref tests)
3. Check browser console for errors
4. Test edge cases (empty inputs, large inputs, null values)

### 6.3 Check Memory Usage

**Memory Leak Check:**
```bash
# ASAN should report no leaks
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Exit browser normally
# Check ASAN output for "LeakSanitizer: detected memory leaks"
# Should be zero leaks
```

**Allocation Increase:**
```bash
# Compare allocation counts
heaptrack_print heaptrack.before.gz | grep "allocations:"
heaptrack_print heaptrack.after.gz | grep "allocations:"

# Should decrease or stay same (not increase)
```

### 6.4 Ensure No New Bugs

**Regression Testing:**
```bash
# Run existing bug tests
./Meta/ladybird.py run test-web -- -f Text/input/regression-*.html

# Check for new crashes
./Build/release/bin/Ladybird https://browserleaks.com
./Build/release/bin/Ladybird https://html5test.com
```

**Code Review Checklist:**
- [ ] No new undefined behavior (check UBSAN)
- [ ] No new data races (check TSAN if multi-threaded)
- [ ] No integer overflows (check SafeMath usage)
- [ ] No unhandled error cases (check ErrorOr usage)

## Phase 7: Iteration

### 7.1 If Insufficient Improvement

**Actual improvement < Expected:**

```
Expected: 3x speedup (150ms → 50ms)
Actual: 1.5x speedup (150ms → 100ms)
```

**Actions:**
1. Re-profile to find remaining bottleneck
2. Check if optimization hypothesis was incorrect
3. Consider alternative optimization approaches
4. Return to Phase 2: Analysis

**Example Iteration:**
```bash
# Re-profile optimized version
perf record -F 999 -g -- ./Build/release/bin/Ladybird

# Find new hotspot
perf report --stdio | head -20

# If new bottleneck appears: optimize that next
# If same bottleneck: try different optimization approach
```

### 7.2 If Regressions Detected

**Other code paths got slower:**

**Actions:**
1. Identify regressed functions (perf diff)
2. Understand why (increased cache misses? More allocations?)
3. Adjust optimization to avoid regression
4. Return to Phase 4: Implementation

**Example:**
```bash
# Find regressions
perf diff baseline.perf.data optimized.perf.data | grep "^+[5-9]"

# Output:
# +8.00% other_function

# Investigate why other_function got slower
perf annotate other_function

# Possible causes:
# - Cache eviction from new data structures
# - Increased memory usage causing swapping
# - Lock contention if multi-threaded
```

### 7.3 If Successful

**Improvement meets or exceeds target:**

```
Expected: 3x speedup (150ms → 50ms)
Actual: 4x speedup (150ms → 37ms)
✓ Success!
```

**Actions:**
1. Document optimization (see Phase 7.4)
2. Create commit (see Phase 7.4)
3. Update benchmark baseline
4. Move to next optimization target

### 7.4 Document and Commit

**Commit Message Format:**
```
Category: Brief imperative description

Detailed explanation of optimization including:
- Problem: What was slow
- Root cause: Why it was slow
- Solution: How it was fixed
- Impact: Measured improvement

Performance impact:
- Function X: 150ms → 37ms (4x faster)
- Page load: 5s → 3.5s (30% improvement)
- Memory: 450MB → 420MB (7% reduction)
```

**Example Commit:**
```
LibWeb: Optimize querySelector with selector caching

The querySelector function was performing O(n) element scans on
every call, even for repeated selectors. Profiling showed this
consumed 40% of page load time on selector-heavy pages.

This commit adds a SelectorCache that maps selector strings to
matching element vectors. The cache is invalidated on DOM mutation
to ensure correctness. Pre-allocates cache capacity based on
typical page selector count.

Performance impact (1000-element document, 100 queries):
- querySelector: 150ms → 37ms (4x faster)
- Page load (complex page): 5s → 3.5s (30% faster)
- Memory overhead: +1KB per unique selector (~20KB typical page)
```

## Optimization Patterns by Category

### String and Memory Optimizations

**Problem: Excessive String Allocations**
```cpp
// SLOW: Creates temporary strings
String result;
for (size_t i = 0; i < 1000; ++i) {
    result = result + " " + String::number(i);  // Reallocates every time
}

// FAST: Use StringBuilder
StringBuilder builder;
builder.ensure_capacity(5000);  // Pre-allocate
for (size_t i = 0; i < 1000; ++i) {
    builder.append(' ');
    builder.append(String::number(i));
}
String result = builder.to_string();  // Single allocation
```

**Problem: Vector Reallocations**
```cpp
// SLOW: Multiple reallocations
Vector<int> numbers;
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);  // Reallocates ~14 times
}

// FAST: Reserve capacity
Vector<int> numbers;
numbers.ensure_capacity(10000);  // Single allocation
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);  // No reallocations
}
```

**Problem: Unnecessary Copies**
```cpp
// SLOW: Copies entire vector
void process_data(Vector<Element*> elements) {
    // ...
}

// FAST: Pass by reference
void process_data(Vector<Element*> const& elements) {
    // ...
}
```

**Problem: Temporary StringView Allocations**
```cpp
// SLOW: Allocates temporary String
element->set_attribute("class", String("active"));

// FAST: Use StringView
element->set_attribute("class", "active"sv);
```

### Layout and Rendering Optimizations

**Problem: Layout Thrashing (Forced Synchronous Layout)**
```cpp
// SLOW: Causes multiple layouts
for (auto& element : elements) {
    auto width = element.offset_width();  // Forces layout
    element.set_inline_style("width", String::formatted("{}px", width + 10));  // Invalidates layout
    // Next iteration: Forces layout again!
}

// FAST: Batch reads, then batch writes
Vector<int> widths;
for (auto& element : elements) {
    widths.append(element.offset_width());  // Read phase (single layout)
}
for (size_t i = 0; i < elements.size(); ++i) {
    elements[i].set_inline_style("width", String::formatted("{}px", widths[i] + 10));  // Write phase
}
```

**Problem: Excessive Repaints**
```cpp
// SLOW: Repaints entire page every time
void update_element(Element& element) {
    element.set_needs_display();
    page().invalidate();  // Full page repaint!
}

// FAST: Invalidate only dirty region
void update_element(Element& element) {
    element.set_needs_display();
    page().invalidate_rect(element.absolute_rect());  // Partial repaint
}
```

**Problem: Unnecessary Style Recalculation**
```cpp
// Avoid:
// 1. Universal selectors (div * span)
// 2. Deep descendant combinators (body div div div span)
// 3. Attribute selectors on non-indexed attributes

// Instead: Use class selectors (fast)
.specific-class { /* ... */ }
```

### JavaScript Optimizations

**Problem: Repeated Property Lookups**
```cpp
// SLOW: Looks up property every iteration
for (size_t i = 0; i < 1000; ++i) {
    auto value = object->get_property("length");
    process(value);
}

// FAST: Cache property
auto length = object->get_property("length");
for (size_t i = 0; i < 1000; ++i) {
    process(length);
}
```

**Problem: Excessive Object Creation**
```cpp
// SLOW: Creates temporary objects
for (size_t i = 0; i < 1000; ++i) {
    auto obj = JS::Object::create();
    obj->set_property("value", i);
    process(obj);
}

// FAST: Reuse object
auto obj = JS::Object::create();
for (size_t i = 0; i < 1000; ++i) {
    obj->set_property("value", i);
    process(obj);
}
```

**Problem: Bytecode Recompilation**
```cpp
// Cache compiled bytecode
class BytecodeCache {
    HashMap<String, NonnullOwnPtr<Bytecode::Executable>> m_cache;

public:
    NonnullOwnPtr<Bytecode::Executable> compile(String const& source) {
        auto hash = calculate_hash(source);
        if (auto cached = m_cache.get(hash))
            return cached.value();

        auto executable = Bytecode::compile(source);
        m_cache.set(hash, executable);
        return executable;
    }
};
```

### Parser Optimizations

**Problem: Slow HTML Tokenization**
```cpp
// FAST: Use lookup tables instead of switch/if
static constexpr bool is_whitespace_lookup[256] = {
    // Initialize at compile time
};

bool is_whitespace(u8 c) {
    return is_whitespace_lookup[c];  // O(1) instead of branches
}
```

**Problem: Repeated String Comparisons**
```cpp
// SLOW: String comparison in hot loop
for (auto& attr : attributes) {
    if (attr.name == "class") { /* ... */ }
    if (attr.name == "id") { /* ... */ }
}

// FAST: Use FlyString (interned strings)
for (auto& attr : attributes) {
    if (attr.name == HTML::AttributeNames::class_) { /* ... */ }
    if (attr.name == HTML::AttributeNames::id) { /* ... */ }
}
```

**Problem: Sequential Parsing**
```cpp
// SLOW: Parse stylesheets sequentially
for (auto& sheet : stylesheets) {
    parse_css(sheet);  // One at a time
}

// FAST: Parse in parallel (if independent)
Vector<Thread> threads;
for (auto& sheet : stylesheets) {
    threads.append(Thread::create([&sheet] {
        parse_css(sheet);
    }));
}
for (auto& thread : threads)
    thread->join();
```

### IPC Optimizations

**Problem: Chatty IPC (Too Many Round-Trips)**
```cpp
// SLOW: One IPC call per item
for (auto& cookie : cookies) {
    client().set_cookie(cookie);  // 1000 IPC calls!
}

// FAST: Batch IPC messages
client().set_cookies(cookies);  // 1 IPC call
```

**Problem: Large IPC Messages**
```cpp
// SLOW: Send entire bitmap over IPC
client().update_bitmap(large_bitmap);  // 10MB transfer

// FAST: Use shared memory
auto shared_buffer = MUST(SharedBuffer::create(size));
render_to_buffer(shared_buffer);
client().update_bitmap_shared(shared_buffer.fd());  // Just send FD
```

**Problem: Synchronous IPC Blocking**
```cpp
// SLOW: Blocks caller
auto response = client().sync_request();  // Waits for response

// FAST: Async IPC
client().async_request([](auto response) {
    // Handle response asynchronously
});
```

### HashMap and Lookup Optimizations

**Problem: HashMap Lookups in Loops**
```cpp
// SLOW: Repeated lookups
for (size_t i = 0; i < 1000; ++i) {
    auto value = map.get("key");  // Lookup every time
    process(value);
}

// FAST: Lookup once
auto value = map.get("key");
for (size_t i = 0; i < 1000; ++i) {
    process(value);
}
```

**Problem: Small HashMap with Frequent Creation**
```cpp
// SLOW: HashMap overhead for small data
HashMap<String, int> map;  // Overhead: ~100 bytes
map.set("a", 1);
map.set("b", 2);

// FAST: Use Vector for small data
Vector<Pair<String, int>> pairs;  // Overhead: ~24 bytes
pairs.append({"a", 1});
pairs.append({"b", 2});
// Linear search is faster for <10 items
```

## Checklist: Performance Optimization

### Before Starting
- [ ] Establish baseline metrics (run benchmarks)
- [ ] Create reproducible test case
- [ ] Build optimized release build
- [ ] Document performance goals

### During Profiling
- [ ] Choose appropriate profiling tool for issue type
- [ ] Collect profiling data with sufficient samples
- [ ] Generate visualizations (flame graphs)
- [ ] Save baseline profiling data for comparison

### During Analysis
- [ ] Identify top CPU/memory hotspots
- [ ] Understand root cause (not just symptoms)
- [ ] Estimate potential impact (Amdahl's Law)
- [ ] Prioritize high-impact targets

### During Implementation
- [ ] Form clear optimization hypothesis
- [ ] Document expected improvement
- [ ] Apply appropriate optimization technique
- [ ] Maintain correctness (tests still pass)
- [ ] Follow coding patterns (TRY, ErrorOr, east const)
- [ ] Add comments explaining tradeoffs

### During Measurement
- [ ] Re-profile with identical methodology
- [ ] Generate differential flame graph
- [ ] Run benchmarks and compare results
- [ ] Check for performance regressions

### During Validation
- [ ] Run full test suite
- [ ] Run with ASAN/UBSAN (no new errors)
- [ ] Check for memory leaks
- [ ] Verify no behavior changes
- [ ] Test edge cases

### Before Committing
- [ ] Improvement meets or exceeds target
- [ ] No correctness regressions
- [ ] No performance regressions in other code
- [ ] Documentation updated
- [ ] Commit message includes performance impact

### Code Review Checklist
- [ ] Avoid string allocations in hot paths (use StringBuilder)
- [ ] Reserve vector capacity when size known
- [ ] Use const& to avoid copies
- [ ] Cache expensive lookups (HashMap, property access)
- [ ] Batch operations (IPC, DOM updates)
- [ ] Use StringView for temporary strings
- [ ] Avoid layout thrashing (batch reads/writes)
- [ ] Minimize virtual function calls in tight loops

## Performance Budget

Track performance metrics over time:

```bash
# Create performance budget file
cat > performance_budget.txt << EOF
Metric                  Budget    Current   Status
Page load (simple)      < 1s      0.8s      ✓
Page load (complex)     < 3s      3.5s      ✗
First paint            < 500ms    450ms     ✓
JavaScript execution   < 100ms    120ms     ✗
Memory (idle)          < 100MB    85MB      ✓
Memory (10 tabs)       < 500MB    650MB     ✗
EOF
```

## Quick Reference

### Profiling Commands
```bash
# CPU
perf record -F 99 -g -- ./Build/release/bin/Ladybird
./Meta/ladybird.py profile ladybird https://example.com

# Memory
heaptrack ./Build/release/bin/Ladybird

# Leaks
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Flame graph
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg
```

### Benchmark Commands
```bash
# Run benchmarks
./Build/release/bin/BenchmarkRunner

# Compare results
./scripts/compare_benchmarks.py before.json after.json
```

### Common Optimization Patterns
```cpp
// String building
StringBuilder builder;
builder.ensure_capacity(expected_size);

// Vector allocation
Vector<T> vec;
vec.ensure_capacity(known_size);

// Avoid copies
void process(Vector<T> const& data);  // Not: Vector<T> data

// Use StringView
element->set_attribute("class", "active"sv);  // Not: String("active")

// Cache lookups
auto value = map.get(key);
for (/* ... */) { use(value); }  // Not: map.get(key) in loop

// Batch operations
client().batch_update(all_items);  // Not: loop with client().update(item)
```
