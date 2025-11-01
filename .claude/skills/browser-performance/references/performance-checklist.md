# Performance Review Checklist for Ladybird

Use this checklist before committing performance-critical code.

## Pre-Commit Checklist

### Profiling
- [ ] Profiled code with perf/callgrind before changes
- [ ] Profiled code with perf/callgrind after changes
- [ ] Generated flame graphs for visual comparison
- [ ] Identified top 10 hotspots and verified optimization targets
- [ ] No new unexpected hotspots introduced

### Benchmarking
- [ ] Created/updated relevant benchmarks
- [ ] Ran benchmarks before changes (baseline)
- [ ] Ran benchmarks after changes
- [ ] Verified no performance regressions (< 5% threshold)
- [ ] Documented performance improvements (if any)
- [ ] Updated benchmark baseline if intentional changes

### Memory Analysis
- [ ] Profiled with heaptrack/massif
- [ ] Verified no memory leaks (ASAN)
- [ ] Checked allocation count didn't increase
- [ ] Verified peak memory usage didn't increase
- [ ] No temporary allocations in hot paths

### Code Quality
- [ ] Reviewed hot paths for optimization opportunities
- [ ] Used StringBuilder for string concatenation
- [ ] Reserved vector capacity where size is known
- [ ] Avoided unnecessary string/container copies
- [ ] Used StringView for temporary strings
- [ ] Minimized virtual function calls in loops
- [ ] Cached expensive computations where appropriate
- [ ] Batched operations to reduce overhead

## Detailed Reviews

### String Operations
- [ ] No string concatenation in loops (use StringBuilder)
- [ ] StringView used for read-only string operations
- [ ] String literals use "..."sv suffix
- [ ] No temporary String objects in hot paths
- [ ] Format strings use String::formatted() or StringBuilder

**Example:**
```cpp
// BAD
String result;
for (auto& item : items) {
    result = result + item;  // O(n²)
}

// GOOD
StringBuilder builder;
for (auto& item : items) {
    builder.append(item);    // O(n)
}
String result = builder.to_string();
```

### Container Operations
- [ ] Vector capacity reserved when size is known
- [ ] No repeated HashMap lookups for same key
- [ ] Move semantics used for large objects
- [ ] Const references used in range-based for loops
- [ ] Appropriate container chosen (Vector vs HashMap vs HashSet)

**Example:**
```cpp
// BAD
Vector<int> numbers;
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);  // Multiple reallocations
}

// GOOD
Vector<int> numbers;
numbers.ensure_capacity(10000);  // Single allocation
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);
}
```

### Caching
- [ ] Expensive computations cached when appropriate
- [ ] Cache invalidation strategy in place
- [ ] Cache hit rate monitored (if critical)
- [ ] No unbounded cache growth
- [ ] Thread safety considered for shared caches

**Example:**
```cpp
// GOOD
class Element {
    CSS::ComputedStyle const& computed_style() const {
        if (!m_cached_style.has_value()) {
            m_cached_style = compute_style();
        }
        return m_cached_style.value();
    }

    void invalidate_style() {
        m_cached_style.clear();
    }

private:
    mutable Optional<CSS::ComputedStyle> m_cached_style;
};
```

### Layout/Rendering
- [ ] No layout thrashing (interleaved reads/writes)
- [ ] Batch DOM reads before writes
- [ ] Invalidate only affected regions (not entire page)
- [ ] Use DocumentFragment for batch insertions
- [ ] Minimize forced synchronous layouts

**Example:**
```cpp
// BAD: Layout thrashing
for (auto* element : elements) {
    auto width = element->offset_width();  // Force layout
    element->set_width(width + 10);         // Invalidate layout
}

// GOOD: Batch reads, then batch writes
Vector<float> widths;
for (auto* element : elements) {
    widths.append(element->offset_width());  // Single layout
}
for (size_t i = 0; i < elements.size(); ++i) {
    elements[i]->set_width(widths[i] + 10);
}
```

### IPC Communication
- [ ] Messages batched when possible
- [ ] No chatty IPC (multiple calls in loop)
- [ ] Async IPC with coalescing for frequent updates
- [ ] Message size minimized
- [ ] Unnecessary roundtrips eliminated

**Example:**
```cpp
// BAD: N IPC calls
for (auto& cookie : cookies) {
    ipc_client.set_cookie(cookie);
}

// GOOD: 1 IPC call
ipc_client.set_cookies(cookies);
```

### Memory Efficiency
- [ ] No memory leaks (verified with ASAN)
- [ ] Temporary allocations minimized
- [ ] Large objects moved instead of copied
- [ ] Immutable data shared (RefPtr/NonnullRefPtr)
- [ ] Lazy initialization used for optional features

**Example:**
```cpp
// GOOD: Lazy initialization
class Document {
    DocumentStatistics const& statistics() const {
        if (!m_statistics.has_value()) {
            m_statistics = compute_statistics();
        }
        return m_statistics.value();
    }

private:
    mutable Optional<DocumentStatistics> m_statistics;
};
```

### Algorithm Complexity
- [ ] Time complexity documented for non-trivial algorithms
- [ ] No accidental O(n²) algorithms in large datasets
- [ ] Appropriate data structure for use case
- [ ] Binary search used for sorted data
- [ ] HashMap used for frequent lookups

**Complexity Reference:**
- Vector append: O(1) amortized, O(n) worst case
- HashMap lookup: O(1) average, O(n) worst case
- Linear search: O(n)
- Binary search: O(log n)
- Sorting: O(n log n)

### Multi-threading
- [ ] CPU-bound work parallelized where beneficial
- [ ] No data races (verified with TSAN if available)
- [ ] Thread pool used instead of creating threads in loops
- [ ] Lock contention minimized
- [ ] Parallel overhead doesn't exceed benefits

## Performance Testing

### Load Testing
- [ ] Tested with simple pages (fast baseline)
- [ ] Tested with complex pages (stress test)
- [ ] Tested with real-world websites
- [ ] Tested with large DOM trees (10,000+ elements)
- [ ] Tested with large stylesheets (1,000+ rules)

### Regression Testing
- [ ] Automated benchmarks run before/after
- [ ] No regressions in existing benchmarks
- [ ] New benchmarks added for new features
- [ ] Performance documented in commit message
- [ ] Baseline updated if intentional changes

## Documentation

### Code Comments
- [ ] Performance-critical sections marked with comments
- [ ] Algorithm complexity documented
- [ ] Cache invalidation strategy explained
- [ ] Threading assumptions documented
- [ ] Optimization rationale explained

**Example:**
```cpp
// PERF: Cache computed styles to avoid expensive recalculation.
// Invalidate when element attributes or stylesheets change.
Optional<CSS::ComputedStyle> m_cached_style;
```

### Commit Message
- [ ] Performance impact mentioned if significant
- [ ] Benchmark results included
- [ ] Profiling methodology described
- [ ] Breaking changes noted
- [ ] Future optimization opportunities noted

**Example:**
```
LibWeb: Optimize CSS selector matching with cache

Implement selector compilation cache to avoid re-parsing identical
selectors. Reduces CSS selector matching time by 40% on complex pages
with many repeated selectors.

Benchmark results:
  selector_matching: 150ms → 90ms (-40%)
  page_load: 2.5s → 2.1s (-16%)

Profiled with perf on real-world websites. Flame graphs show
significant reduction in parse_selector() calls.
```

## Warning Signs

Watch out for these performance anti-patterns:

### High CPU Usage
- [ ] No tight loops without termination condition
- [ ] No excessive string allocations in loops
- [ ] No repeated expensive operations
- [ ] No excessive virtual function calls
- [ ] No busy-waiting (use proper synchronization)

### High Memory Usage
- [ ] No unbounded container growth
- [ ] No memory leaks
- [ ] No excessive temporary allocations
- [ ] No large objects on stack
- [ ] No cache without size limits

### Long Load Times
- [ ] No synchronous I/O on main thread
- [ ] No unnecessary network requests
- [ ] No blocking operations in critical path
- [ ] No sequential operations that could be parallel
- [ ] No forced synchronous layouts

### UI Freezing
- [ ] No long-running operations on main thread
- [ ] No blocking IPC calls in UI code
- [ ] No synchronous file I/O
- [ ] No expensive computations without yielding
- [ ] No large DOM manipulations without batching

## Tools to Use

### Before Committing
```bash
# CPU profiling
perf record -F 99 -g -- ./Build/release/bin/Ladybird
perf report

# Memory profiling
heaptrack ./Build/release/bin/Ladybird
heaptrack_gui heaptrack.*.gz

# Leak detection
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Benchmarks
./Build/release/bin/BenchmarkRunner
```

### For Analysis
```bash
# Flame graphs
perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg

# Call graphs
valgrind --tool=callgrind ./Build/release/bin/Ladybird
kcachegrind callgrind.out

# Compare profiles
./scripts/compare_profiles.sh perf.data.old perf.data.new
```

## Sign-off

Performance review completed by: _______________

Date: _______________

- [ ] All checklist items reviewed
- [ ] Performance impact assessed
- [ ] No significant regressions introduced
- [ ] Optimizations verified with profiling
- [ ] Ready for commit
