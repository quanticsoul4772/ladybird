/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Random.h>
#include <LibCore/System.h>

#include "BehavioralAnalyzer.h"

namespace Sentinel::Sandbox {

ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> BehavioralAnalyzer::create(SandboxConfig const& config)
{
    SyscallFilter default_filter;
    return create(config, default_filter);
}

ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> BehavioralAnalyzer::create(SandboxConfig const& config, SyscallFilter const& filter)
{
    auto analyzer = adopt_own(*new BehavioralAnalyzer(config, filter));
    TRY(analyzer->initialize_sandbox());
    return analyzer;
}

BehavioralAnalyzer::BehavioralAnalyzer(SandboxConfig const& config, SyscallFilter const& filter)
    : m_config(config)
    , m_syscall_filter(filter)
{
}

BehavioralAnalyzer::~BehavioralAnalyzer()
{
    // Cleanup sandbox resources
    if (!m_sandbox_dir.is_empty()) {
        // TODO: Remove temporary sandbox directory
        dbgln_if(false, "BehavioralAnalyzer: Cleaning up sandbox dir: {}", m_sandbox_dir);
    }
}

ErrorOr<void> BehavioralAnalyzer::initialize_sandbox()
{
    dbgln_if(false, "BehavioralAnalyzer: Initializing native sandbox");

    // Try to detect nsjail availability
    // For now, use mock implementation (Phase 1a)
    m_use_mock = true;

    // Create temporary sandbox directory
    // TODO: Use proper temp directory creation
    m_sandbox_dir = "/tmp/sentinel-sandbox"_string;

    if (m_use_mock) {
        dbgln("BehavioralAnalyzer: Using mock implementation (nsjail not available)");
    } else {
        TRY(setup_seccomp_filter());
        dbgln_if(false, "BehavioralAnalyzer: nsjail sandbox initialized");
    }

    return {};
}

ErrorOr<void> BehavioralAnalyzer::setup_seccomp_filter()
{
    // TODO: Configure seccomp-BPF filter based on SyscallFilter
    // Allow: read, write, open, close, mmap (basics)
    // Monitor: exec, fork, socket, connect (suspicious)
    // Block: reboot, mount, ptrace (dangerous)

    dbgln_if(false, "BehavioralAnalyzer: seccomp filter configured");
    return {};
}

ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze(
    ByteBuffer const& file_data,
    String const& filename,
    Duration timeout)
{
    dbgln_if(false, "BehavioralAnalyzer: Analyzing '{}' ({} bytes) with timeout {}ms",
        filename, file_data.size(), timeout.to_milliseconds());

    auto start_time = MonotonicTime::now();
    m_stats.total_analyses++;

    BehavioralMetrics metrics;

    // Execute using mock or nsjail based on availability
    if (m_use_mock) {
        auto mock_result = TRY(analyze_mock(file_data, filename));
        metrics = move(mock_result);
    } else {
        auto nsjail_result = TRY(analyze_nsjail(file_data, filename));
        metrics = move(nsjail_result);
    }

    metrics.execution_time = MonotonicTime::now() - start_time;

    // Check for timeout
    if (metrics.execution_time > timeout) {
        metrics.timed_out = true;
        m_stats.timeouts++;
        dbgln("BehavioralAnalyzer: Analysis timed out after {}ms", metrics.execution_time.to_milliseconds());
    }

    // Calculate threat score and generate explanations
    metrics.threat_score = TRY(calculate_threat_score(metrics));
    metrics.suspicious_behaviors = TRY(generate_suspicious_behaviors(metrics));

    // Update statistics
    if (m_stats.total_analyses > 0) {
        m_stats.average_execution_time = Duration::from_nanoseconds(
            (m_stats.average_execution_time.to_nanoseconds() * (m_stats.total_analyses - 1) + metrics.execution_time.to_nanoseconds()) / m_stats.total_analyses);
    }

    if (metrics.execution_time > m_stats.max_execution_time)
        m_stats.max_execution_time = metrics.execution_time;

    dbgln_if(false, "BehavioralAnalyzer: Analysis complete in {}ms - Threat score: {:.2f}, Behaviors: {}",
        metrics.execution_time.to_milliseconds(), metrics.threat_score, metrics.suspicious_behaviors.size());

    return metrics;
}

ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_mock(ByteBuffer const& file_data, [[maybe_unused]] String const& filename)
{
    // Mock implementation: Heuristic behavioral analysis without actual execution
    // This simulates what Tier 2 would detect based on static analysis

    dbgln_if(false, "BehavioralAnalyzer: Using mock implementation");

    BehavioralMetrics metrics;

    // Analyze different behavioral categories
    TRY(analyze_file_behavior(file_data, metrics));
    TRY(analyze_process_behavior(file_data, metrics));
    TRY(analyze_network_behavior(file_data, metrics));

    metrics.exit_code = 0;
    metrics.timed_out = false;

    return metrics;
}

ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail([[maybe_unused]] ByteBuffer const& file_data, [[maybe_unused]] String const& filename)
{
    // TODO: Real nsjail implementation
    // 1. Write file to sandbox directory
    // 2. Launch nsjail with seccomp filter
    // 3. Start syscall tracing with ptrace/eBPF
    // 4. Monitor execution for suspicious behavior
    // 5. Collect metrics and kill sandbox

    BehavioralMetrics metrics;
    // ... nsjail execution code ...
    return metrics;
}

ErrorOr<void> BehavioralAnalyzer::analyze_file_behavior(ByteBuffer const& file_data, BehavioralMetrics& metrics)
{
    // Simulate file system behavior analysis

    // Heuristic: PE/ELF files likely perform file operations
    if (file_data.size() > 2 && file_data[0] == 'M' && file_data[1] == 'Z') {
        // Windows PE
        metrics.file_operations = 5 + (get_random_uniform(10)); // Simulated
        metrics.executable_drops = get_random_uniform(3);
    } else if (file_data.size() > 4 && file_data[0] == 0x7F && file_data[1] == 'E') {
        // Linux ELF
        metrics.file_operations = 3 + (get_random_uniform(8));
    }

    // Heuristic: Scripts with temp file patterns
    StringView content { file_data.bytes() };
    if (content.contains("/tmp/"sv) || content.contains("%TEMP%"sv)) {
        metrics.temp_file_creates = 1 + (get_random_uniform(5));
    }

    if (content.contains("hidden"sv) || content.contains("/."sv)) {
        metrics.hidden_file_creates = get_random_uniform(3);
    }

    dbgln_if(false, "BehavioralAnalyzer: File behavior - Ops: {}, Temp: {}, Hidden: {}, Exec: {}",
        metrics.file_operations, metrics.temp_file_creates, metrics.hidden_file_creates, metrics.executable_drops);

    return {};
}

ErrorOr<void> BehavioralAnalyzer::analyze_process_behavior(ByteBuffer const& file_data, BehavioralMetrics& metrics)
{
    // Simulate process/execution behavior analysis

    StringView content { file_data.bytes() };

    // Heuristic: Process creation keywords
    if (content.contains("CreateProcess"sv) || content.contains("exec"sv) || content.contains("fork"sv)) {
        metrics.process_operations = 1 + (get_random_uniform(5));
    }

    // Heuristic: Self-modification (packers, runtime code generation)
    if (content.contains("VirtualProtect"sv) || content.contains("mprotect"sv)) {
        metrics.self_modification_attempts = get_random_uniform(3);
    }

    // Heuristic: Persistence mechanisms
    if (content.contains("crontab"sv) || content.contains("Startup"sv) || content.contains("RunOnce"sv)) {
        metrics.persistence_mechanisms = 1 + (get_random_uniform(4));
    }

    dbgln_if(false, "BehavioralAnalyzer: Process behavior - Ops: {}, Self-mod: {}, Persistence: {}",
        metrics.process_operations, metrics.self_modification_attempts, metrics.persistence_mechanisms);

    return {};
}

ErrorOr<void> BehavioralAnalyzer::analyze_network_behavior(ByteBuffer const& file_data, BehavioralMetrics& metrics)
{
    // Simulate network behavior analysis

    StringView content { file_data.bytes() };

    // Heuristic: Network API calls
    if (content.contains("socket"sv) || content.contains("connect"sv) || content.contains("send"sv)) {
        metrics.network_operations = 2 + (get_random_uniform(8));
    }

    // Heuristic: Outbound connections (count IPs)
    size_t ip_count = 0;
    if (content.contains("192.168."sv)) ip_count++;
    if (content.contains("10."sv)) ip_count++;
    if (content.contains("http://"sv)) ip_count += 2;
    if (content.contains("https://"sv)) ip_count += 1;
    metrics.outbound_connections = static_cast<u32>(ip_count);

    // Heuristic: DNS queries
    if (content.contains(".com"sv) || content.contains(".org"sv) || content.contains(".net"sv)) {
        metrics.dns_queries = 1 + (get_random_uniform(5));
    }

    // Heuristic: HTTP requests
    if (content.contains("GET"sv) || content.contains("POST"sv) || content.contains("User-Agent"sv)) {
        metrics.http_requests = 1 + (get_random_uniform(10));
    }

    dbgln_if(false, "BehavioralAnalyzer: Network behavior - Ops: {}, Connections: {}, DNS: {}, HTTP: {}",
        metrics.network_operations, metrics.outbound_connections, metrics.dns_queries, metrics.http_requests);

    return {};
}

ErrorOr<float> BehavioralAnalyzer::calculate_threat_score(BehavioralMetrics const& metrics)
{
    // Multi-factor scoring based on BEHAVIORAL_ANALYSIS_SPEC.md
    // Each category contributes to final score (0.0-1.0)

    float score = 0.0f;

    // Category 1: File System (weight: 0.25)
    float file_score = 0.0f;
    if (metrics.file_operations > 10) file_score += 0.3f;
    if (metrics.temp_file_creates > 3) file_score += 0.3f;
    if (metrics.hidden_file_creates > 0) file_score += 0.2f;
    if (metrics.executable_drops > 0) file_score += 0.2f;
    score += min(1.0f, file_score) * 0.25f;

    // Category 2: Process (weight: 0.25)
    float process_score = 0.0f;
    if (metrics.process_operations > 5) process_score += 0.3f;
    if (metrics.self_modification_attempts > 0) process_score += 0.4f;
    if (metrics.persistence_mechanisms > 0) process_score += 0.3f;
    score += min(1.0f, process_score) * 0.25f;

    // Category 3: Network (weight: 0.25)
    float network_score = 0.0f;
    if (metrics.network_operations > 5) network_score += 0.2f;
    if (metrics.outbound_connections > 3) network_score += 0.3f;
    if (metrics.dns_queries > 5) network_score += 0.2f;
    if (metrics.http_requests > 5) network_score += 0.3f;
    score += min(1.0f, network_score) * 0.25f;

    // Category 4: System/Registry (weight: 0.15, Windows-specific)
    float system_score = 0.0f;
    if (metrics.registry_operations > 5) system_score += 0.3f;
    if (metrics.service_modifications > 0) system_score += 0.4f;
    if (metrics.privilege_escalation_attempts > 0) system_score += 0.3f;
    score += min(1.0f, system_score) * 0.15f;

    // Category 5: Memory (weight: 0.10)
    float memory_score = 0.0f;
    if (metrics.memory_operations > 5) memory_score += 0.5f;
    if (metrics.code_injection_attempts > 0) memory_score += 0.5f;
    score += min(1.0f, memory_score) * 0.10f;

    return min(1.0f, score);
}

ErrorOr<Vector<String>> BehavioralAnalyzer::generate_suspicious_behaviors(BehavioralMetrics const& metrics)
{
    Vector<String> behaviors;

    // File system suspicions
    if (metrics.file_operations > 10)
        TRY(behaviors.try_append(TRY(String::formatted("Excessive file operations: {}", metrics.file_operations))));
    if (metrics.temp_file_creates > 3)
        TRY(behaviors.try_append(TRY(String::formatted("Multiple temp file creations: {}", metrics.temp_file_creates))));
    if (metrics.hidden_file_creates > 0)
        TRY(behaviors.try_append(TRY(String::formatted("Hidden file creation: {}", metrics.hidden_file_creates))));
    if (metrics.executable_drops > 0)
        TRY(behaviors.try_append(TRY(String::formatted("Executable dropped: {}", metrics.executable_drops))));

    // Process suspicions
    if (metrics.process_operations > 5)
        TRY(behaviors.try_append(TRY(String::formatted("Multiple process spawns: {}", metrics.process_operations))));
    if (metrics.self_modification_attempts > 0)
        TRY(behaviors.try_append("Self-modification detected (possible packer/obfuscation)"_string));
    if (metrics.persistence_mechanisms > 0)
        TRY(behaviors.try_append("Persistence mechanism installed (autostart/cron)"_string));

    // Network suspicions
    if (metrics.network_operations > 5)
        TRY(behaviors.try_append(TRY(String::formatted("Network activity: {} operations", metrics.network_operations))));
    if (metrics.outbound_connections > 3)
        TRY(behaviors.try_append(TRY(String::formatted("Multiple outbound connections: {}", metrics.outbound_connections))));
    if (metrics.dns_queries > 5)
        TRY(behaviors.try_append(TRY(String::formatted("Suspicious DNS queries: {}", metrics.dns_queries))));

    // System/Memory suspicions
    if (metrics.registry_operations > 5)
        TRY(behaviors.try_append("Registry modifications detected"_string));
    if (metrics.service_modifications > 0)
        TRY(behaviors.try_append("Service/daemon modification attempted"_string));
    if (metrics.privilege_escalation_attempts > 0)
        TRY(behaviors.try_append("Privilege escalation attempted"_string));
    if (metrics.code_injection_attempts > 0)
        TRY(behaviors.try_append("Code injection detected"_string));

    return behaviors;
}

void BehavioralAnalyzer::reset_statistics()
{
    m_stats = Statistics {};
    dbgln_if(false, "BehavioralAnalyzer: Statistics reset");
}

}
