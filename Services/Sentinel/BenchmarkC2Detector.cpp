/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "C2Detector.h"
#include <AK/Time.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/System.h>
#include <LibMain/Main.h>

static void benchmark_beaconing_analysis()
{
    dbgln("\n=== Benchmark: Beaconing Analysis ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    // Prepare test data (regular intervals)
    Vector<float> intervals;
    for (size_t i = 0; i < 100; i++)
        intervals.append(30.0f + (i % 3) * 0.1f);  // Small variation

    // Warmup
    for (size_t i = 0; i < 100; i++)
        (void)detector->analyze_beaconing(intervals);

    // Benchmark
    Core::ElapsedTimer timer;
    timer.start();

    size_t iterations = 10000;
    for (size_t i = 0; i < iterations; i++) {
        auto result = detector->analyze_beaconing(intervals);
        (void)result;
    }

    auto elapsed_ms = timer.elapsed_milliseconds();
    float avg_time_us = (elapsed_ms * 1000.0f) / static_cast<float>(iterations);

    dbgln("Iterations: {}", iterations);
    dbgln("Total Time: {} ms", elapsed_ms);
    dbgln("Average Time: {:.2f} µs ({:.4f} ms)", avg_time_us, avg_time_us / 1000.0f);

    if (avg_time_us / 1000.0f < 2.0f) {
        dbgln("✓ PASS: Performance target met (<2ms)");
    } else {
        dbgln("✗ WARNING: Performance target missed (>2ms)");
    }
}

static void benchmark_exfiltration_analysis()
{
    dbgln("\n=== Benchmark: Exfiltration Analysis ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    u64 bytes_sent = 50 * 1024 * 1024;
    u64 bytes_received = 5 * 1024 * 1024;
    auto domain = "test-domain.com"sv;

    // Warmup
    for (size_t i = 0; i < 100; i++)
        (void)detector->analyze_exfiltration(bytes_sent, bytes_received, domain);

    // Benchmark
    Core::ElapsedTimer timer;
    timer.start();

    size_t iterations = 10000;
    for (size_t i = 0; i < iterations; i++) {
        auto result = detector->analyze_exfiltration(bytes_sent, bytes_received, domain);
        (void)result;
    }

    auto elapsed_ms = timer.elapsed_milliseconds();
    float avg_time_us = (elapsed_ms * 1000.0f) / static_cast<float>(iterations);

    dbgln("Iterations: {}", iterations);
    dbgln("Total Time: {} ms", elapsed_ms);
    dbgln("Average Time: {:.2f} µs ({:.4f} ms)", avg_time_us, avg_time_us / 1000.0f);

    if (avg_time_us / 1000.0f < 2.0f) {
        dbgln("✓ PASS: Performance target met (<2ms)");
    } else {
        dbgln("✗ WARNING: Performance target missed (>2ms)");
    }
}

static void benchmark_known_service_lookup()
{
    dbgln("\n=== Benchmark: Known Service Lookup ===");

    auto detector = MUST(Sentinel::C2Detector::create());

    u64 bytes_sent = 50 * 1024 * 1024;
    u64 bytes_received = 5 * 1024 * 1024;

    // Test various known services
    Vector<StringView> domains = {
        "drive.google.com"sv,
        "s3.amazonaws.com"sv,
        "github.com"sv,
        "dropbox.com"sv,
        "unknown-evil-domain.com"sv
    };

    Core::ElapsedTimer timer;
    timer.start();

    size_t iterations = 10000;
    for (size_t i = 0; i < iterations; i++) {
        for (auto const& domain : domains) {
            auto result = detector->analyze_exfiltration(bytes_sent, bytes_received, domain);
            (void)result;
        }
    }

    auto elapsed_ms = timer.elapsed_milliseconds();
    size_t total_lookups = iterations * domains.size();
    float avg_time_us = (elapsed_ms * 1000.0f) / static_cast<float>(total_lookups);

    dbgln("Iterations: {} (x{} domains)", iterations, domains.size());
    dbgln("Total Lookups: {}", total_lookups);
    dbgln("Total Time: {} ms", elapsed_ms);
    dbgln("Average Time per Lookup: {:.2f} µs", avg_time_us);

    if (avg_time_us / 1000.0f < 0.1f) {
        dbgln("✓ PASS: Domain lookup very fast (<0.1ms)");
    }
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    dbgln("========================================");
    dbgln("  C2Detector Performance Benchmarks");
    dbgln("  Milestone 0.4 Phase 6");
    dbgln("========================================");

    benchmark_beaconing_analysis();
    benchmark_exfiltration_analysis();
    benchmark_known_service_lookup();

    dbgln("\n========================================");
    dbgln("  Benchmarks Complete");
    dbgln("  Target: <2ms per analysis");
    dbgln("========================================");

    return 0;
}
