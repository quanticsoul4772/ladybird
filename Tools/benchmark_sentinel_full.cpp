/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/ByteBuffer.h>
#include <AK/Random.h>
#include <AK/Time.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <Services/RequestServer/SecurityTap.h>
#include <stdio.h>

using namespace RequestServer;

struct BenchmarkResult {
    String test_name;
    size_t file_size;
    size_t iterations;
    Duration total_time;
    Duration avg_time;
    Duration min_time;
    Duration max_time;
};

static ErrorOr<ByteBuffer> generate_random_content(size_t size)
{
    auto buffer = TRY(ByteBuffer::create_uninitialized(size));
    fill_with_random(buffer.data(), buffer.size());
    return buffer;
}

static ErrorOr<ByteString> compute_hash_for_content(ReadonlyBytes content)
{
    return SecurityTap::compute_sha256(content);
}

static ErrorOr<BenchmarkResult> benchmark_download_path(
    SecurityTap& tap,
    size_t file_size,
    size_t iterations,
    String const& test_name)
{
    outln("Running benchmark: {} (file_size={}KB, iterations={})",
        test_name, file_size / 1024, iterations);

    // Generate test content
    auto content = TRY(generate_random_content(file_size));
    auto hash = TRY(compute_hash_for_content(content.bytes()));

    SecurityTap::DownloadMetadata metadata {
        .url = "https://example.com/test.bin"sv,
        .filename = "test.bin"sv,
        .mime_type = "application/octet-stream"sv,
        .sha256 = hash,
        .size_bytes = file_size
    };

    Vector<Duration> timings;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();
    Duration total_time {};

    // Warmup run
    (void)tap.inspect_download(metadata, content.bytes());

    // Benchmark runs
    for (size_t i = 0; i < iterations; i++) {
        auto start_time = MonotonicTime::now();
        auto result = tap.inspect_download(metadata, content.bytes());
        auto end_time = MonotonicTime::now();

        if (result.is_error()) {
            outln("  [{}] Error: {}", i, result.error());
            continue;
        }

        auto elapsed = end_time - start_time;
        timings.append(elapsed);
        total_time = total_time + elapsed;

        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;

        if ((i + 1) % 10 == 0)
            outln("  Progress: {}/{}", i + 1, iterations);
    }

    auto avg_time = total_time / iterations;

    return BenchmarkResult {
        .test_name = test_name,
        .file_size = file_size,
        .iterations = iterations,
        .total_time = total_time,
        .avg_time = avg_time,
        .min_time = min_time,
        .max_time = max_time
    };
}

static ErrorOr<BenchmarkResult> benchmark_hash_computation(
    size_t file_size,
    size_t iterations,
    String const& test_name)
{
    outln("Running benchmark: {} (file_size={}KB, iterations={})",
        test_name, file_size / 1024, iterations);

    auto content = TRY(generate_random_content(file_size));

    Vector<Duration> timings;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();
    Duration total_time {};

    // Warmup
    (void)compute_hash_for_content(content.bytes());

    for (size_t i = 0; i < iterations; i++) {
        auto start_time = MonotonicTime::now();
        auto hash = TRY(compute_hash_for_content(content.bytes()));
        auto end_time = MonotonicTime::now();

        auto elapsed = end_time - start_time;
        timings.append(elapsed);
        total_time = total_time + elapsed;

        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;
    }

    auto avg_time = total_time / iterations;

    return BenchmarkResult {
        .test_name = test_name,
        .file_size = file_size,
        .iterations = iterations,
        .total_time = total_time,
        .avg_time = avg_time,
        .min_time = min_time,
        .max_time = max_time
    };
}

static void print_results(Vector<BenchmarkResult> const& results)
{
    outln("\n==================================================");
    outln("BENCHMARK RESULTS");
    outln("==================================================\n");

    outln("{:<45} | {:>10} | {:>8} | {:>10} | {:>10} | {:>10}",
        "Test Name", "File Size", "Iters", "Avg (ms)", "Min (ms)", "Max (ms)");
    outln("{:-<45}-+-{:-<10}-+-{:-<8}-+-{:-<10}-+-{:-<10}-+-{:-<10}",
        "", "", "", "", "", "");

    for (auto const& result : results) {
        auto file_size_str = String::formatted("{}KB", result.file_size / 1024).release_value_but_fixme_should_propagate_errors();
        outln("{:<45} | {:>10} | {:>8} | {:>10.2f} | {:>10.2f} | {:>10.2f}",
            result.test_name,
            file_size_str,
            result.iterations,
            result.avg_time.to_milliseconds(),
            result.min_time.to_milliseconds(),
            result.max_time.to_milliseconds());
    }

    outln("\n==================================================\n");

    // Calculate overhead percentages
    outln("Performance Analysis:");
    outln("--------------------");

    for (auto const& result : results) {
        if (result.test_name.contains("Download Path"sv)) {
            // Find corresponding hash-only benchmark
            for (auto const& hash_result : results) {
                if (hash_result.test_name.contains("Hash Only"sv) && hash_result.file_size == result.file_size) {
                    auto overhead_ms = result.avg_time.to_milliseconds() - hash_result.avg_time.to_milliseconds();
                    auto overhead_pct = (overhead_ms / hash_result.avg_time.to_milliseconds()) * 100.0;
                    outln("  {} - Sentinel Overhead: {:.2f}ms ({:.1f}%)",
                        String::formatted("{}KB", result.file_size / 1024).release_value_but_fixme_should_propagate_errors(),
                        overhead_ms,
                        overhead_pct);
                    break;
                }
            }
        }
    }

    outln("");
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    bool verbose = false;
    bool skip_sentinel = false;

    Core::ArgsParser args_parser;
    args_parser.add_option(verbose, "Verbose output", "verbose", 'v');
    args_parser.add_option(skip_sentinel, "Skip Sentinel benchmarks (hash only)", "skip-sentinel", 's');
    args_parser.parse(arguments);

    Vector<BenchmarkResult> results;

    // Benchmark 1: Hash computation only (baseline)
    outln("=== BASELINE: Hash Computation ===\n");
    TRY_OR_RETURN(results, benchmark_hash_computation(100 * 1024, 100, "Hash Only - Small File (100KB)"_string));
    TRY_OR_RETURN(results, benchmark_hash_computation(1024 * 1024, 50, "Hash Only - Medium File (1MB)"_string));
    TRY_OR_RETURN(results, benchmark_hash_computation(10 * 1024 * 1024, 20, "Hash Only - Large File (10MB)"_string));

    if (!skip_sentinel) {
        // Benchmark 2: Full download path with Sentinel
        outln("\n=== FULL PATH: Download with Sentinel Scan ===\n");

        // Try to connect to Sentinel
        auto tap_result = SecurityTap::create();
        if (tap_result.is_error()) {
            warnln("ERROR: Could not connect to Sentinel daemon: {}", tap_result.error());
            warnln("Make sure Sentinel is running at /tmp/sentinel.sock");
            warnln("Run with --skip-sentinel to benchmark hash computation only");
            return 1;
        }

        auto tap = tap_result.release_value();

        TRY_OR_RETURN(results, benchmark_download_path(*tap, 100 * 1024, 100, "Download Path - Small File (100KB)"_string));
        TRY_OR_RETURN(results, benchmark_download_path(*tap, 1024 * 1024, 50, "Download Path - Medium File (1MB)"_string));
        TRY_OR_RETURN(results, benchmark_download_path(*tap, 10 * 1024 * 1024, 20, "Download Path - Large File (10MB)"_string));
        TRY_OR_RETURN(results, benchmark_download_path(*tap, 100 * 1024 * 1024, 5, "Download Path - XL File (100MB)"_string));
    }

    // Print summary
    print_results(results);

    // Target analysis
    outln("Target Compliance:");
    outln("-----------------");
    outln("Target: < 5% overhead for download path");
    outln("");

    for (auto const& result : results) {
        if (result.test_name.contains("Download Path"sv)) {
            for (auto const& hash_result : results) {
                if (hash_result.test_name.contains("Hash Only"sv) && hash_result.file_size == result.file_size) {
                    auto overhead_ms = result.avg_time.to_milliseconds() - hash_result.avg_time.to_milliseconds();
                    auto overhead_pct = (overhead_ms / hash_result.avg_time.to_milliseconds()) * 100.0;

                    auto status = overhead_pct < 5.0 ? "PASS" : "FAIL";
                    outln("  {} - {:.1f}% overhead [{}]",
                        String::formatted("{}KB", result.file_size / 1024).release_value_but_fixme_should_propagate_errors(),
                        overhead_pct,
                        status);
                    break;
                }
            }
        }
    }

    outln("");
    return 0;
}
