---
name: browser-performance
description: Comprehensive performance profiling and optimization for Ladybird browser, covering CPU/memory profiling, benchmarking, and browser-specific optimization patterns
use-when: Investigating performance issues, optimizing page load times, reducing memory usage, profiling CPU hotspots, detecting performance regressions
allowed-tools:
  - Bash
  - Read
  - Write
  - Grep
scripts:
  - profile_page_load.sh
  - generate_flame_graph.sh
  - memory_profile.sh
  - benchmark_suite.sh
  - compare_profiles.sh
---

# Browser Performance Engineering for Ladybird

## Overview
```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Profile    │ → │   Analyze    │ → │   Optimize   │ → │    Verify    │
│ (CPU/Memory) │    │  (Hotspots)  │    │   (Change)   │    │ (Benchmark)  │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
        ↑                                                             ↓
        └─────────────────────────────────────────────────────────────┘
                    (Regression detected? Re-profile)
```

## 1. CPU Profiling Tools

### Linux perf (Production-Grade Profiling)

**Setup:**
```bash
# Install perf
sudo apt install linux-tools-common linux-tools-generic linux-tools-`uname -r`

# Allow non-root profiling
echo 0 | sudo tee /proc/sys/kernel/perf_event_paranoid
echo 0 | sudo tee /proc/sys/kernel/kptr_restrict
```

**Basic CPU Profiling:**
```bash
# Profile Ladybird loading a URL
perf record -F 99 -g -- ./Build/release/bin/Ladybird https://example.com

# View report
perf report

# Generate flame graph
perf script | ./scripts/flamegraph.pl > flamegraph.svg
```

**Advanced perf Usage:**
```bash
# Profile specific process (attach to WebContent)
perf record -F 999 -g -p $(pgrep WebContent)

# Profile with call stacks and data addresses
perf record -F 99 -g --call-graph=dwarf -- ./Build/release/bin/Ladybird

# Record branch misses (CPU pipeline stalls)
perf record -e branch-misses -g -- ./Build/release/bin/Ladybird

# Record cache misses
perf record -e cache-misses -g -- ./Build/release/bin/Ladybird

# Profile all cores
perf record -F 99 -g -a -- ./Build/release/bin/Ladybird
```

**Analyzing perf Data:**
```bash
# Top functions by CPU time
perf report --stdio | head -50

# Call graph analysis
perf report --stdio -g graph

# Annotate source code with profiling data
perf annotate -s <symbol_name>

# Generate differential profile (before/after)
perf diff perf.data.old perf.data
```

### Valgrind Callgrind (Call Graph Profiling)

**Built-in Support:**
```bash
# Ladybird has built-in callgrind support
./Meta/ladybird.py profile ladybird https://example.com

# This runs: valgrind --tool=callgrind --instr-atstart=yes ./Build/*/bin/Ladybird
```

**Manual Callgrind:**
```bash
# Profile with callgrind
valgrind --tool=callgrind \
    --callgrind-out-file=callgrind.out \
    --dump-instr=yes \
    --collect-jumps=yes \
    ./Build/release/bin/Ladybird

# Visualize with KCachegrind
kcachegrind callgrind.out
```

**Callgrind Control (Dynamic Instrumentation):**
```bash
# Start with instrumentation off
valgrind --tool=callgrind --instr-atstart=no ./Build/release/bin/Ladybird &

# Get PID
LADYBIRD_PID=$!

# Turn instrumentation on when ready
callgrind_control -i on $LADYBIRD_PID

# Dump current statistics
callgrind_control -d $LADYBIRD_PID

# Turn instrumentation off
callgrind_control -i off $LADYBIRD_PID
```

### macOS Instruments (Time Profiler)

**Command-Line Profiling:**
```bash
# Profile with Instruments Time Profiler
xcrun xctrace record --template 'Time Profiler' \
    --launch ./Build/release/bin/Ladybird.app \
    --output ladybird_profile.trace

# View in Instruments GUI
open ladybird_profile.trace
```

**Manual Profiling:**
1. Open Instruments.app
2. Choose "Time Profiler" template
3. Select Ladybird as target
4. Click Record
5. Perform operations to profile
6. Stop recording
7. Analyze call tree and heaviest stack traces

### Tracy Profiler (Real-Time Profiling)

**Setup Tracy:**
```bash
# Build Tracy server
git clone https://github.com/wolfpld/tracy.git
cd tracy/profiler/build/unix
make release
```

**Instrument Ladybird:**
```cpp
// Add to CMakeLists.txt
option(ENABLE_TRACY "Enable Tracy profiling" OFF)

if(ENABLE_TRACY)
    add_compile_definitions(TRACY_ENABLE)
    include_directories(${CMAKE_SOURCE_DIR}/ThirdParty/tracy/public)
endif()

// In performance-critical code
#include <tracy/Tracy.hpp>

void render_page() {
    ZoneScoped;  // Automatic scope profiling
    // ... rendering code ...
}

void parse_html(StringView html) {
    ZoneScopedN("HTML Parsing");  // Named zone
    // ... parsing code ...
}
```

**Run with Tracy:**
```bash
# Build with Tracy
cmake -B Build -DENABLE_TRACY=ON
cmake --build Build

# Run Tracy server
./tracy/profiler/build/unix/Tracy-release &

# Run Ladybird (connects to Tracy automatically)
./Build/release/bin/Ladybird
```

## 2. Memory Profiling Tools

### Heaptrack (Heap Memory Profiler)

**Setup:**
```bash
# Install Heaptrack
sudo apt install heaptrack heaptrack-gui
```

**Profile Memory Usage:**
```bash
# Run with heaptrack
heaptrack ./Build/release/bin/Ladybird https://example.com

# This creates: heaptrack.ladybird.XXXXX.gz

# Analyze in GUI
heaptrack_gui heaptrack.ladybird.*.gz
```

**Key Metrics to Watch:**
- **Peak heap usage**: Maximum memory allocated
- **Allocations**: Number of allocations (high count = performance issue)
- **Temporary allocations**: Short-lived allocations (candidates for optimization)
- **Leak suspects**: Memory not freed before exit

### Valgrind Massif (Heap Snapshots)

**Run Massif:**
```bash
# Profile heap memory over time
valgrind --tool=massif \
    --massif-out-file=massif.out \
    --time-unit=ms \
    --detailed-freq=1 \
    ./Build/release/bin/Ladybird

# Visualize
ms_print massif.out | less
```

**Massif Output:**
```
--------------------------------------------------------------------------------
  n        time(ms)         total(B)   useful-heap(B) extra-heap(B)    stacks(B)
--------------------------------------------------------------------------------
 10        1,000           10,485,760      10,240,000       245,760            0
 20        2,000           20,971,520      20,480,000       491,520            0
 30        3,000           15,728,640      15,360,000       368,640            0
```

**Analyze Growth:**
```bash
# Plot memory growth
massif-visualizer massif.out
```

### ASAN LeakSanitizer (Memory Leak Detection)

**Build with ASAN:**
```bash
# Ladybird has Sanitizers preset
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Run with leak detection
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird
```

**Suppress Known Leaks:**
```bash
# Create suppressions file
cat > lsan.supp << EOF
leak:third_party_library
leak:known_leak_function
EOF

# Run with suppressions
LSAN_OPTIONS=suppressions=lsan.supp ./Build/sanitizers/bin/Ladybird
```

## 3. Browser-Specific Optimization Patterns

### Page Load Performance

**Critical Path Analysis:**
```cpp
// Measure key page load milestones
class PageLoadMetrics {
public:
    void mark_navigation_start() { m_navigation_start = MonotonicTime::now(); }
    void mark_first_paint() { m_first_paint = MonotonicTime::now(); }
    void mark_dom_content_loaded() { m_dom_content_loaded = MonotonicTime::now(); }
    void mark_load_complete() { m_load_complete = MonotonicTime::now(); }

    void report() {
        auto first_paint_time = m_first_paint - m_navigation_start;
        auto dcl_time = m_dom_content_loaded - m_navigation_start;
        auto load_time = m_load_complete - m_navigation_start;

        dbgln("First Paint: {}ms", first_paint_time.to_milliseconds());
        dbgln("DOMContentLoaded: {}ms", dcl_time.to_milliseconds());
        dbgln("Load Complete: {}ms", load_time.to_milliseconds());
    }

private:
    MonotonicTime m_navigation_start;
    MonotonicTime m_first_paint;
    MonotonicTime m_dom_content_loaded;
    MonotonicTime m_load_complete;
};
```

**Optimization Checklist:**
- [ ] Reduce DNS lookups (cache DNS results)
- [ ] Minimize TCP connections (HTTP/2, connection pooling)
- [ ] Optimize resource loading order (critical CSS first)
- [ ] Lazy load images/scripts below the fold
- [ ] Enable gzip/brotli compression
- [ ] Cache parsed resources (bytecode, style sheets)

### JavaScript Execution (JIT/Bytecode)

**Hot Function Optimization:**
```cpp
// Before: Interpreted every time
JSValue slow_function(VM& vm) {
    // Complex logic executed in interpreter
    for (size_t i = 0; i < 1000000; ++i) {
        perform_operation(i);
    }
}

// After: JIT-compiled hot path
JSValue fast_function(VM& vm) {
    // JIT compiler detects hot loop
    // Generates native code after N iterations
    for (size_t i = 0; i < 1000000; ++i) {
        perform_operation(i);  // JIT-optimized
    }
}
```

**Bytecode Caching:**
```cpp
// Cache compiled bytecode
class BytecodeCache {
public:
    Optional<NonnullOwnPtr<Bytecode::Executable>> get(StringView source_hash) {
        return m_cache.get(source_hash);
    }

    void set(String source_hash, NonnullOwnPtr<Bytecode::Executable> executable) {
        m_cache.set(move(source_hash), move(executable));
    }

private:
    HashMap<String, NonnullOwnPtr<Bytecode::Executable>> m_cache;
};
```

### Rendering Pipeline (Layout/Paint)

**Layout Performance:**
```cpp
// Avoid forced synchronous layout (layout thrashing)

// BAD: Causes multiple layouts
for (size_t i = 0; i < elements.size(); ++i) {
    auto width = elements[i].offset_width();  // Forces layout
    elements[i].set_inline_style("width", String::formatted("{}px", width + 10));
}

// GOOD: Batch reads, then batch writes
Vector<int> widths;
for (auto& element : elements) {
    widths.append(element.offset_width());  // Read phase
}
for (size_t i = 0; i < elements.size(); ++i) {
    elements[i].set_inline_style("width", String::formatted("{}px", widths[i] + 10));  // Write phase
}
```

**Paint Optimization:**
```cpp
// Avoid unnecessary repaints
class PaintInvalidation {
public:
    // Track dirty regions instead of repainting entire page
    void invalidate_rect(Gfx::IntRect const& rect) {
        m_dirty_region.add_rect(rect);
    }

    void repaint() {
        // Only repaint dirty regions
        for (auto& rect : m_dirty_region.rects()) {
            paint_rect(rect);
        }
        m_dirty_region.clear();
    }

private:
    Gfx::DisjointIntRectSet m_dirty_region;
};
```

### DOM Manipulation Performance

**Efficient DOM Updates:**
```cpp
// Avoid repeated DOM queries
// BAD: O(n²) queries
for (size_t i = 0; i < 1000; ++i) {
    auto element = document.query_selector("#myElement");  // Queries every iteration
    element->set_text_content(String::number(i));
}

// GOOD: Cache DOM reference
auto element = document.query_selector("#myElement");
for (size_t i = 0; i < 1000; ++i) {
    element->set_text_content(String::number(i));
}

// BEST: Use DocumentFragment for batch insertions
auto fragment = document.create_document_fragment();
for (size_t i = 0; i < 1000; ++i) {
    auto div = document.create_element("div");
    div->set_text_content(String::number(i));
    fragment->append_child(div);
}
parent->append_child(fragment);  // Single reflow
```

### CSS Selector Matching

**Selector Performance:**
```cpp
// Selector matching is right-to-left

// SLOW: Universal selector with descendant combinator
// div * span { ... }  // Checks every element

// FAST: Specific class selector
// .specific-class { ... }

// Optimize selector matching
class SelectorMatcher {
    // Cache compiled selectors
    HashMap<String, CompiledSelector> m_selector_cache;

    bool matches(Element const& element, StringView selector) {
        auto compiled = m_selector_cache.get(selector);
        if (!compiled.has_value()) {
            compiled = compile_selector(selector);
            m_selector_cache.set(selector, compiled.value());
        }
        return compiled.value().matches(element);
    }
};
```

**Style Computation Optimization:**
```cpp
// Share computed styles for identical elements
class StyleComputer {
    // Cache computed styles by style hash
    HashMap<u64, NonnullRefPtr<CSS::StyleProperties>> m_style_cache;

    NonnullRefPtr<CSS::StyleProperties> compute_style(Element const& element) {
        auto hash = compute_style_hash(element);

        if (auto cached = m_style_cache.get(hash)) {
            return cached.value();
        }

        auto style = compute_style_uncached(element);
        m_style_cache.set(hash, style);
        return style;
    }
};
```

### Parser Performance (HTML/CSS/JS)

**Streaming Parsing:**
```cpp
// Parse incrementally as data arrives
class StreamingHTMLParser {
public:
    void append_data(ByteBuffer const& chunk) {
        m_buffer.append(chunk);

        // Parse complete tokens
        while (auto token = try_extract_token()) {
            process_token(token.value());
        }
    }

    // Don't wait for entire document
    Optional<HTMLToken> try_extract_token() {
        // Extract token if complete in buffer
        // Return empty if incomplete (wait for more data)
    }
};
```

**Parallel CSS Parsing:**
```cpp
// Parse multiple stylesheets in parallel
class ParallelCSSParser {
    Vector<NonnullOwnPtr<Thread>> parse_stylesheets(Vector<String> const& sources) {
        Vector<NonnullOwnPtr<Thread>> threads;

        for (auto& source : sources) {
            threads.append(Thread::create([&source, this] {
                auto stylesheet = parse_css(source);
                m_stylesheets.append(move(stylesheet));
            }));
        }

        for (auto& thread : threads) {
            thread->join();
        }

        return m_stylesheets;
    }
};
```

### IPC Overhead Reduction

**Batch IPC Messages:**
```cpp
// Avoid chatty IPC
// BAD: One message per operation
for (auto& cookie : cookies) {
    ipc_client.set_cookie(cookie);  // 1000 IPC calls
}

// GOOD: Batch messages
ipc_client.set_cookies(cookies);  // 1 IPC call

// Batch multiple types of operations
struct BatchedIPCMessages {
    Vector<Cookie> cookies_to_set;
    Vector<String> scripts_to_execute;
    Vector<NavigationRequest> navigations;
};

ipc_client.send_batch(batched_messages);
```

**Async IPC with Coalescing:**
```cpp
// Coalesce frequent updates
class IPCCoalescer {
    void request_update(String const& property, String const& value) {
        m_pending_updates.set(property, value);

        if (!m_timer.is_active()) {
            // Batch updates every 16ms (60fps)
            m_timer.start(16, [this] {
                flush_updates();
            });
        }
    }

    void flush_updates() {
        ipc_client.send_batched_updates(m_pending_updates);
        m_pending_updates.clear();
    }

    HashMap<String, String> m_pending_updates;
    Timer m_timer;
};
```

## 4. Benchmarking Patterns

### Creating Reproducible Benchmarks

**Page Load Benchmark:**
```cpp
// Tests/Benchmarks/PageLoadBenchmark.cpp
#include <LibTest/Benchmark.h>

BENCHMARK_CASE(load_simple_page) {
    auto page = create_test_page();

    BENCHMARK_LOOP {
        page->load_url("file:///test_pages/simple.html");
        page->wait_for_load_complete();
    }
}

BENCHMARK_CASE(load_complex_page) {
    auto page = create_test_page();

    BENCHMARK_LOOP {
        page->load_url("file:///test_pages/complex.html");
        page->wait_for_load_complete();
    }
}
```

**Microbenchmark Structure:**
```cpp
// Tests/Benchmarks/DOMBenchmark.cpp
#include <LibTest/Benchmark.h>

BENCHMARK_CASE(dom_query_selector) {
    auto document = create_document_with_1000_elements();

    BENCHMARK_LOOP {
        auto element = document->query_selector(".test-class");
        MUST(element);  // Prevent optimization
    }
}

BENCHMARK_CASE(dom_batch_insertion) {
    BENCHMARK_LOOP {
        auto document = create_empty_document();
        auto fragment = document->create_document_fragment();

        for (size_t i = 0; i < 1000; ++i) {
            auto div = document->create_element("div");
            fragment->append_child(div);
        }

        document->body()->append_child(fragment);
    }
}
```

### Detecting Performance Regressions

**Benchmark Runner Script:**
```bash
#!/bin/bash
# scripts/benchmark_suite.sh

BUILD_DIR="$1"
BASELINE_FILE="benchmark_baseline.json"

# Run all benchmarks
echo "Running benchmarks..."
"$BUILD_DIR/bin/BenchmarkRunner" --json > current_results.json

# Compare against baseline
if [ -f "$BASELINE_FILE" ]; then
    echo "Comparing against baseline..."
    ./scripts/compare_benchmarks.py "$BASELINE_FILE" current_results.json
else
    echo "No baseline found. Creating baseline..."
    cp current_results.json "$BASELINE_FILE"
fi
```

**Benchmark Comparison:**
```python
#!/usr/bin/env python3
# scripts/compare_benchmarks.py

import json
import sys

def compare_benchmarks(baseline_file, current_file):
    with open(baseline_file) as f:
        baseline = json.load(f)
    with open(current_file) as f:
        current = json.load(f)

    regressions = []
    improvements = []

    for name, current_time in current.items():
        baseline_time = baseline.get(name)
        if not baseline_time:
            continue

        change_pct = ((current_time - baseline_time) / baseline_time) * 100

        if change_pct > 5:  # 5% regression threshold
            regressions.append((name, change_pct))
        elif change_pct < -5:  # 5% improvement
            improvements.append((name, change_pct))

    if regressions:
        print("⚠️  Performance regressions detected:")
        for name, pct in regressions:
            print(f"  {name}: +{pct:.1f}% slower")
        sys.exit(1)

    if improvements:
        print("✅ Performance improvements:")
        for name, pct in improvements:
            print(f"  {name}: {abs(pct):.1f}% faster")

    print("No significant performance changes.")
    sys.exit(0)

if __name__ == "__main__":
    compare_benchmarks(sys.argv[1], sys.argv[2])
```

### A/B Testing Changes

**Controlled Benchmark Comparison:**
```bash
#!/bin/bash
# Compare performance before/after a change

# Build baseline (before change)
git checkout main
cmake --preset Release
cmake --build Build/release
./Build/release/bin/BenchmarkRunner --json > baseline.json

# Build with change (after)
git checkout feature-branch
cmake --build Build/release
./Build/release/bin/BenchmarkRunner --json > feature.json

# Compare
./scripts/compare_benchmarks.py baseline.json feature.json
```

## 5. Performance Debugging Workflow

### Step 1: Identify Bottleneck

**Profile First:**
```bash
# Get high-level CPU profile
perf record -F 99 -g -- ./Build/release/bin/Ladybird https://slow-page.com

# Look for hotspots
perf report --stdio | head -20
```

**Common Symptoms:**
- **High CPU usage**: Algorithmic inefficiency, tight loops
- **High memory usage**: Memory leaks, excessive allocations
- **Long load times**: Network latency, blocking operations
- **UI freezing**: Synchronous operations on main thread

### Step 2: Analyze Root Cause

**CPU-bound:**
```bash
# Generate flame graph
perf script | ./scripts/flamegraph.pl > flame.svg

# Look for:
# - Wide flames (hot functions)
# - Deep stacks (excessive recursion)
# - Unexpected library calls
```

**Memory-bound:**
```bash
# Profile allocations
heaptrack ./Build/release/bin/Ladybird

# Look for:
# - Peak memory usage
# - Allocation count
# - Temporary allocations
# - Leak suspects
```

### Step 3: Implement Optimization

**Apply Pattern:**
```cpp
// Choose optimization strategy:
// 1. Algorithmic: O(n²) → O(n log n)
// 2. Caching: Compute once, reuse
// 3. Lazy evaluation: Defer until needed
// 4. Batching: Reduce overhead
// 5. Parallelization: Use multiple cores
```

### Step 4: Verify Improvement

**Measure Impact:**
```bash
# Run benchmark before
./Build/release/bin/BenchmarkRunner --filter=my_function
# Result: 150ms

# Apply optimization
# ...

# Run benchmark after
./Build/release/bin/BenchmarkRunner --filter=my_function
# Result: 80ms (47% improvement)
```

## 6. Common Performance Bottlenecks

### 1. Excessive String Allocations

**Problem:**
```cpp
// Creates temporary strings
String result;
for (size_t i = 0; i < 1000; ++i) {
    result = result + String::number(i);  // Reallocates every iteration
}
```

**Solution:**
```cpp
// Use StringBuilder
StringBuilder builder;
for (size_t i = 0; i < 1000; ++i) {
    builder.append(String::number(i));  // Efficient append
}
String result = builder.to_string();  // Single allocation
```

### 2. Vector Reallocations

**Problem:**
```cpp
Vector<int> numbers;
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);  // Reallocates multiple times
}
```

**Solution:**
```cpp
Vector<int> numbers;
numbers.ensure_capacity(10000);  // Reserve upfront
for (size_t i = 0; i < 10000; ++i) {
    numbers.append(i);  // No reallocations
}
```

### 3. Unnecessary Copies

**Problem:**
```cpp
void process_data(Vector<int> data) {  // Copy!
    // ...
}
```

**Solution:**
```cpp
void process_data(Vector<int> const& data) {  // Reference
    // ...
}
```

### 4. HashMap Lookups in Loops

**Problem:**
```cpp
for (size_t i = 0; i < 1000; ++i) {
    auto value = map.get("key");  // Lookup every iteration
    process(value);
}
```

**Solution:**
```cpp
auto value = map.get("key");  // Lookup once
for (size_t i = 0; i < 1000; ++i) {
    process(value);
}
```

### 5. Synchronous I/O on Main Thread

**Problem:**
```cpp
// Blocks UI thread
String load_file(String const& path) {
    auto file = Core::File::open(path);  // Blocks
    return file->read_all();
}
```

**Solution:**
```cpp
// Async I/O
void load_file_async(String const& path, Function<void(String)> callback) {
    Thread::create([path, callback] {
        auto file = Core::File::open(path);
        auto content = file->read_all();

        // Call callback on main thread
        Core::EventLoop::current().deferred_invoke([content, callback] {
            callback(content);
        });
    });
}
```

### 6. Excessive Virtual Function Calls

**Problem:**
```cpp
for (size_t i = 0; i < 1000000; ++i) {
    shape->draw();  // Virtual call overhead
}
```

**Solution:**
```cpp
// Batch virtual calls
shape->draw_n_times(1000000);  // Single virtual call
```

## 7. Performance Optimization Checklist

### Before Committing
- [ ] Run benchmarks to verify no regressions
- [ ] Profile with perf/callgrind to check hotspots
- [ ] Check for memory leaks with ASAN
- [ ] Verify no increase in allocations (heaptrack)
- [ ] Test on representative workloads (large pages, complex sites)
- [ ] Compare flame graphs before/after
- [ ] Update performance-critical comments
- [ ] Document optimization rationale

### Code Review Checklist
- [ ] Avoid string allocations in hot paths
- [ ] Reserve vector capacity when size known
- [ ] Use references to avoid copies
- [ ] Cache expensive lookups
- [ ] Batch operations to reduce overhead
- [ ] Avoid forced synchronous layout
- [ ] Use StringView for temporary strings
- [ ] Minimize virtual function calls in tight loops

## Quick Reference

### Profiling Commands
```bash
# CPU profiling
perf record -F 99 -g -- ./Build/release/bin/Ladybird
./Meta/ladybird.py profile ladybird https://example.com

# Memory profiling
heaptrack ./Build/release/bin/Ladybird
valgrind --tool=massif ./Build/release/bin/Ladybird

# Leak detection
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Flame graphs
perf script | ./scripts/flamegraph.pl > flame.svg
```

### Analysis Commands
```bash
# View perf report
perf report

# View callgrind report
kcachegrind callgrind.out

# View heaptrack report
heaptrack_gui heaptrack.*.gz

# View massif report
ms_print massif.out
```

### Benchmark Commands
```bash
# Run benchmarks
./Build/release/bin/BenchmarkRunner

# Run specific benchmark
./Build/release/bin/BenchmarkRunner --filter=pattern

# Compare benchmarks
./scripts/compare_benchmarks.py baseline.json current.json
```

## Related Skills

### Architecture Performance
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Profile multi-process overhead and IPC latency. Optimize process spawn times and message passing.
- **[ipc-security](../ipc-security/SKILL.md)**: Balance security validation with performance. Profile IPC message validation overhead.

### Component Optimization
- **[libweb-patterns](../libweb-patterns/SKILL.md)**: Optimize rendering pipeline (layout, paint, style computation). Profile DOM manipulation and CSS selector matching.
- **[libjs-patterns](../libjs-patterns/SKILL.md)**: Optimize JavaScript execution. Profile bytecode generation, JIT compilation, and GC performance.
- **[tor-integration](../tor-integration/SKILL.md)**: Profile Tor circuit creation overhead. Optimize circuit reuse and stream multiplexing.

### Build and Testing
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Build Release preset for accurate profiling. Understand optimization flags (LTO, PGO).
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Run performance regression tests in CI. Track benchmark results across commits.
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Benchmark spec-compliant implementations. Measure WPT test execution time.

### Debugging Tools
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Use profiling tools (perf, heaptrack, massif). Debug memory leaks and allocation hotspots.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Profile fuzzer corpus processing. Optimize fuzzer harness performance.

### Development Patterns
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Apply performance patterns (avoid allocations, use StringView, cache lookups).
