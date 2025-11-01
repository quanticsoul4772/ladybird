# Benchmarking Guide for Ladybird

## Principles of Good Benchmarks

### 1. Reproducibility
- Same input → same output
- Minimal environmental variation
- Documented dependencies
- Fixed random seeds

### 2. Relevance
- Measure real-world scenarios
- Focus on user-visible metrics
- Benchmark what matters

### 3. Sensitivity
- Detect meaningful changes (>5%)
- Minimize noise (<1% stddev)
- Sufficient iteration count

### 4. Isolation
- One thing at a time
- No external interference
- Controlled environment

## Benchmark Structure

### Microbenchmark Template
```cpp
// Tests/Benchmarks/MyBenchmark.cpp
#include <LibTest/Benchmark.h>

BENCHMARK_CASE(descriptive_name)
{
    // Setup (not timed)
    auto test_data = create_test_data();

    // Benchmark loop (timed)
    BENCHMARK_LOOP {
        auto result = function_to_benchmark(test_data);
        VERIFY(result);  // Prevent dead code elimination
    }

    // Cleanup (not timed)
}
```

### Macro Benchmark Template
```cpp
BENCHMARK_CASE(page_load_simple)
{
    BENCHMARK_LOOP {
        // Setup
        auto page = create_test_page();

        // Load page (timed)
        auto start = MonotonicTime::now();
        page->load_url("file:///test_pages/simple.html");
        page->wait_for_load_complete();
        auto duration = MonotonicTime::now() - start;

        // Report
        dbgln("Load time: {}ms", duration.to_milliseconds());
    }
}
```

## Types of Benchmarks

### 1. Microbenchmarks
Focus on single functions or small operations.

**Example: String Operations**
```cpp
BENCHMARK_CASE(string_concatenation_naive)
{
    BENCHMARK_LOOP {
        String result;
        for (size_t i = 0; i < 1000; ++i) {
            result = String::formatted("{}{}", result, i);
        }
        VERIFY(!result.is_empty());
    }
}

BENCHMARK_CASE(string_concatenation_builder)
{
    BENCHMARK_LOOP {
        StringBuilder builder;
        for (size_t i = 0; i < 1000; ++i) {
            builder.append(String::number(i));
        }
        String result = builder.to_string();
        VERIFY(!result.is_empty());
    }
}
```

**When to use:**
- Optimizing specific functions
- Comparing implementation alternatives
- Testing data structure performance

### 2. Component Benchmarks
Focus on subsystem performance.

**Example: DOM Manipulation**
```cpp
BENCHMARK_CASE(dom_query_selector_by_id)
{
    auto document = create_document_with_1000_elements();

    BENCHMARK_LOOP {
        auto element = document->query_selector("#element-500");
        VERIFY(element);
    }
}

BENCHMARK_CASE(dom_batch_insertion)
{
    BENCHMARK_LOOP {
        auto document = create_empty_document();
        auto fragment = document->create_document_fragment();

        for (size_t i = 0; i < 100; ++i) {
            auto div = document->create_element("div");
            fragment->append_child(div);
        }

        document->body()->append_child(fragment);
    }
}
```

**When to use:**
- Testing specific subsystems (DOM, CSS, JS)
- Isolating component performance
- Regression testing for specific features

### 3. End-to-End Benchmarks
Focus on complete workflows.

**Example: Page Load**
```cpp
BENCHMARK_CASE(page_load_complex)
{
    BENCHMARK_LOOP {
        auto page = create_test_page();

        auto start = MonotonicTime::now();
        page->load_url("https://example.com");
        page->wait_for_load_complete();
        auto load_time = MonotonicTime::now() - start;

        dbgln("Load: {}ms", load_time.to_milliseconds());
        VERIFY(page->is_loaded());
    }
}
```

**When to use:**
- Measuring user-visible performance
- Overall system performance
- Release benchmarks

## Creating Benchmarks

### Step 1: Identify What to Measure
```
Ask:
- What user action does this affect?
- What metric matters? (time, memory, throughput)
- What's the realistic workload?
```

### Step 2: Create Test Data
```cpp
static NonnullRefPtr<Document> create_test_document()
{
    auto document = Document::create();

    // Setup HTML structure
    auto html = document->create_element("html");
    auto head = document->create_element("head");
    auto body = document->create_element("body");

    html->append_child(head);
    html->append_child(body);
    document->append_child(html);

    return document;
}

static void populate_with_elements(Document& doc, size_t count)
{
    auto body = doc.body();
    for (size_t i = 0; i < count; ++i) {
        auto div = doc.create_element("div");
        div->set_attribute("id", String::formatted("element-{}", i));
        div->set_attribute("class", "test-class");
        div->set_text_content(String::formatted("Element {}", i));
        body->append_child(div);
    }
}
```

### Step 3: Write Benchmark
```cpp
BENCHMARK_CASE(query_selector_performance)
{
    // Setup test data
    auto document = create_test_document();
    populate_with_elements(*document, 1000);

    // Warm up
    for (size_t i = 0; i < 10; ++i) {
        document->query_selector(".test-class");
    }

    // Benchmark
    BENCHMARK_LOOP {
        auto element = document->query_selector(".test-class");
        VERIFY(element);  // Prevent optimization
    }
}
```

### Step 4: Validate Results
```bash
# Run benchmark multiple times
for i in {1..5}; do
    ./Build/release/bin/MyBenchmark
done

# Check for consistency
# Coefficient of variation should be < 5%
```

## Benchmark Best Practices

### Prevent Dead Code Elimination
```cpp
// BAD: Compiler may optimize away
BENCHMARK_LOOP {
    auto result = expensive_function();
    // Result not used - may be optimized out!
}

// GOOD: Force compiler to keep result
BENCHMARK_LOOP {
    auto result = expensive_function();
    VERIFY(result);  // Ensures result is used
}

// ALTERNATIVE: Use volatile
BENCHMARK_LOOP {
    volatile auto result = expensive_function();
}
```

### Minimize Setup Overhead
```cpp
// BAD: Setup inside benchmark loop
BENCHMARK_LOOP {
    auto test_data = create_large_dataset();  // Expensive!
    process(test_data);
}

// GOOD: Setup outside benchmark loop
auto test_data = create_large_dataset();
BENCHMARK_LOOP {
    process(test_data);
}
```

### Use Representative Data
```cpp
// BAD: Unrealistic data
auto html = "<div></div>";  // Too simple

// GOOD: Realistic data
auto html = load_real_webpage("examples/complex.html");
```

### Account for Warmup
```cpp
BENCHMARK_CASE(with_warmup)
{
    auto data = create_test_data();

    // Warm up caches, JIT, etc.
    for (size_t i = 0; i < 10; ++i) {
        function_to_benchmark(data);
    }

    // Actual benchmark
    BENCHMARK_LOOP {
        function_to_benchmark(data);
    }
}
```

### Measure What Matters
```cpp
// Page load benchmark should measure user-visible metrics
BENCHMARK_CASE(page_load_metrics)
{
    BENCHMARK_LOOP {
        auto page = create_test_page();

        auto nav_start = MonotonicTime::now();
        page->load_url("test.html");

        // Wait for First Paint
        auto first_paint = wait_for_first_paint(page);
        auto first_paint_time = first_paint - nav_start;

        // Wait for DOMContentLoaded
        auto dcl = wait_for_dom_content_loaded(page);
        auto dcl_time = dcl - nav_start;

        // Wait for Load
        page->wait_for_load_complete();
        auto load_time = MonotonicTime::now() - nav_start;

        // Report all metrics
        dbgln("First Paint: {}ms", first_paint_time.to_milliseconds());
        dbgln("DOMContentLoaded: {}ms", dcl_time.to_milliseconds());
        dbgln("Load Complete: {}ms", load_time.to_milliseconds());
    }
}
```

## Running Benchmarks

### Build Configuration
```bash
# Always use Release build
cmake --preset Release
cmake --build Build/release

# Run specific benchmark
./Build/release/bin/MyBenchmark

# Run with filter
./Build/release/bin/MyBenchmark --filter=pattern
```

### Environment Setup
```bash
# Minimize system interference
# Close unnecessary applications
# Disable CPU frequency scaling
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Pin to specific CPUs
taskset -c 0,1 ./Build/release/bin/MyBenchmark

# Set process priority
nice -n -20 ./Build/release/bin/MyBenchmark
```

### Multiple Runs
```bash
# Run 10 times and collect statistics
for i in {1..10}; do
    ./Build/release/bin/MyBenchmark --json >> results.jsonl
done

# Analyze results
python3 scripts/analyze_benchmark_stats.py results.jsonl
```

## Comparing Benchmarks

### Before/After Comparison
```bash
# Baseline
git checkout main
cmake --build Build/release
./Build/release/bin/MyBenchmark --json > baseline.json

# After changes
git checkout feature-branch
cmake --build Build/release
./Build/release/bin/MyBenchmark --json > feature.json

# Compare
python3 scripts/compare_benchmarks.py baseline.json feature.json
```

### Statistical Significance
```python
# scripts/compare_with_significance.py
import json
import scipy.stats as stats

def compare_with_ttest(baseline, current, alpha=0.05):
    # Load multiple runs
    baseline_runs = [json.loads(line) for line in open(baseline)]
    current_runs = [json.loads(line) for line in open(current)]

    for test_name in baseline_runs[0].keys():
        baseline_times = [run[test_name] for run in baseline_runs]
        current_times = [run[test_name] for run in current_runs]

        # Perform t-test
        t_stat, p_value = stats.ttest_ind(baseline_times, current_times)

        # Check significance
        if p_value < alpha:
            mean_baseline = sum(baseline_times) / len(baseline_times)
            mean_current = sum(current_times) / len(current_times)
            change_pct = ((mean_current - mean_baseline) / mean_baseline) * 100

            if change_pct > 0:
                print(f"⚠️  {test_name}: +{change_pct:.1f}% (p={p_value:.4f})")
            else:
                print(f"✅ {test_name}: {change_pct:.1f}% (p={p_value:.4f})")
        else:
            print(f"➖ {test_name}: No significant change (p={p_value:.4f})")
```

## Regression Detection

### Automated Regression Testing
```bash
#!/bin/bash
# scripts/detect_regression.sh

BASELINE="benchmark_baseline.json"
THRESHOLD=5  # 5% regression threshold

# Run current benchmarks
./Build/release/bin/BenchmarkRunner --json > current.json

# Compare
python3 << EOF
import json
import sys

with open("$BASELINE") as f:
    baseline = json.load(f)

with open("current.json") as f:
    current = json.load(f)

regressions = []
for name, current_time in current.items():
    baseline_time = baseline.get(name, 0)
    if baseline_time == 0:
        continue

    change_pct = ((current_time - baseline_time) / baseline_time) * 100

    if change_pct > $THRESHOLD:
        regressions.append((name, change_pct))
        print(f"⚠️  Regression: {name} +{change_pct:.1f}%")

if regressions:
    sys.exit(1)
EOF

if [ $? -ne 0 ]; then
    echo "❌ Performance regression detected"
    exit 1
fi

echo "✅ No performance regressions"
```

### CI Integration
```yaml
# .github/workflows/performance.yml
name: Performance Tests

on: [pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build Release
        run: |
          cmake --preset Release
          cmake --build Build/release

      - name: Download Baseline
        uses: actions/download-artifact@v3
        with:
          name: benchmark-baseline
          path: .

      - name: Run Benchmarks
        run: |
          ./Build/release/bin/BenchmarkRunner --json > current.json

      - name: Compare Results
        run: |
          python3 scripts/compare_benchmarks.py baseline.json current.json

      - name: Upload Results
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: benchmark-regression
          path: current.json
```

## Benchmark Maintenance

### Updating Baselines
```bash
# When intentional performance changes are made
./Build/release/bin/BenchmarkRunner --json > new_baseline.json

# Review changes
python3 scripts/compare_benchmarks.py baseline.json new_baseline.json

# Update baseline
cp new_baseline.json benchmark_baseline.json
git add benchmark_baseline.json
git commit -m "Update benchmark baseline after optimization"
```

### Archiving Historical Data
```bash
# Keep historical benchmark data
mkdir -p benchmark_history
DATE=$(date +%Y%m%d)
cp benchmark_baseline.json benchmark_history/baseline_$DATE.json
```

## Common Pitfalls

### 1. Measuring Compilation Time
```cpp
// BAD: First call may include compilation
BENCHMARK_LOOP {
    regex->match(input);  // First call compiles regex
}

// GOOD: Compile before benchmarking
auto regex = compile_regex(pattern);
BENCHMARK_LOOP {
    regex->match(input);
}
```

### 2. Ignoring Variance
```
Run 1: 100ms
Run 2: 150ms  # High variance!
Run 3: 95ms

→ Benchmark is unreliable, investigate noise sources
```

### 3. Benchmarking Debug Builds
```bash
# WRONG
cmake --preset Debug
./Build/debug/bin/MyBenchmark  # Results meaningless

# RIGHT
cmake --preset Release
./Build/release/bin/MyBenchmark
```

### 4. Inadequate Iteration Count
```cpp
// BAD: Too few iterations, high variance
for (size_t i = 0; i < 10; ++i) {
    benchmark_function();
}

// GOOD: Sufficient iterations for stable results
for (size_t i = 0; i < 1000; ++i) {
    benchmark_function();
}
```

## Checklist

Before submitting benchmarks:
- [ ] Reproducible (same input → same output)
- [ ] Relevant (measures real-world scenarios)
- [ ] Isolated (one thing at a time)
- [ ] Prevents dead code elimination
- [ ] Setup outside benchmark loop
- [ ] Representative test data
- [ ] Adequate warmup
- [ ] Sufficient iterations (low variance)
- [ ] Release build
- [ ] Multiple runs for statistical significance
- [ ] Documented purpose and interpretation
