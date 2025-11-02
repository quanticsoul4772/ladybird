/*
 * Copyright (c) 2025, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/File.h>
#include <LibFileSystem/FileSystem.h>
#include <Services/Sentinel/Sandbox/BehavioralAnalyzer.h>
// #include <Services/Sentinel/Sandbox/SandboxConfig.h> // File doesn't exist yet

using namespace Sentinel::Sandbox;

struct BenchmarkResult {
    ByteString name;
    size_t file_size;
    Duration total_time;
    Duration min_time;
    Duration max_time;
    Duration avg_time;
    size_t iterations;
};

static ErrorOr<BenchmarkResult> benchmark_file_analysis(
    StringView file_path,
    StringView benchmark_name,
    size_t iterations = 10)
{
    outln("Running benchmark: {}", benchmark_name);
    outln("  File: {}", file_path);
    outln("  Iterations: {}", iterations);

    // Get file size
    auto stat = TRY(Core::File::stat(file_path));
    size_t file_size = stat.size;

    Vector<Duration> timings;
    Duration total_time;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();

    for (size_t i = 0; i < iterations; ++i) {
        outln("  Iteration {}/{}...", i + 1, iterations);

        // Create fresh analyzer for each iteration
        auto config = SandboxConfig {};
        auto analyzer = TRY(BehavioralAnalyzer::create(config));

        // Measure analysis time
        Core::ElapsedTimer timer;
        timer.start();

        // TODO: Implement actual analysis
        // auto result = TRY(analyzer->analyze(file_path));
        // For now, just sleep to simulate work
        // This will be replaced with real analysis

        auto elapsed = timer.elapsed_time();

        total_time = total_time + elapsed;
        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;

        timings.append(elapsed);
    }

    auto avg_time = Duration::from_nanoseconds(total_time.to_nanoseconds() / iterations);

    outln("  Results:");
    outln("    Total time: {} ms", total_time.to_milliseconds());
    outln("    Min time:   {} ms", min_time.to_milliseconds());
    outln("    Max time:   {} ms", max_time.to_milliseconds());
    outln("    Avg time:   {} ms", avg_time.to_milliseconds());
    outln("");

    return BenchmarkResult {
        .name = benchmark_name,
        .file_size = file_size,
        .total_time = total_time,
        .min_time = min_time,
        .max_time = max_time,
        .avg_time = avg_time,
        .iterations = iterations,
    };
}

static ErrorOr<void> create_test_file(StringView path, size_t size_bytes)
{
    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Write));

    // Write random data
    Vector<u8> buffer;
    buffer.resize(min(size_bytes, 4096uz));

    size_t written = 0;
    while (written < size_bytes) {
        size_t to_write = min(buffer.size(), size_bytes - written);
        TRY(file->write_until_depleted(buffer.span().trim(to_write)));
        written += to_write;
    }

    return {};
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    outln("======================================");
    outln("BehavioralAnalyzer Performance Benchmark");
    outln("======================================");
    outln("");

    Vector<BenchmarkResult> results;

    // ========================================================================
    // Benchmark 1: Small file (1 KB)
    // ========================================================================
    {
        auto temp_file = "/tmp/sentinel_bench_1kb.bin"sv;
        TRY(create_test_file(temp_file, 1024));

        auto result = TRY(benchmark_file_analysis(
            temp_file,
            "Small file (1 KB)",
            10
        ));
        results.append(result);

        FileSystem::remove(temp_file, FileSystem::RecursionMode::Disallowed);
    }

    // ========================================================================
    // Benchmark 2: Medium file (100 KB)
    // ========================================================================
    {
        auto temp_file = "/tmp/sentinel_bench_100kb.bin"sv;
        TRY(create_test_file(temp_file, 100 * 1024));

        auto result = TRY(benchmark_file_analysis(
            temp_file,
            "Medium file (100 KB)",
            10
        ));
        results.append(result);

        FileSystem::remove(temp_file, FileSystem::RecursionMode::Disallowed);
    }

    // ========================================================================
    // Benchmark 3: Large file (1 MB)
    // ========================================================================
    {
        auto temp_file = "/tmp/sentinel_bench_1mb.bin"sv;
        TRY(create_test_file(temp_file, 1024 * 1024));

        auto result = TRY(benchmark_file_analysis(
            temp_file,
            "Large file (1 MB)",
            5 // Fewer iterations for large files
        ));
        results.append(result);

        FileSystem::remove(temp_file, FileSystem::RecursionMode::Disallowed);
    }

    // ========================================================================
    // Benchmark 4: Real test files (if available)
    // ========================================================================
    {
        auto test_file = "Services/Sentinel/Test/behavioral/benign/hello.sh"sv;
        if (FileSystem::exists(test_file)) {
            auto result = TRY(benchmark_file_analysis(
                test_file,
                "Benign script (hello.sh)",
                10
            ));
            results.append(result);
        } else {
            outln("Skipping real file benchmark (test file not found)");
            outln("");
        }
    }

    // ========================================================================
    // Summary Report
    // ========================================================================
    outln("======================================");
    outln("Summary Report");
    outln("======================================");
    outln("");

    outln("┌────────────────────────────┬────────────┬─────────────┬─────────────┬─────────────┐");
    outln("│ Benchmark                  │ File Size  │ Min (ms)    │ Avg (ms)    │ Max (ms)    │");
    outln("├────────────────────────────┼────────────┼─────────────┼─────────────┼─────────────┤");

    for (auto const& result : results) {
        outln("│ {:26} │ {:>10} │ {:>11} │ {:>11} │ {:>11} │",
            result.name,
            result.file_size,
            result.min_time.to_milliseconds(),
            result.avg_time.to_milliseconds(),
            result.max_time.to_milliseconds()
        );
    }

    outln("└────────────────────────────┴────────────┴─────────────┴─────────────┴─────────────┘");
    outln("");

    // ========================================================================
    // Performance Target Validation
    // ========================================================================
    outln("Performance Target Validation:");
    outln("  Target: 95th percentile < 5000 ms");
    outln("");

    bool all_passed = true;
    for (auto const& result : results) {
        // Use max time as proxy for 95th percentile (conservative)
        bool passed = result.max_time.to_milliseconds() < 5000;
        all_passed = all_passed && passed;

        outln("  {} - {} (max: {} ms)",
            passed ? "✓ PASS" : "✗ FAIL",
            result.name,
            result.max_time.to_milliseconds()
        );
    }

    outln("");
    outln("======================================");
    outln("Overall: {}", all_passed ? "PASS" : "FAIL");
    outln("======================================");

    return all_passed ? 0 : 1;
}

/*
 * Expected output when implementation is complete:
 *
 * ======================================
 * BehavioralAnalyzer Performance Benchmark
 * ======================================
 *
 * Running benchmark: Small file (1 KB)
 *   File: /tmp/sentinel_bench_1kb.bin
 *   Iterations: 10
 *   Iteration 1/10...
 *   ...
 *   Results:
 *     Total time: 5000 ms
 *     Min time:   450 ms
 *     Max time:   550 ms
 *     Avg time:   500 ms
 *
 * [... more benchmarks ...]
 *
 * ======================================
 * Summary Report
 * ======================================
 *
 * ┌────────────────────────────┬────────────┬─────────────┬─────────────┬─────────────┐
 * │ Benchmark                  │ File Size  │ Min (ms)    │ Avg (ms)    │ Max (ms)    │
 * ├────────────────────────────┼────────────┼─────────────┼─────────────┼─────────────┤
 * │ Small file (1 KB)          │       1024 │         450 │         500 │         550 │
 * │ Medium file (100 KB)       │     102400 │        1200 │        1500 │        1800 │
 * │ Large file (1 MB)          │    1048576 │        3500 │        4000 │        4500 │
 * │ Benign script (hello.sh)   │        256 │         400 │         450 │         500 │
 * └────────────────────────────┴────────────┴─────────────┴─────────────┴─────────────┘
 *
 * Performance Target Validation:
 *   Target: 95th percentile < 5000 ms
 *
 *   ✓ PASS - Small file (1 KB) (max: 550 ms)
 *   ✓ PASS - Medium file (100 KB) (max: 1800 ms)
 *   ✓ PASS - Large file (1 MB) (max: 4500 ms)
 *   ✓ PASS - Benign script (hello.sh) (max: 500 ms)
 *
 * ======================================
 * Overall: PASS
 * ======================================
 */
