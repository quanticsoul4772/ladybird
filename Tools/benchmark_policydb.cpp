/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Random.h>
#include <AK/StringBuilder.h>
#include <AK/Time.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>
#include <Services/Sentinel/PolicyGraph.h>
#include <stdio.h>

using namespace Sentinel;

struct BenchmarkResult {
    String operation_name;
    size_t iterations;
    Duration total_time;
    Duration avg_time;
    Duration min_time;
    Duration max_time;
    bool target_met { false };
    Duration target_time {};
};

static ErrorOr<String> generate_random_hash()
{
    u8 hash_bytes[32];
    fill_with_random(hash_bytes, 32);

    StringBuilder builder;
    for (size_t i = 0; i < 32; i++)
        builder.appendff("{:02x}", hash_bytes[i]);

    return builder.to_string();
}

static ErrorOr<String> generate_random_url()
{
    u32 random_id = get_random<u32>();
    return String::formatted("https://example{}.com/file{}.bin", random_id % 1000, random_id);
}

static ErrorOr<void> populate_test_data(PolicyGraph& graph, size_t num_policies, size_t num_threats)
{
    outln("Populating test data: {} policies, {} threats", num_policies, num_threats);

    // Create diverse policies
    for (size_t i = 0; i < num_policies; i++) {
        PolicyGraph::Policy policy {};
        policy.rule_name = TRY(String::formatted("test_rule_{}", i));
        policy.created_by = "benchmark"_string;
        policy.created_at = UnixDateTime::now();

        // Vary policy types
        if (i % 3 == 0) {
            // Hash-based policy
            policy.file_hash = TRY(generate_random_hash());
            policy.action = PolicyGraph::PolicyAction::Block;
        } else if (i % 3 == 1) {
            // URL pattern policy
            policy.url_pattern = TRY(String::formatted("https://example{}.com/*", i % 100));
            policy.action = PolicyGraph::PolicyAction::Quarantine;
        } else {
            // Rule name only policy
            policy.action = PolicyGraph::PolicyAction::Block;
        }

        TRY(graph.create_policy(policy));

        if ((i + 1) % 100 == 0)
            outln("  Created {} policies", i + 1);
    }

    // Record threat history
    for (size_t i = 0; i < num_threats; i++) {
        PolicyGraph::ThreatMetadata threat {
            .url = TRY(generate_random_url()),
            .filename = TRY(String::formatted("malware_{}.exe", i)),
            .file_hash = TRY(generate_random_hash()),
            .mime_type = "application/octet-stream"_string,
            .file_size = static_cast<u64>(get_random<u32>() % (10 * 1024 * 1024)),
            .rule_name = TRY(String::formatted("test_rule_{}", i % num_policies)),
            .severity = (i % 3 == 0) ? "high"_string : "medium"_string
        };

        TRY(graph.record_threat(threat, "blocked"_string, {}, "{}"_string));

        if ((i + 1) % 500 == 0)
            outln("  Recorded {} threats", i + 1);
    }

    outln("Test data populated successfully\n");
    return {};
}

static ErrorOr<BenchmarkResult> benchmark_policy_match(
    PolicyGraph& graph,
    size_t iterations,
    String const& test_name,
    Duration target_time)
{
    outln("Running benchmark: {} (iterations={})", test_name, iterations);

    Vector<Duration> timings;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();
    Duration total_time {};

    size_t hits = 0;
    size_t misses = 0;

    // Warmup
    {
        PolicyGraph::ThreatMetadata threat {
            .url = "https://example0.com/file.bin"_string,
            .filename = "test.bin"_string,
            .file_hash = TRY(generate_random_hash()),
            .mime_type = "application/octet-stream"_string,
            .file_size = 1024,
            .rule_name = "test_rule_0"_string,
            .severity = "high"_string
        };
        (void)graph.match_policy(threat);
    }

    // Benchmark runs
    for (size_t i = 0; i < iterations; i++) {
        // Create varied threat scenarios
        PolicyGraph::ThreatMetadata threat {
            .url = TRY(generate_random_url()),
            .filename = TRY(String::formatted("file_{}.bin", i)),
            .file_hash = TRY(generate_random_hash()),
            .mime_type = "application/octet-stream"_string,
            .file_size = static_cast<u64>(1024 + (i * 100)),
            .rule_name = TRY(String::formatted("test_rule_{}", i % 10)),
            .severity = "medium"_string
        };

        auto start_time = MonotonicTime::now();
        auto result = TRY(graph.match_policy(threat));
        auto end_time = MonotonicTime::now();

        if (result.has_value())
            hits++;
        else
            misses++;

        auto elapsed = end_time - start_time;
        timings.append(elapsed);
        total_time = total_time + elapsed;

        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;
    }

    auto avg_time = total_time / iterations;
    bool target_met = avg_time <= target_time;

    outln("  Cache performance: {} hits, {} misses ({:.1f}% hit rate)",
        hits, misses, (hits * 100.0) / iterations);

    return BenchmarkResult {
        .operation_name = test_name,
        .iterations = iterations,
        .total_time = total_time,
        .avg_time = avg_time,
        .min_time = min_time,
        .max_time = max_time,
        .target_met = target_met,
        .target_time = target_time
    };
}

static ErrorOr<BenchmarkResult> benchmark_policy_creation(
    PolicyGraph& graph,
    size_t iterations,
    String const& test_name,
    Duration target_time)
{
    outln("Running benchmark: {} (iterations={})", test_name, iterations);

    Vector<Duration> timings;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();
    Duration total_time {};

    for (size_t i = 0; i < iterations; i++) {
        PolicyGraph::Policy policy {
            .rule_name = TRY(String::formatted("bench_policy_{}", i)),
            .file_hash = TRY(generate_random_hash()),
            .action = PolicyGraph::PolicyAction::Block,
            .created_at = UnixDateTime::now(),
            .created_by = "benchmark"_string
        };

        auto start_time = MonotonicTime::now();
        auto policy_id = TRY(graph.create_policy(policy));
        auto end_time = MonotonicTime::now();

        auto elapsed = end_time - start_time;
        timings.append(elapsed);
        total_time = total_time + elapsed;

        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;

        // Clean up to avoid bloating the database
        if (i % 100 == 99) {
            TRY(graph.delete_policy(policy_id));
        }
    }

    auto avg_time = total_time / iterations;
    bool target_met = avg_time <= target_time;

    return BenchmarkResult {
        .operation_name = test_name,
        .iterations = iterations,
        .total_time = total_time,
        .avg_time = avg_time,
        .min_time = min_time,
        .max_time = max_time,
        .target_met = target_met,
        .target_time = target_time
    };
}

static ErrorOr<BenchmarkResult> benchmark_threat_history_query(
    PolicyGraph& graph,
    size_t iterations,
    String const& test_name,
    Duration target_time)
{
    outln("Running benchmark: {} (iterations={})", test_name, iterations);

    Vector<Duration> timings;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();
    Duration total_time {};

    // Query last 30 days
    auto cutoff = UnixDateTime::from_seconds_since_epoch(
        UnixDateTime::now().seconds_since_epoch() - (30 * 24 * 60 * 60));

    for (size_t i = 0; i < iterations; i++) {
        auto start_time = MonotonicTime::now();
        auto threats = TRY(graph.get_threat_history(cutoff));
        auto end_time = MonotonicTime::now();

        auto elapsed = end_time - start_time;
        timings.append(elapsed);
        total_time = total_time + elapsed;

        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;

        if (i == 0)
            outln("  Found {} threats in history", threats.size());
    }

    auto avg_time = total_time / iterations;
    bool target_met = avg_time <= target_time;

    return BenchmarkResult {
        .operation_name = test_name,
        .iterations = iterations,
        .total_time = total_time,
        .avg_time = avg_time,
        .min_time = min_time,
        .max_time = max_time,
        .target_met = target_met,
        .target_time = target_time
    };
}

static ErrorOr<BenchmarkResult> benchmark_statistics_aggregation(
    PolicyGraph& graph,
    size_t iterations,
    String const& test_name,
    Duration target_time)
{
    outln("Running benchmark: {} (iterations={})", test_name, iterations);

    Vector<Duration> timings;
    Duration min_time = Duration::max();
    Duration max_time = Duration::min();
    Duration total_time {};

    for (size_t i = 0; i < iterations; i++) {
        auto start_time = MonotonicTime::now();

        // Aggregate multiple statistics
        auto policy_count = TRY(graph.get_policy_count());
        auto threat_count = TRY(graph.get_threat_count());
        auto policies = TRY(graph.list_policies());

        auto end_time = MonotonicTime::now();

        auto elapsed = end_time - start_time;
        timings.append(elapsed);
        total_time = total_time + elapsed;

        if (elapsed < min_time)
            min_time = elapsed;
        if (elapsed > max_time)
            max_time = elapsed;

        if (i == 0)
            outln("  Stats: {} policies, {} threats, {} policy records",
                policy_count, threat_count, policies.size());
    }

    auto avg_time = total_time / iterations;
    bool target_met = avg_time <= target_time;

    return BenchmarkResult {
        .operation_name = test_name,
        .iterations = iterations,
        .total_time = total_time,
        .avg_time = avg_time,
        .min_time = min_time,
        .max_time = max_time,
        .target_met = target_met,
        .target_time = target_time
    };
}

static void print_results(Vector<BenchmarkResult> const& results)
{
    outln("\n==================================================");
    outln("DATABASE BENCHMARK RESULTS");
    outln("==================================================\n");

    outln("{:<50} | {:>8} | {:>10} | {:>10} | {:>10} | {:>12} | {:>8}",
        "Operation", "Iters", "Avg (ms)", "Min (ms)", "Max (ms)", "Target (ms)", "Status");
    outln("{:-<50}-+-{:-<8}-+-{:-<10}-+-{:-<10}-+-{:-<10}-+-{:-<12}-+-{:-<8}",
        "", "", "", "", "", "", "");

    for (auto const& result : results) {
        auto status = result.target_met ? "PASS" : "FAIL";
        outln("{:<50} | {:>8} | {:>10.2f} | {:>10.2f} | {:>10.2f} | {:>12.2f} | {:>8}",
            result.operation_name,
            result.iterations,
            result.avg_time.to_milliseconds(),
            result.min_time.to_milliseconds(),
            result.max_time.to_milliseconds(),
            result.target_time.to_milliseconds(),
            status);
    }

    outln("\n==================================================\n");

    // Summary
    size_t passed = 0;
    size_t failed = 0;
    for (auto const& result : results) {
        if (result.target_met)
            passed++;
        else
            failed++;
    }

    outln("Summary: {}/{} tests passed", passed, results.size());
    if (failed > 0)
        outln("WARNING: {} test(s) exceeded target performance", failed);

    outln("");
}

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    bool cleanup = false;
    size_t num_policies = 1000;
    size_t num_threats = 5000;

    Core::ArgsParser args_parser;
    args_parser.add_option(cleanup, "Clean up test database after benchmark", "cleanup", 'c');
    args_parser.add_option(num_policies, "Number of policies to create", "policies", 'p', "count");
    args_parser.add_option(num_threats, "Number of threats to record", "threats", 't', "count");
    args_parser.parse(arguments);

    // Create temporary database directory
    auto temp_db_dir = "/tmp/sentinel_benchmark_db"_string;
    if (FileSystem::exists(temp_db_dir))
        TRY(FileSystem::remove(temp_db_dir, FileSystem::RecursionMode::Allowed));

    outln("Creating benchmark database at: {}", temp_db_dir);
    auto graph = TRY(PolicyGraph::create(temp_db_dir));

    // Populate test data
    TRY(populate_test_data(graph, num_policies, num_threats));

    Vector<BenchmarkResult> results;

    // Run benchmarks with targets from Day 31 plan
    outln("=== Running Database Benchmarks ===\n");

    // Policy matching (target: < 5ms)
    results.append(TRY(benchmark_policy_match(
        graph, 1000, "Policy Match Query"_string, Duration::from_milliseconds(5))));

    // Policy creation (target: < 10ms)
    results.append(TRY(benchmark_policy_creation(
        graph, 500, "Policy Creation"_string, Duration::from_milliseconds(10))));

    // Threat history query (target: < 50ms)
    results.append(TRY(benchmark_threat_history_query(
        graph, 200, "Threat History Query"_string, Duration::from_milliseconds(50))));

    // Statistics aggregation (target: < 100ms)
    results.append(TRY(benchmark_statistics_aggregation(
        graph, 100, "Statistics Aggregation"_string, Duration::from_milliseconds(100))));

    // Print results
    print_results(results);

    // Database statistics
    outln("Database Statistics:");
    outln("-------------------");
    auto final_policy_count = TRY(graph.get_policy_count());
    auto final_threat_count = TRY(graph.get_threat_count());
    outln("Policies: {}", final_policy_count);
    outln("Threats: {}", final_threat_count);
    outln("");

    // Cleanup
    if (cleanup) {
        outln("Cleaning up test database...");
        TRY(FileSystem::remove(temp_db_dir, FileSystem::RecursionMode::Allowed));
        outln("Cleanup complete");
    } else {
        outln("Test database preserved at: {}", temp_db_dir);
        outln("Run with --cleanup to remove it");
    }

    outln("");
    return 0;
}
