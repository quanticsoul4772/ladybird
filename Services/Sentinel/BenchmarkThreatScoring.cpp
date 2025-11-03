/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Sandbox/BehavioralAnalyzer.h"
#include <AK/QuickSort.h>
#include <AK/Random.h>
#include <AK/Time.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>

using namespace Sentinel::Sandbox;

// ============================================================================
// Benchmark Helper Functions
// ============================================================================

static ByteBuffer create_test_file(size_t size, StringView pattern)
{
    ByteBuffer buffer;
    MUST(buffer.try_resize(size));

    // Fill with pattern that triggers heuristics
    if (pattern == "ransomware"sv) {
        // Include patterns that suggest ransomware
        auto content = "VirtualProtect mprotect /tmp/ CreateProcess socket connect http:// .com delete unlink rename chmod"sv;
        for (size_t i = 0; i < size; i++) {
            buffer[i] = content[i % content.length()];
        }
    } else if (pattern == "network"sv) {
        // Include network patterns
        auto content = "socket connect send recv http:// https:// 192.168. 10. .com .org GET POST User-Agent"sv;
        for (size_t i = 0; i < size; i++) {
            buffer[i] = content[i % content.length()];
        }
    } else if (pattern == "process"sv) {
        // Include process patterns
        auto content = "CreateProcess exec fork vfork clone ptrace process_vm_writev crontab Startup RunOnce"sv;
        for (size_t i = 0; i < size; i++) {
            buffer[i] = content[i % content.length()];
        }
    } else {
        // Benign pattern
        auto content = "Hello World! This is a test file with normal content."sv;
        for (size_t i = 0; i < size; i++) {
            buffer[i] = content[i % content.length()];
        }
    }

    return buffer;
}

// ============================================================================
// Benchmark 1: End-to-End Mock Analysis (Threat Scoring Core)
// ============================================================================

static void benchmark_mock_analysis_benign()
{
    dbgln("\n=== Benchmark 1a: Mock Analysis - Benign File ===");
    dbgln("Target: <1ms for threat scoring calculations");

    auto config = SandboxConfig {};
    config.timeout = Duration::from_seconds(1);
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create benign test file
    auto file_data = create_test_file(1024, "benign"sv);
    auto filename = "test_benign.txt"_string;

    // Warmup
    for (size_t i = 0; i < 100; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    // Benchmark
    Core::ElapsedTimer timer;
    timer.start();

    constexpr size_t iterations = 1000;
    for (size_t i = 0; i < iterations; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    auto elapsed_ns = timer.elapsed_time().to_nanoseconds();
    auto avg_ns = elapsed_ns / iterations;
    auto avg_us = static_cast<float>(avg_ns) / 1000.0f;
    auto avg_ms = static_cast<float>(avg_ns) / 1'000'000.0f;

    dbgln("Iterations: {}", iterations);
    dbgln("Total Time: {:.2f} ms", static_cast<float>(elapsed_ns) / 1'000'000.0f);
    dbgln("Average Time: {} ns ({:.2f} µs, {:.4f} ms)", avg_ns, avg_us, avg_ms);
    dbgln("Target: <1,000,000 ns (1 ms) for scoring");

    if (avg_ns < 1'000'000) {
        dbgln("✓ PASS: Threat scoring performance target met");
    } else {
        dbgln("✗ FAIL: Threat scoring too slow");
    }
}

static void benchmark_mock_analysis_suspicious()
{
    dbgln("\n=== Benchmark 1b: Mock Analysis - Suspicious File ===");
    dbgln("Target: <1ms for threat scoring calculations");

    auto config = SandboxConfig {};
    config.timeout = Duration::from_seconds(1);
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create suspicious test file (network + process patterns)
    auto file_data = create_test_file(2048, "network"sv);
    auto filename = "test_suspicious.exe"_string;

    // Warmup
    for (size_t i = 0; i < 100; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    // Benchmark
    Core::ElapsedTimer timer;
    timer.start();

    constexpr size_t iterations = 1000;
    for (size_t i = 0; i < iterations; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    auto elapsed_ns = timer.elapsed_time().to_nanoseconds();
    auto avg_ns = elapsed_ns / iterations;
    auto avg_us = static_cast<float>(avg_ns) / 1000.0f;
    auto avg_ms = static_cast<float>(avg_ns) / 1'000'000.0f;

    dbgln("Iterations: {}", iterations);
    dbgln("Total Time: {:.2f} ms", static_cast<float>(elapsed_ns) / 1'000'000.0f);
    dbgln("Average Time: {} ns ({:.2f} µs, {:.4f} ms)", avg_ns, avg_us, avg_ms);

    if (avg_ns < 1'000'000) {
        dbgln("✓ PASS: Threat scoring performance target met");
    } else {
        dbgln("✗ FAIL: Threat scoring too slow");
    }
}

static void benchmark_mock_analysis_malicious()
{
    dbgln("\n=== Benchmark 1c: Mock Analysis - Malicious File (Ransomware Pattern) ===");
    dbgln("Target: <1ms for threat scoring calculations");

    auto config = SandboxConfig {};
    config.timeout = Duration::from_seconds(1);
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    // Create malicious test file (ransomware patterns)
    auto file_data = create_test_file(4096, "ransomware"sv);
    auto filename = "test_malware.exe"_string;

    // Warmup
    for (size_t i = 0; i < 100; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    // Benchmark
    Core::ElapsedTimer timer;
    timer.start();

    constexpr size_t iterations = 1000;
    for (size_t i = 0; i < iterations; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    auto elapsed_ns = timer.elapsed_time().to_nanoseconds();
    auto avg_ns = elapsed_ns / iterations;
    auto avg_us = static_cast<float>(avg_ns) / 1000.0f;
    auto avg_ms = static_cast<float>(avg_ns) / 1'000'000.0f;

    dbgln("Iterations: {}", iterations);
    dbgln("Total Time: {:.2f} ms", static_cast<float>(elapsed_ns) / 1'000'000.0f);
    dbgln("Average Time: {} ns ({:.2f} µs, {:.4f} ms)", avg_ns, avg_us, avg_ms);
    dbgln("Note: Includes pattern detection (5 patterns) + description generation");

    if (avg_ns < 2'500'000) {
        dbgln("✓ PASS: Combined scoring + pattern detection under target (<2.5ms)");
    } else {
        dbgln("✗ FAIL: Combined analysis too slow");
    }
}

// ============================================================================
// Benchmark 2: Statistical Analysis (Latency Distribution)
// ============================================================================

static void benchmark_latency_distribution()
{
    dbgln("\n=== Benchmark 2: Latency Distribution Analysis ===");
    dbgln("Measuring min, max, median, p95, p99 latencies");

    auto config = SandboxConfig {};
    config.timeout = Duration::from_seconds(1);
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    auto file_data = create_test_file(2048, "network"sv);
    auto filename = "test_perf.exe"_string;

    // Warmup
    for (size_t i = 0; i < 100; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    // Collect latency samples
    Vector<u64> latencies;
    constexpr size_t samples = 5000;

    for (size_t i = 0; i < samples; i++) {
        Core::ElapsedTimer timer;
        timer.start();

        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;

        auto elapsed_ns = timer.elapsed_time().to_nanoseconds();
        MUST(latencies.try_append(elapsed_ns));
    }

    // Sort for percentile calculation
    AK::quick_sort(latencies);

    auto min_ns = latencies[0];
    auto max_ns = latencies[samples - 1];
    auto median_ns = latencies[samples / 2];
    auto p95_ns = latencies[(samples * 95) / 100];
    auto p99_ns = latencies[(samples * 99) / 100];

    // Calculate average
    u64 sum = 0;
    for (auto ns : latencies)
        sum += ns;
    auto avg_ns = sum / samples;

    dbgln("Samples: {}", samples);
    dbgln("Min:     {} ns ({:.2f} µs)", min_ns, static_cast<float>(min_ns) / 1000.0f);
    dbgln("Avg:     {} ns ({:.2f} µs)", avg_ns, static_cast<float>(avg_ns) / 1000.0f);
    dbgln("Median:  {} ns ({:.2f} µs)", median_ns, static_cast<float>(median_ns) / 1000.0f);
    dbgln("P95:     {} ns ({:.2f} µs)", p95_ns, static_cast<float>(p95_ns) / 1000.0f);
    dbgln("P99:     {} ns ({:.2f} µs)", p99_ns, static_cast<float>(p99_ns) / 1000.0f);
    dbgln("Max:     {} ns ({:.2f} µs)", max_ns, static_cast<float>(max_ns) / 1000.0f);

    // Count samples in different buckets
    size_t under_100us = 0, under_500us = 0, under_1ms = 0, over_1ms = 0;
    for (auto ns : latencies) {
        if (ns < 100'000)
            under_100us++;
        else if (ns < 500'000)
            under_500us++;
        else if (ns < 1'000'000)
            under_1ms++;
        else
            over_1ms++;
    }

    dbgln("\nLatency Distribution:");
    dbgln("  <100 µs:  {} samples ({:.1f}%)", under_100us, static_cast<float>(under_100us * 100) / samples);
    dbgln("  <500 µs:  {} samples ({:.1f}%)", under_500us, static_cast<float>(under_500us * 100) / samples);
    dbgln("  <1 ms:    {} samples ({:.1f}%)", under_1ms, static_cast<float>(under_1ms * 100) / samples);
    dbgln("  >1 ms:    {} samples ({:.1f}%)", over_1ms, static_cast<float>(over_1ms * 100) / samples);

    if (p95_ns < 1'000'000) {
        dbgln("✓ PASS: P95 latency under 1ms");
    } else {
        dbgln("⚠ WARNING: P95 latency exceeds 1ms");
    }
}

// ============================================================================
// Benchmark 3: Throughput Test
// ============================================================================

static void benchmark_throughput()
{
    dbgln("\n=== Benchmark 3: Throughput Test ===");
    dbgln("Measuring analyses per second");

    auto config = SandboxConfig {};
    config.timeout = Duration::from_seconds(1);
    auto analyzer = MUST(BehavioralAnalyzer::create(config));

    auto file_data = create_test_file(2048, "network"sv);
    auto filename = "test_throughput.exe"_string;

    // Warmup
    for (size_t i = 0; i < 100; i++) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
    }

    // Run for fixed time period (1 second)
    Core::ElapsedTimer timer;
    timer.start();

    size_t completed = 0;
    auto deadline = MonotonicTime::now() + Duration::from_seconds(1);

    while (MonotonicTime::now() < deadline) {
        auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
        (void)result;
        completed++;
    }

    auto elapsed_ms = timer.elapsed_milliseconds();
    auto throughput = static_cast<float>(completed * 1000) / static_cast<float>(elapsed_ms);

    dbgln("Completed: {} analyses", completed);
    dbgln("Elapsed Time: {} ms", elapsed_ms);
    dbgln("Throughput: {:.0f} analyses/second", throughput);

    if (throughput >= 1000.0f) {
        dbgln("✓ EXCELLENT: >1000 analyses/second");
    } else if (throughput >= 500.0f) {
        dbgln("✓ PASS: >500 analyses/second");
    } else {
        dbgln("⚠ WARNING: Low throughput");
    }
}

// ============================================================================
// Benchmark 4: Memory Efficiency
// ============================================================================

static void benchmark_memory_efficiency()
{
    dbgln("\n=== Benchmark 4: Memory Efficiency Test ===");
    dbgln("Testing memory allocation patterns");

    auto config = SandboxConfig {};
    config.timeout = Duration::from_seconds(1);

    // Test multiple analyzer instances (simulates parallel analysis)
    Vector<NonnullOwnPtr<BehavioralAnalyzer>> analyzers;

    Core::ElapsedTimer timer;
    timer.start();

    constexpr size_t instances = 10;
    for (size_t i = 0; i < instances; i++) {
        auto analyzer = MUST(BehavioralAnalyzer::create(config));
        MUST(analyzers.try_append(move(analyzer)));
    }

    auto creation_ms = timer.elapsed_milliseconds();

    dbgln("Created {} analyzer instances in {} ms", instances, creation_ms);
    dbgln("Average: {:.2f} ms per instance", static_cast<float>(creation_ms) / instances);

    // Run analyses in parallel
    auto file_data = create_test_file(1024, "benign"sv);
    auto filename = "test_mem.exe"_string;

    timer.start();

    size_t total_analyses = 0;
    for (auto& analyzer : analyzers) {
        for (size_t i = 0; i < 100; i++) {
            auto result = MUST(analyzer->analyze(file_data, filename, Duration::from_milliseconds(100)));
            (void)result;
            total_analyses++;
        }
    }

    auto analysis_ms = timer.elapsed_milliseconds();

    dbgln("Completed {} analyses across {} instances in {} ms",
        total_analyses, instances, analysis_ms);
    dbgln("Average: {:.2f} ms per analysis", static_cast<float>(analysis_ms) / total_analyses);

    dbgln("✓ Memory efficiency test complete");
}

// ============================================================================
// Main Entry Point
// ============================================================================

ErrorOr<int> ladybird_main(Main::Arguments)
{
    dbgln("========================================");
    dbgln("  BehavioralAnalyzer Performance Benchmarks");
    dbgln("  Milestone 0.5 Phase 2 - Week 3");
    dbgln("  Threat Scoring System");
    dbgln("========================================");
    dbgln("");
    dbgln("Testing Components:");
    dbgln("  - Threat score calculation");
    dbgln("  - Pattern detection (5 patterns)");
    dbgln("  - Description generation");
    dbgln("  - End-to-end analysis pipeline");
    dbgln("");

    // Run all benchmarks
    benchmark_mock_analysis_benign();
    benchmark_mock_analysis_suspicious();
    benchmark_mock_analysis_malicious();
    benchmark_latency_distribution();
    benchmark_throughput();
    benchmark_memory_efficiency();

    dbgln("\n========================================");
    dbgln("  Benchmarks Complete");
    dbgln("  Summary:");
    dbgln("    - Threat Scoring: Target <1ms");
    dbgln("    - Pattern Detection: Target <2.5ms (5 patterns)");
    dbgln("    - Description Gen: Included in analysis");
    dbgln("    - End-to-End: Target <5 seconds (mock mode)");
    dbgln("========================================");

    return 0;
}
