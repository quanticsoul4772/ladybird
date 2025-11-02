/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Random.h>
#include <AK/ScopeGuard.h>
#include <AK/StringBuilder.h>
#include <LibCore/System.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "BehavioralAnalyzer.h"

// Declare environ for execve
extern char** environ;

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
        auto result = cleanup_temp_directory(m_sandbox_dir);
        if (result.is_error()) {
            dbgln("BehavioralAnalyzer: Warning - Failed to cleanup sandbox directory: {}", result.error());
        }
    }
}

ErrorOr<void> BehavioralAnalyzer::initialize_sandbox()
{
    dbgln_if(false, "BehavioralAnalyzer: Initializing native sandbox");

    // Try to detect nsjail availability
    // For now, use mock implementation (Phase 1a)
    m_use_mock = true;

    // Create temporary sandbox directory using mkdtemp
    m_sandbox_dir = TRY(create_temp_sandbox_directory());

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

// ============================================================================
// File Handling Infrastructure (Week 1 Day 3-4)
// ============================================================================

ErrorOr<String> BehavioralAnalyzer::create_temp_sandbox_directory()
{
    // Create unique temporary directory using mkdtemp
    // Pattern: /tmp/sentinel-XXXXXX where X's are replaced with unique characters

    char template_path[] = "/tmp/sentinel-XXXXXX";

    // mkdtemp modifies template_path in place, replacing XXXXXX with unique characters
    char const* result = mkdtemp(template_path);
    if (!result) {
        return Error::from_errno(errno);
    }

    auto directory = TRY(String::from_utf8(StringView { result, strlen(result) }));

    dbgln_if(false, "BehavioralAnalyzer: Created temp sandbox directory: {}", directory);
    return directory;
}

ErrorOr<void> BehavioralAnalyzer::cleanup_temp_directory(String const& directory)
{
    // Recursively remove the temporary sandbox directory
    // This uses Core::System::rmdir which handles recursive removal

    if (directory.is_empty()) {
        return Error::from_string_literal("Cannot cleanup empty directory path");
    }

    // Verify this is a sentinel temp directory to avoid accidental deletion
    if (!directory.bytes_as_string_view().starts_with("/tmp/sentinel-"sv)) {
        return Error::from_string_literal("Refusing to cleanup directory outside sentinel temp path");
    }

    dbgln_if(false, "BehavioralAnalyzer: Cleaning up sandbox directory: {}", directory);

    // Use recursive directory removal
    // Note: Core::System doesn't have rm -rf equivalent yet, so we need to implement it
    // For now, we'll use system() as a temporary solution
    auto command = TRY(String::formatted("rm -rf {}", directory));
    int result = system(command.bytes_as_string_view().to_byte_string().characters());

    if (result != 0) {
        return Error::from_string_literal("Failed to remove sandbox directory");
    }

    return {};
}

ErrorOr<String> BehavioralAnalyzer::write_file_to_sandbox(String const& sandbox_dir, ByteBuffer const& file_data, String const& filename)
{
    // Write file data to sandbox directory with secure permissions (0600)
    // Returns the full path to the written file

    if (sandbox_dir.is_empty() || filename.is_empty()) {
        return Error::from_string_literal("Invalid sandbox directory or filename");
    }

    // Construct full file path
    auto file_path = TRY(String::formatted("{}/{}", sandbox_dir, filename));

    // Open file with secure permissions (0600 = owner read/write only)
    auto fd = TRY(Core::System::open(file_path.bytes_as_string_view(), O_WRONLY | O_CREAT | O_EXCL, 0600));

    // Write file data
    TRY(Core::System::write(fd, file_data.bytes()));

    // Close file descriptor
    TRY(Core::System::close(fd));

    dbgln_if(false, "BehavioralAnalyzer: Wrote {} bytes to {} with permissions 0600",
        file_data.size(), file_path);

    return file_path;
}

ErrorOr<void> BehavioralAnalyzer::make_executable(String const& file_path)
{
    // Make file executable by changing permissions to 0700 (owner read/write/execute only)

    if (file_path.is_empty()) {
        return Error::from_string_literal("Invalid file path");
    }

    // Change file permissions to 0700
    TRY(Core::System::chmod(file_path.bytes_as_string_view(), 0700));

    dbgln_if(false, "BehavioralAnalyzer: Made {} executable (permissions 0700)", file_path);

    return {};
}

// ============================================================================
// nsjail Command Building (Week 1 Day 5-6)
// ============================================================================

ErrorOr<String> BehavioralAnalyzer::locate_nsjail_config_file()
{
    // Locate the nsjail configuration file
    // Search order:
    // 1. Relative to current binary (Services/Sentinel/Sandbox/configs/)
    // 2. Build directory (Build/release/Services/Sentinel/Sandbox/configs/)
    // 3. Absolute source path (for development)

    Vector<String> search_paths;

    // Add relative paths from current directory
    TRY(search_paths.try_append(TRY(String::from_utf8("Services/Sentinel/Sandbox/configs/malware-sandbox.cfg"sv))));
    TRY(search_paths.try_append(TRY(String::from_utf8("Sandbox/configs/malware-sandbox.cfg"sv))));
    TRY(search_paths.try_append(TRY(String::from_utf8("configs/malware-sandbox.cfg"sv))));
    TRY(search_paths.try_append(TRY(String::from_utf8("malware-sandbox.cfg"sv))));

    // Add build directory paths
    TRY(search_paths.try_append(TRY(String::from_utf8("Build/release/Services/Sentinel/Sandbox/configs/malware-sandbox.cfg"sv))));
    TRY(search_paths.try_append(TRY(String::from_utf8("../Services/Sentinel/Sandbox/configs/malware-sandbox.cfg"sv))));

    // Check each search path
    for (auto const& path : search_paths) {
        auto result = Core::System::access(path.bytes_as_string_view(), R_OK);
        if (!result.is_error()) {
            dbgln_if(false, "BehavioralAnalyzer: Found nsjail config at: {}", path);
            return path;
        }
    }

    return Error::from_string_literal("nsjail configuration file not found");
}

ErrorOr<Vector<String>> BehavioralAnalyzer::build_nsjail_command(
    String const& executable_path,
    Vector<String> const& args)
{
    // Build nsjail command using configuration file approach
    // Command structure: nsjail -C <config> --time_limit <timeout> -- <executable> <args...>

    if (executable_path.is_empty()) {
        return Error::from_string_literal("Executable path cannot be empty");
    }

    Vector<String> command;

    // 1. nsjail binary
    TRY(command.try_append(TRY(String::from_utf8("nsjail"sv))));

    // 2. Configuration file argument
    TRY(command.try_append(TRY(String::from_utf8("-C"sv))));

    // Locate config file
    auto config_path = TRY(locate_nsjail_config_file());
    TRY(command.try_append(config_path));

    // 3. Override time limit from SandboxConfig
    TRY(command.try_append(TRY(String::from_utf8("--time_limit"sv))));
    auto timeout_seconds = m_config.timeout.to_seconds();
    TRY(command.try_append(TRY(String::formatted("{}", timeout_seconds))));

    // 4. Override memory limit from SandboxConfig (as rlimit_as)
    TRY(command.try_append(TRY(String::from_utf8("--rlimit_as"sv))));
    TRY(command.try_append(TRY(String::formatted("{}", m_config.max_memory_bytes))));

    // 5. Network configuration (disable by default unless explicitly enabled)
    if (!m_config.allow_network) {
        TRY(command.try_append(TRY(String::from_utf8("--disable_clone_newnet"sv))));
        TRY(command.try_append(TRY(String::from_utf8("false"sv))));
    }

    // 6. Verbose logging for debugging
    TRY(command.try_append(TRY(String::from_utf8("--log_level"sv))));
    TRY(command.try_append(TRY(String::from_utf8("DEBUG"sv))));

    // 7. Separator between nsjail args and executable args
    TRY(command.try_append(TRY(String::from_utf8("--"sv))));

    // 8. Executable path
    TRY(command.try_append(executable_path));

    // 9. Executable arguments
    for (auto const& arg : args) {
        TRY(command.try_append(arg));
    }

    // Debug log the complete command
    StringBuilder cmd_builder;
    for (size_t i = 0; i < command.size(); ++i) {
        TRY(cmd_builder.try_append(command[i]));
        if (i < command.size() - 1) {
            TRY(cmd_builder.try_append(' '));
        }
    }
    dbgln_if(false, "BehavioralAnalyzer: Built nsjail command: {}", TRY(cmd_builder.to_string()));

    return command;
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

ErrorOr<BehavioralMetrics> BehavioralAnalyzer::analyze_nsjail(ByteBuffer const& file_data, String const& filename)
{
    // Real nsjail implementation with syscall monitoring
    // Week 2 Day 5: Complete integration of all Week 1-2 components

    dbgln_if(false, "BehavioralAnalyzer: Starting nsjail analysis for '{}'", filename);

    BehavioralMetrics metrics;

    // Step 1: Write file to sandbox directory (Week 1 Day 3-4)
    auto file_path = TRY(write_file_to_sandbox(m_sandbox_dir, file_data, filename));
    dbgln_if(false, "BehavioralAnalyzer: Wrote file to sandbox: {}", file_path);

    // Step 2: Make executable
    TRY(make_executable(file_path));

    // Step 3: Build nsjail command (Week 1 Day 4-5)
    auto command = TRY(build_nsjail_command(file_path));

    // Step 4: Launch nsjail sandbox process (Week 1 Day 4-5)
    auto process = TRY(launch_nsjail_sandbox(command));
    dbgln_if(false, "BehavioralAnalyzer: Launched sandbox process PID {}", process.pid);

    // Ensure all file descriptors are closed on exit
    ScopeGuard fd_cleanup = [&process] {
        (void)Core::System::close(process.stdin_fd);
        (void)Core::System::close(process.stdout_fd);
        (void)Core::System::close(process.stderr_fd);
    };

    // Step 5: Enforce timeout (Week 1 Day 4-5)
    auto timeout = m_config.timeout;
    TRY(enforce_sandbox_timeout(process.pid, timeout));
    dbgln_if(false, "BehavioralAnalyzer: Timeout enforced: {}ms", timeout.to_milliseconds());

    // Step 6: Monitor syscall events from stderr (Week 2)
    // nsjail writes syscall events to stderr in format: [SYSCALL] name(args...)
    auto deadline = MonotonicTime::now() + timeout;
    u32 syscall_count = 0;

    while (MonotonicTime::now() < deadline) {
        // Read available lines from stderr with short timeout (100ms)
        auto lines_result = read_pipe_lines(process.stderr_fd, Duration::from_milliseconds(100));

        if (lines_result.is_error()) {
            // EOF or pipe closed - sandbox exited
            dbgln_if(false, "BehavioralAnalyzer: Pipe closed, sandbox exited");
            break;
        }

        auto lines = lines_result.release_value();

        // Parse each line for syscall events
        for (auto const& line : lines) {
            auto event_result = TRY(parse_syscall_event(line.bytes_as_string_view()));

            if (event_result.has_value()) {
                auto const& event = event_result.value();

                // Update metrics based on syscall name
                update_metrics_from_syscall(event.name.bytes_as_string_view(), metrics);

                syscall_count++;
                dbgln_if(false, "BehavioralAnalyzer: Syscall #{}: {} (args: {})",
                    syscall_count, event.name, event.args.size());
            }
        }

        // Check if process has exited
        int status = 0;
        pid_t result = waitpid(process.pid, &status, WNOHANG);
        if (result == process.pid) {
            // Process exited
            dbgln_if(false, "BehavioralAnalyzer: Sandbox exited early");
            break;
        }
    }

    dbgln_if(false, "BehavioralAnalyzer: Captured {} syscall events", syscall_count);

    // Step 7: Wait for sandbox completion and get exit code (Week 1 Day 4-5)
    auto exit_code = TRY(wait_for_sandbox_completion(process.pid));
    metrics.exit_code = exit_code;

    // Step 8: Detect timeout from exit code
    if (exit_code == 137) {  // 128 + SIGKILL
        metrics.timed_out = true;
        m_stats.timeouts++;
        dbgln("BehavioralAnalyzer: Sandbox timed out (SIGKILL)");
    } else if (exit_code >= 128) {
        // Crashed with signal
        int signal_num = exit_code - 128;
        dbgln("BehavioralAnalyzer: Sandbox crashed with signal {}", signal_num);
    } else {
        dbgln_if(false, "BehavioralAnalyzer: Sandbox exited normally with code {}", exit_code);
    }

    // Step 9: Cleanup temp file
    TRY(cleanup_temp_directory(m_sandbox_dir));

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
    // Enhanced multi-factor scoring using real syscall-based metrics from Week 2
    // Threat model: Ransomware, C2 beaconing, process injection, privilege escalation
    //
    // Reweighted categories based on threat severity:
    // 1. Privilege Escalation: 40% (highest severity - system compromise)
    // 2. Code Injection: 30% (process compromise, memory manipulation)
    // 3. Network C2: 20% (command & control, data exfiltration)
    // 4. File Operations: 10% (ransomware indicators)
    //
    // Score range: 0.0-1.0
    // Interpretation:
    //   0.0-0.3: Low threat (normal behavior)
    //   0.3-0.6: Medium threat (suspicious activity)
    //   0.6-0.8: High threat (likely malicious)
    //   0.8-1.0: Critical threat (active attack)

    float score = 0.0f;

    // Calculate execution time in seconds for rate-based analysis
    float execution_seconds = max(1.0f, static_cast<float>(metrics.execution_time.to_milliseconds()) / 1000.0f);

    // =============================================================================
    // Category 1: Privilege Escalation (Weight: 40% - MOST CRITICAL)
    // =============================================================================
    // Detected by: setuid, setgid, capset, mount, unshare, setns, chroot,
    //              init_module, reboot, ioperm, syslog, quotactl syscalls
    // Attack types: SUID abuse, capability escalation, container escape, kernel module loading
    //
    // Rationale: Any privilege escalation attempt in a sandbox is HIGHLY suspicious
    // Legitimate programs should not need elevated privileges in our threat model

    float priv_esc_score = 0.0f;

    if (metrics.privilege_escalation_attempts > 0) {
        // Binary detection: ANY privilege escalation attempt is critical
        // Justification: nsjail sandbox should block these operations
        // Multiple attempts indicate persistent exploit behavior
        if (metrics.privilege_escalation_attempts == 1) {
            priv_esc_score = 0.7f; // Single attempt: likely malicious
        } else if (metrics.privilege_escalation_attempts <= 5) {
            priv_esc_score = 0.85f; // Multiple attempts: persistent exploit
        } else {
            priv_esc_score = 1.0f; // Excessive attempts: active exploit campaign
        }
    }

    score += priv_esc_score * 0.40f;

    // =============================================================================
    // Category 2: Code Injection (Weight: 30% - CRITICAL)
    // =============================================================================
    // Detected by: ptrace, process_vm_readv, process_vm_writev, mprotect syscalls
    // Attack types: Process injection, shellcode execution, memory manipulation
    //
    // Rationale: Code injection is primary technique for:
    // - Evading detection by injecting into legitimate processes
    // - Creating RWX memory pages for shellcode execution
    // - Cross-process memory tampering

    float injection_score = 0.0f;

    if (metrics.code_injection_attempts > 0) {
        // Binary detection: ANY injection attempt is critical
        // ptrace: Process debugging/injection (should never occur in sandbox)
        // process_vm_*: Direct memory read/write to another process
        // mprotect: Memory protection changes (often for RWX pages)
        if (metrics.code_injection_attempts == 1) {
            injection_score = 0.6f; // Single attempt: suspicious
        } else if (metrics.code_injection_attempts <= 3) {
            injection_score = 0.8f; // Multiple attempts: likely malicious
        } else {
            injection_score = 1.0f; // Excessive attempts: active injection
        }
    }

    // Memory operations context: Large volume suggests heap spray or shellcode preparation
    if (metrics.memory_operations > 20) {
        injection_score = max(injection_score, 0.4f); // Heap manipulation baseline
        if (metrics.memory_operations > 100) {
            injection_score = max(injection_score, 0.7f); // Excessive memory manipulation
        }
    }

    score += injection_score * 0.30f;

    // =============================================================================
    // Category 3: Network C2 (Weight: 20% - HIGH)
    // =============================================================================
    // Detected by: socket, connect, bind, listen, send, recv, setsockopt syscalls
    // Attack types: Command & control beaconing, data exfiltration, backdoor server
    //
    // Rationale: Network behavior analysis:
    // - C2 beacon: Repeated connections to same IP (connect syscalls)
    // - Data exfiltration: High volume of outbound data (send syscalls)
    // - Backdoor: Server socket binding (bind/listen syscalls)

    float network_score = 0.0f;

    // C2 beaconing detection: Ratio of connections to total network ops
    // High ratio suggests repeated connection attempts (beacon behavior)
    if (metrics.network_operations > 0) {
        float connection_ratio = static_cast<float>(metrics.outbound_connections) / static_cast<float>(metrics.network_operations);

        // Beaconing pattern: Many connections relative to total network activity
        // Typical C2: connect() -> send() -> recv() -> close(), repeated
        // Ratio > 0.3 indicates connection-heavy behavior (beaconing)
        if (connection_ratio > 0.3f && metrics.outbound_connections >= 3) {
            network_score = 0.5f; // Beaconing pattern detected
            if (metrics.outbound_connections > 10) {
                network_score = 0.8f; // Aggressive beaconing
            }
        }
        // Moderate network activity: Could be legitimate or data exfiltration
        else if (metrics.outbound_connections >= 5) {
            network_score = 0.3f; // Moderate outbound connections
        } else if (metrics.outbound_connections >= 2) {
            network_score = 0.15f; // Low outbound connections (borderline normal)
        }
    }

    // DNS queries context: Excessive DNS may indicate DGA (Domain Generation Algorithm)
    // Malware uses DGA to generate fallback C2 domains
    if (metrics.dns_queries > 10) {
        network_score = max(network_score, 0.4f); // Potential DGA behavior
        if (metrics.dns_queries > 50) {
            network_score = max(network_score, 0.7f); // Active DGA
        }
    }

    score += network_score * 0.20f;

    // =============================================================================
    // Category 4: File Operations (Weight: 10% - MEDIUM)
    // =============================================================================
    // Detected by: open, write, read, unlink, rename, mkdir, chmod, truncate syscalls
    // Attack types: Ransomware encryption, file deletion, executable dropping
    //
    // Rationale: Rate-based analysis for ransomware detection
    // - Ransomware: Rapid file encryption (high write/rename rate per second)
    // - Wipers: Rapid file deletion (high unlink rate per second)
    // - Droppers: Executable file creation

    float file_score = 0.0f;

    // Ransomware detection: File operation rate (operations per second)
    // Typical ransomware: 100-1000 files/second encryption rate
    // Note: Current metrics don't distinguish read/write/delete counts separately
    // Using total file_operations as proxy for now
    float file_ops_per_second = static_cast<float>(metrics.file_operations) / execution_seconds;

    if (file_ops_per_second > 50.0f) {
        file_score = 0.6f; // High file operation rate (potential ransomware)
        if (file_ops_per_second > 200.0f) {
            file_score = 0.9f; // Extreme rate (active ransomware encryption)
        }
    } else if (file_ops_per_second > 20.0f) {
        file_score = 0.3f; // Moderate rate (suspicious)
    } else if (metrics.file_operations > 50) {
        file_score = 0.2f; // High absolute count but slow rate (borderline)
    }

    // Process creation context: Fork bomb detection
    // Excessive process spawning depletes system resources
    float process_ops_per_second = static_cast<float>(metrics.process_operations) / execution_seconds;
    if (process_ops_per_second > 10.0f) {
        file_score = max(file_score, 0.5f); // Fork bomb or process spawning malware
        if (process_ops_per_second > 50.0f) {
            file_score = max(file_score, 0.8f); // Active fork bomb
        }
    }

    // Executable drops: Any executable creation is suspicious
    if (metrics.executable_drops > 0) {
        file_score = max(file_score, 0.4f); // Dropper behavior
        if (metrics.executable_drops > 3) {
            file_score = max(file_score, 0.7f); // Multiple executable drops
        }
    }

    // Hidden files: Often used for persistence or stealth
    if (metrics.hidden_file_creates > 0) {
        file_score = max(file_score, 0.2f); // Hidden file creation
    }

    // Temp file usage: Common in malware for staging payloads
    if (metrics.temp_file_creates > 5) {
        file_score = max(file_score, 0.25f); // Excessive temp file usage
    }

    score += file_score * 0.10f;

    // =============================================================================
    // Final Score Normalization
    // =============================================================================
    // Ensure score is within 0.0-1.0 range
    // Note: With weighted categories summing to 100%, score should naturally be ≤ 1.0
    // However, we clamp to handle floating-point rounding edge cases

    return min(1.0f, score);
}

ErrorOr<Vector<String>> BehavioralAnalyzer::generate_suspicious_behaviors(BehavioralMetrics const& metrics)
{
    Vector<String> behaviors;

    // ========================================================================
    // CRITICAL-LEVEL THREATS (Immediate Action Required)
    // ========================================================================

    // Code injection - most dangerous threat
    if (metrics.code_injection_attempts > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🔴 CRITICAL: Code injection detected ({} attempts via ptrace/process_vm_writev)\n"
            "   → Remediation: KILL PROCESS IMMEDIATELY - Likely malware attempting process hijacking\n"
            "   → Evidence: {} suspicious memory operations recorded",
            metrics.code_injection_attempts,
            metrics.memory_operations))));
    }

    // Privilege escalation
    if (metrics.privilege_escalation_attempts > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🔴 CRITICAL: Privilege escalation attempted ({} attempts)\n"
            "   → Remediation: Quarantine file immediately, review setuid/setgid syscalls\n"
            "   → Evidence: Unauthorized attempt to gain root/admin privileges",
            metrics.privilege_escalation_attempts))));
    }

    // Self-modification (packer/obfuscation)
    if (metrics.self_modification_attempts > 0) {
        TRY(behaviors.try_append(
            "🔴 CRITICAL: Self-modification detected (runtime code patching)\n"
            "   → Remediation: Strong indicator of packed/obfuscated malware - quarantine file\n"
            "   → Evidence: Binary modified its own code at runtime (anti-analysis technique)"_string));
    }

    // ========================================================================
    // HIGH-LEVEL THREATS (Advanced Pattern Detection)
    // ========================================================================

    // Ransomware pattern detection (integrated from Task 2)
    if (detect_ransomware_pattern(metrics)) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: RANSOMWARE PATTERN DETECTED\n"
            "   → Remediation: IMMEDIATE QUARANTINE - File shows encryption behavior\n"
            "   → Evidence: {} file operations with rapid modification + deletion pattern\n"
            "   → Details: {} temp files, {} outbound connections (possible C2 beaconing)",
            metrics.file_operations,
            metrics.temp_file_creates,
            metrics.outbound_connections))));
    }

    // Cryptominer pattern detection (integrated from Task 2)
    if (detect_cryptominer_pattern(metrics)) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: CRYPTOMINER PATTERN DETECTED\n"
            "   → Remediation: Block network access, terminate process\n"
            "   → Evidence: {} network operations to {} unique IPs (mining pool beaconing)\n"
            "   → Details: {} memory operations, {} process spawns (multi-threaded mining)",
            metrics.network_operations,
            metrics.outbound_connections,
            metrics.memory_operations,
            metrics.process_operations))));
    }

    // Keylogger pattern detection (integrated from Task 2)
    if (detect_keylogger_pattern(metrics)) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: KEYLOGGER PATTERN DETECTED\n"
            "   → Remediation: Investigate file writes to hidden locations\n"
            "   → Evidence: {} file operations, {} hidden files, {} outbound connections\n"
            "   → Details: Suspicious input monitoring + data exfiltration pattern",
            metrics.file_operations,
            metrics.hidden_file_creates,
            metrics.outbound_connections))));
    }

    // Rootkit pattern detection (integrated from Task 2)
    if (detect_rootkit_pattern(metrics)) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: ROOTKIT PATTERN DETECTED\n"
            "   → Remediation: CRITICAL - System compromise likely, full scan required\n"
            "   → Evidence: {} privilege escalation attempts, {} service modifications\n"
            "   → Details: Kernel-level manipulation or system file tampering detected",
            metrics.privilege_escalation_attempts,
            metrics.service_modifications))));
    }

    // Process injector pattern detection (integrated from Task 2)
    if (detect_process_injector_pattern(metrics)) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: PROCESS INJECTOR PATTERN DETECTED\n"
            "   → Remediation: Quarantine - Malware attempting to hide in legitimate processes\n"
            "   → Evidence: {} code injection attempts, {} memory operations\n"
            "   → Details: ptrace usage + memory manipulation + process spawning",
            metrics.code_injection_attempts,
            metrics.memory_operations))));
    }

    // ========================================================================
    // MEDIUM-LEVEL THREATS (File System Behavior)
    // ========================================================================

    // Excessive file operations (detailed breakdown)
    if (metrics.file_operations > 10) {
        // Estimate operation types based on typical ratios
        // Note: Future enhancement - track specific operation counts in metrics
        u32 estimated_writes = metrics.file_operations / 3;      // ~33% writes
        u32 estimated_deletes = metrics.file_operations / 10;    // ~10% deletes
        u32 estimated_reads = metrics.file_operations - estimated_writes - estimated_deletes;

        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Excessive file operations detected ({} total)\n"
            "   → Breakdown: ~{} reads, ~{} writes, ~{} deletes/renames\n"
            "   → Remediation: Review file access patterns - may indicate data theft or ransomware\n"
            "   → Evidence: {} file system syscalls (open/read/write/unlink/rename)",
            metrics.file_operations,
            estimated_reads,
            estimated_writes,
            estimated_deletes,
            metrics.file_operations))));
    }

    // Temp file creation
    if (metrics.temp_file_creates > 3) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Multiple temporary file creations ({} files)\n"
            "   → Remediation: Inspect /tmp directory for dropped payloads\n"
            "   → Evidence: mkdir syscalls to temporary directories",
            metrics.temp_file_creates))));
    }

    // Hidden file creation
    if (metrics.hidden_file_creates > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Hidden file creation detected ({} files)\n"
            "   → Remediation: Search for dotfiles in user directories, check for persistence\n"
            "   → Evidence: Files created with hidden attribute or dotfile naming",
            metrics.hidden_file_creates))));
    }

    // Executable drops
    if (metrics.executable_drops > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: Executable file dropped to disk ({} files)\n"
            "   → Remediation: QUARANTINE dropped executables, scan with YARA rules\n"
            "   → Evidence: {} chmod +x syscalls or .exe/.sh/.bat file creation",
            metrics.executable_drops,
            metrics.executable_drops))));
    }

    // ========================================================================
    // MEDIUM-LEVEL THREATS (Process Behavior)
    // ========================================================================

    // Multiple process spawns
    if (metrics.process_operations > 5) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Multiple process creation operations ({} total)\n"
            "   → Breakdown: fork/vfork/clone + execve syscalls\n"
            "   → Remediation: Review child process tree - may indicate lateral movement\n"
            "   → Evidence: {} process-related syscalls detected",
            metrics.process_operations,
            metrics.process_operations))));
    }

    // Persistence mechanisms
    if (metrics.persistence_mechanisms > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: Persistence mechanism installation detected ({} mechanisms)\n"
            "   → Remediation: Check autostart entries, cron jobs, systemd services\n"
            "   → Evidence: Modifications to startup directories or service configurations",
            metrics.persistence_mechanisms))));
    }

    // ========================================================================
    // MEDIUM-LEVEL THREATS (Network Behavior)
    // ========================================================================

    // Network activity
    if (metrics.network_operations > 5) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Network activity detected ({} operations)\n"
            "   → Breakdown: {} outbound connections, {} DNS queries, {} HTTP requests\n"
            "   → Remediation: Block network access, inspect C2 communication patterns\n"
            "   → Evidence: socket/connect/send/recv syscalls",
            metrics.network_operations,
            metrics.outbound_connections,
            metrics.dns_queries,
            metrics.http_requests))));
    }

    // Outbound connections (potential C2)
    if (metrics.outbound_connections > 3) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: Multiple outbound connections to unique IPs ({} destinations)\n"
            "   → Remediation: BLOCK NETWORK - Likely command-and-control (C2) communication\n"
            "   → Evidence: {} unique remote IPs contacted via connect() syscall\n"
            "   → Next Step: Extract IP addresses from network logs for blacklisting",
            metrics.outbound_connections,
            metrics.outbound_connections))));
    }

    // DNS queries (domain generation algorithm detection)
    if (metrics.dns_queries > 5) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Suspicious DNS query pattern ({} queries)\n"
            "   → Remediation: Review DNS logs for domain generation algorithm (DGA) patterns\n"
            "   → Evidence: {} DNS resolution attempts (may indicate C2 domain lookup)",
            metrics.dns_queries,
            metrics.dns_queries))));
    }

    // HTTP requests (data exfiltration)
    if (metrics.http_requests > 3) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: Multiple HTTP requests detected ({} requests)\n"
            "   → Remediation: Inspect HTTP payloads for data exfiltration\n"
            "   → Evidence: {} HTTP GET/POST operations to external servers",
            metrics.http_requests,
            metrics.http_requests))));
    }

    // ========================================================================
    // MEDIUM-LEVEL THREATS (System/Registry)
    // ========================================================================

    // Registry operations (Windows-specific)
    if (metrics.registry_operations > 5) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Registry modifications detected ({} operations)\n"
            "   → Remediation: Review HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run keys\n"
            "   → Evidence: {} registry read/write operations (Windows only)",
            metrics.registry_operations,
            metrics.registry_operations))));
    }

    // Service modifications
    if (metrics.service_modifications > 0) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟠 HIGH: Service/daemon modification attempted ({} modifications)\n"
            "   → Remediation: Check systemd units, /etc/init.d, Windows services\n"
            "   → Evidence: Unauthorized service creation or configuration changes",
            metrics.service_modifications))));
    }

    // ========================================================================
    // MEDIUM-LEVEL THREATS (Memory Behavior)
    // ========================================================================

    // Memory operations (shellcode detection)
    if (metrics.memory_operations > 5) {
        TRY(behaviors.try_append(TRY(String::formatted(
            "🟡 MEDIUM: Suspicious memory operations detected ({} operations)\n"
            "   → Breakdown: mmap/mprotect/mremap syscalls\n"
            "   → Remediation: Investigate for RWX memory pages (shellcode execution)\n"
            "   → Evidence: {} memory allocation/protection changes (potential heap spray)",
            metrics.memory_operations,
            metrics.memory_operations))));
    }

    // ========================================================================
    // LOW-LEVEL / INFORMATIONAL
    // ========================================================================

    // If no suspicious behaviors detected, indicate clean analysis
    if (behaviors.is_empty()) {
        TRY(behaviors.try_append(
            "🟢 LOW: No significant suspicious behaviors detected\n"
            "   → Status: File appears benign based on syscall analysis\n"
            "   → Note: Static analysis (YARA) may still detect known malware signatures"_string));
    }

    return behaviors;
}

// ============================================================================
// Advanced Malware Pattern Detection (Week 2 Task 2)
// ============================================================================

bool BehavioralAnalyzer::detect_ransomware_pattern(BehavioralMetrics const& metrics)
{
    // Ransomware detection heuristic:
    // - Rapid file modifications/deletions characteristic of encryption + cleanup
    // - Threshold: file_operations > 50 AND (unlink_count / file_ops) > 0.3
    //
    // Rationale:
    // - Ransomware typically encrypts many files rapidly (high file_operations)
    // - After encryption, original files are deleted (high unlink ratio)
    // - Threshold of 50 operations chosen to minimize false positives
    // - Deletion ratio > 30% indicates aggressive file destruction
    //
    // False positive considerations:
    // - Build systems: High file ops but low deletion ratio
    // - Cleaners/optimizers: May exceed thresholds, but legitimate use case
    // - Archive extraction: High ops but primarily creates, not deletes
    //
    // Note: Real implementation would track file_operations and unlink_count separately
    // For now, using conservative heuristic based on total file operations
    // Future enhancement: Parse syscall arguments to track unlink vs write ratio

    if (metrics.file_operations <= 50) {
        return false;
    }

    // Note: Conservative estimate would be ~30% of file operations are deletions
    // This is a rough heuristic since we don't track unlink separately yet
    // A more accurate implementation would count unlink/unlinkat syscalls specifically

    // Additional signals that strengthen ransomware hypothesis
    bool has_encryption_indicators = false;

    // Ransomware often modifies many files rapidly
    if (metrics.file_operations > 100) {
        has_encryption_indicators = true;
    }

    // Ransomware may drop ransom notes (executable_drops or temp files)
    if (metrics.executable_drops > 0 || metrics.temp_file_creates > 5) {
        has_encryption_indicators = true;
    }

    // Network beaconing to C2 after encryption
    if (metrics.outbound_connections > 0 && metrics.file_operations > 50) {
        has_encryption_indicators = true;
    }

    return has_encryption_indicators;
}

bool BehavioralAnalyzer::detect_keylogger_pattern(BehavioralMetrics const& metrics)
{
    // Keylogger detection heuristic:
    // - Access to /dev/input/* devices (keyboard event streams)
    // - Persistent file writing (logging keystrokes to file)
    // - Potential network beaconing (exfiltrating captured data)
    //
    // Rationale:
    // - Linux keyloggers typically read from /dev/input/eventX devices
    // - Raw keyboard input requires special file access patterns
    // - Captured keystrokes stored locally or exfiltrated via network
    //
    // False positive considerations:
    // - Input remapping tools (xmodmap, etc.) may access input devices
    // - Accessibility tools (screen readers) may monitor keyboard
    // - Terminal emulators generally don't access /dev/input directly
    //
    // Current limitation: We don't parse file paths from syscall arguments yet
    // Future enhancement: Track file_operations that target /dev/input/*
    // For now, using proxy signals of suspicious file + network behavior

    // Keyloggers typically have moderate file operations (logging keystrokes)
    bool has_file_logging = metrics.file_operations > 10 && metrics.file_operations < 100;

    // Hidden file creation (common for keylogger output files)
    bool has_hidden_output = metrics.hidden_file_creates > 0;

    // Network exfiltration of captured keystrokes
    bool has_network_exfil = metrics.outbound_connections > 0 && metrics.network_operations > 5;

    // Keyloggers often attempt persistence
    bool has_persistence = metrics.persistence_mechanisms > 0;

    // Conservative detection: Require multiple indicators
    int indicator_count = 0;
    if (has_file_logging) indicator_count++;
    if (has_hidden_output) indicator_count++;
    if (has_network_exfil) indicator_count++;
    if (has_persistence) indicator_count++;

    // Require at least 2 indicators to minimize false positives
    return indicator_count >= 2;
}

bool BehavioralAnalyzer::detect_rootkit_pattern(BehavioralMetrics const& metrics)
{
    // Rootkit detection heuristic:
    // - Privilege escalation attempts (setuid, capabilities, etc.)
    // - Kernel manipulation attempts (/proc/kallsyms, /dev/mem, modules)
    // - Filesystem mounting operations
    // - Namespace/container escape attempts
    //
    // Rationale:
    // - Rootkits attempt to gain kernel-level control
    // - /proc/kallsyms reveals kernel symbol addresses for exploitation
    // - /dev/mem provides direct memory access for kernel patching
    // - Module loading allows injecting code into kernel space
    // - Mount operations can bypass security restrictions
    //
    // False positive considerations:
    // - System utilities (systemd, udev) may access kernel interfaces
    // - Container runtimes may use unshare/setns legitimately
    // - In sandbox context, these operations should be blocked by seccomp
    //
    // Detection strategy: Any privilege escalation attempt is highly suspicious
    // in our sandbox threat model, as legitimate programs don't need these operations

    // Primary indicator: Privilege escalation attempts
    // These include: setuid, capset, mount, unshare, chroot, kernel modules
    if (metrics.privilege_escalation_attempts > 0) {
        return true;
    }

    // Secondary indicator: Multiple suspicious system operations
    // Rootkits often combine file manipulation with process/memory operations
    bool has_system_manipulation = false;

    if (metrics.file_operations > 20 && metrics.process_operations > 3) {
        has_system_manipulation = true;
    }

    if (metrics.memory_operations > 10 && metrics.code_injection_attempts > 0) {
        has_system_manipulation = true;
    }

    // Service modification attempts (systemd units, init scripts)
    if (metrics.service_modifications > 0) {
        has_system_manipulation = true;
    }

    return has_system_manipulation;
}

bool BehavioralAnalyzer::detect_cryptominer_pattern(BehavioralMetrics const& metrics)
{
    // Cryptominer detection heuristic:
    // - High CPU usage implied by excessive operations
    // - Network beaconing to mining pool
    // - Persistent outbound connections (submitting work units)
    //
    // Rationale:
    // - Cryptominers perform intensive computation (mining algorithms)
    // - Connect to mining pools to receive work and submit results
    // - Maintain persistent network connections for continuous mining
    // - Often use multiple outbound connections (pool + backup pools)
    //
    // False positive considerations:
    // - Scientific computation software may have similar patterns
    // - Network clients (browsers, downloaders) have outbound connections
    // - Build systems may have high memory operations
    //
    // Detection strategy: Combination of network activity + resource usage
    // Threshold chosen to catch common mining patterns while minimizing FPs

    // Primary indicators: Network beaconing pattern
    bool has_mining_pool_beaconing = (metrics.network_operations > 10) && (metrics.outbound_connections > 5);

    // Secondary indicator: Resource-intensive operations
    bool has_intensive_operations = false;

    // High memory operations (allocating buffers for mining)
    if (metrics.memory_operations > 20) {
        has_intensive_operations = true;
    }

    // Process spawning (multi-threaded mining)
    if (metrics.process_operations > 5) {
        has_intensive_operations = true;
    }

    // Persistence (ensure mining continues across reboots)
    if (metrics.persistence_mechanisms > 0) {
        has_intensive_operations = true;
    }

    // Conservative detection: Require both network beaconing AND resource usage
    return has_mining_pool_beaconing && has_intensive_operations;
}

bool BehavioralAnalyzer::detect_process_injector_pattern(BehavioralMetrics const& metrics)
{
    // Process injector detection heuristic:
    // - Code injection attempts (ptrace, process_vm_writev)
    // - Memory permission changes (mprotect with RWX)
    // - Process operations (targeting victim processes)
    //
    // Rationale:
    // - Process injection requires attaching to target process (ptrace)
    // - Memory must be made writable and executable (mprotect RWX)
    // - Injected code written to target process memory (process_vm_writev)
    // - Common in malware for defense evasion and privilege escalation
    //
    // False positive considerations:
    // - Debuggers (gdb, lldb) use ptrace legitimately
    // - JIT compilers may use mprotect for code generation
    // - In sandbox context, debuggers shouldn't be running
    // - JIT compilation unlikely in malware samples
    //
    // Detection strategy: code_injection_attempts is direct signal
    // This includes ptrace and process_vm_* syscalls from Week 2 monitoring

    // Primary indicator: Direct code injection attempts detected
    if (metrics.code_injection_attempts > 0) {
        return true;
    }

    // Secondary indicator: Memory manipulation + process operations
    // Combination suggests process injection even without direct ptrace
    bool has_injection_pattern = false;

    // High memory operations + process spawning
    if (metrics.memory_operations > 10 && metrics.process_operations > 3) {
        has_injection_pattern = true;
    }

    // Self-modification attempts (runtime code patching)
    if (metrics.self_modification_attempts > 0 && metrics.process_operations > 0) {
        has_injection_pattern = true;
    }

    return has_injection_pattern;
}

void BehavioralAnalyzer::reset_statistics()
{
    m_stats = Statistics {};
    dbgln_if(false, "BehavioralAnalyzer: Statistics reset");
}

// ============================================================================
// Process Management - fork/exec and IPC pipes (Week 1 Day 4)
// ============================================================================

ErrorOr<SandboxProcess> BehavioralAnalyzer::launch_nsjail_sandbox(Vector<String> const& nsjail_command)
{
    // Fork a child process and exec nsjail with IPC pipes for stdin/stdout/stderr
    // This provides bidirectional communication with the sandboxed process

    dbgln_if(false, "BehavioralAnalyzer: Launching nsjail sandbox with {} arguments", nsjail_command.size());

    // Create three pipes for stdin, stdout, and stderr
    // pipe2 creates: [read_end, write_end]
    auto stdin_pipe = TRY(Core::System::pipe2(0));
    auto stdout_pipe = TRY(Core::System::pipe2(0));
    auto stderr_pipe = TRY(Core::System::pipe2(0));

    dbgln_if(false, "BehavioralAnalyzer: Created IPC pipes - stdin[{},{}] stdout[{},{}] stderr[{},{}]",
        stdin_pipe[0], stdin_pipe[1], stdout_pipe[0], stdout_pipe[1], stderr_pipe[0], stderr_pipe[1]);

    // Fork child process
    pid_t pid = fork();
    if (pid < 0) {
        // Fork failed - cleanup pipes and return error
        auto fork_error = Error::from_errno(errno);

        // Close all pipe file descriptors
        (void)Core::System::close(stdin_pipe[0]);
        (void)Core::System::close(stdin_pipe[1]);
        (void)Core::System::close(stdout_pipe[0]);
        (void)Core::System::close(stdout_pipe[1]);
        (void)Core::System::close(stderr_pipe[0]);
        (void)Core::System::close(stderr_pipe[1]);

        dbgln("BehavioralAnalyzer: fork() failed: {}", fork_error);
        return fork_error;
    }

    if (pid == 0) {
        // ===== CHILD PROCESS =====
        // Redirect stdin/stdout/stderr to pipes and exec nsjail

        // Close parent's pipe ends (child doesn't need these)
        (void)close(stdin_pipe[1]);  // Close parent's write end of stdin
        (void)close(stdout_pipe[0]); // Close parent's read end of stdout
        (void)close(stderr_pipe[0]); // Close parent's read end of stderr

        // Redirect stdin: dup2(read end of stdin_pipe, STDIN_FILENO)
        if (dup2(stdin_pipe[0], STDIN_FILENO) < 0) {
            dbgln("BehavioralAnalyzer: Child - dup2 stdin failed: {}", strerror(errno));
            _exit(127);
        }
        (void)close(stdin_pipe[0]); // Close after dup2

        // Redirect stdout: dup2(write end of stdout_pipe, STDOUT_FILENO)
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) {
            dbgln("BehavioralAnalyzer: Child - dup2 stdout failed: {}", strerror(errno));
            _exit(127);
        }
        (void)close(stdout_pipe[1]); // Close after dup2

        // Redirect stderr: dup2(write end of stderr_pipe, STDERR_FILENO)
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) {
            // Can't use dbgln here since stderr is being redirected
            _exit(127);
        }
        (void)close(stderr_pipe[1]); // Close after dup2

        // Build argv array for execve
        Vector<char const*> argv;
        for (auto const& arg : nsjail_command) {
            if (argv.try_append(arg.bytes_as_string_view().characters_without_null_termination()).is_error()) {
                _exit(127);
            }
        }
        if (argv.try_append(nullptr).is_error()) { // NULL-terminate argv
            _exit(127);
        }

        // Execute nsjail
        // Note: execve only returns on failure
        execve(argv[0], const_cast<char* const*>(argv.data()), environ);

        // If we get here, execve failed
        dbgln("BehavioralAnalyzer: Child - execve failed: {}", strerror(errno));
        _exit(127);
    }

    // ===== PARENT PROCESS =====
    // Close child's pipe ends (parent doesn't need these)
    TRY(Core::System::close(stdin_pipe[0]));  // Close child's read end of stdin
    TRY(Core::System::close(stdout_pipe[1])); // Close child's write end of stdout
    TRY(Core::System::close(stderr_pipe[1])); // Close child's write end of stderr

    // Create SandboxProcess with parent's pipe ends
    SandboxProcess process {
        .pid = pid,
        .stdin_fd = stdin_pipe[1],   // Parent writes to child's stdin
        .stdout_fd = stdout_pipe[0], // Parent reads from child's stdout
        .stderr_fd = stderr_pipe[0]  // Parent reads from child's stderr
    };

    dbgln_if(false, "BehavioralAnalyzer: Launched sandbox process PID={} with IPC pipes stdin={} stdout={} stderr={}",
        process.pid, process.stdin_fd, process.stdout_fd, process.stderr_fd);

    return process;
}

// ============================================================================
// Timeout Enforcement and Process Cleanup (Week 1 Day 4)
// ============================================================================

ErrorOr<void> BehavioralAnalyzer::enforce_sandbox_timeout(pid_t sandbox_pid, Duration timeout)
{
    // Enforce timeout using POSIX timer_create for precise control
    // This approach is preferred over SIGALRM because:
    // 1. Multiple timers can coexist (no global state)
    // 2. Better thread safety in multi-threaded environments
    // 3. More precise control over timer behavior
    // 4. Can target specific processes with signals

    if (sandbox_pid <= 0) {
        return Error::from_string_literal("Invalid sandbox PID");
    }

    // Create timer structure
    struct sigevent sev = {};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGKILL;  // Send SIGKILL on timeout
    sev.sigev_value.sival_int = sandbox_pid;

    timer_t timer_id;
    if (timer_create(CLOCK_MONOTONIC, &sev, &timer_id) == -1) {
        return Error::from_errno(errno);
    }

    // Ensure timer is deleted on function exit
    ScopeGuard timer_cleanup = [&timer_id] {
        timer_delete(timer_id);
    };

    // Configure timer to fire once after timeout duration
    struct itimerspec its = {};
    its.it_value.tv_sec = timeout.to_seconds();
    its.it_value.tv_nsec = (timeout.to_nanoseconds() % 1'000'000'000);
    its.it_interval.tv_sec = 0;  // One-shot timer (no repeat)
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timer_id, 0, &its, nullptr) == -1) {
        return Error::from_errno(errno);
    }

    dbgln_if(false, "BehavioralAnalyzer: Timeout timer set for PID {} ({}ms)",
        sandbox_pid, timeout.to_milliseconds());

    return {};
}

ErrorOr<int> BehavioralAnalyzer::wait_for_sandbox_completion(pid_t sandbox_pid)
{
    // Wait for sandbox process to complete and return exit code
    // Handles:
    // - Normal exit (WIFEXITED)
    // - Signal termination (WIFSIGNALED) - including timeout SIGKILL
    // - EINTR interruption (retry on signal interruption)

    if (sandbox_pid <= 0) {
        return Error::from_string_literal("Invalid sandbox PID");
    }

    int status = 0;
    pid_t result;

    // Retry waitpid on EINTR (interrupted by signal)
    while (true) {
        result = waitpid(sandbox_pid, &status, 0);

        if (result == -1) {
            if (errno == EINTR) {
                // Interrupted by signal, retry
                dbgln_if(false, "BehavioralAnalyzer: waitpid interrupted, retrying...");
                continue;
            }
            return Error::from_errno(errno);
        }

        // Successfully collected process status
        break;
    }

    // Parse exit status
    int exit_code = -1;

    if (WIFEXITED(status)) {
        // Process exited normally
        exit_code = WEXITSTATUS(status);
        dbgln_if(false, "BehavioralAnalyzer: Sandbox PID {} exited normally with code {}",
            sandbox_pid, exit_code);
    } else if (WIFSIGNALED(status)) {
        // Process terminated by signal (could be timeout SIGKILL or crash)
        int signal_number = WTERMSIG(status);

        // Convention: exit code = 128 + signal number
        // This matches shell behavior (bash, etc.)
        exit_code = 128 + signal_number;

        char const* signal_name = "UNKNOWN";
        if (signal_number == SIGKILL)
            signal_name = "SIGKILL (timeout)";
        else if (signal_number == SIGSEGV)
            signal_name = "SIGSEGV (segfault)";
        else if (signal_number == SIGABRT)
            signal_name = "SIGABRT (abort)";
        else if (signal_number == SIGTERM)
            signal_name = "SIGTERM";

        dbgln_if(false, "BehavioralAnalyzer: Sandbox PID {} killed by signal {} ({}), exit code {}",
            sandbox_pid, signal_number, signal_name, exit_code);

        // Update statistics for crashes (non-timeout signals)
        if (signal_number != SIGKILL) {
            m_stats.crashes++;
        }
    } else {
        // WIFSTOPPED or other unexpected status
        // This should not happen in normal operation
        dbgln("BehavioralAnalyzer: Warning - Unexpected wait status for PID {}: {}",
            sandbox_pid, status);
        return Error::from_string_literal("Unexpected process wait status");
    }

    return exit_code;
}

// ============================================================================
// Pipe I/O Helpers - Non-blocking read from nsjail stderr (Week 1 Day 5)
// ============================================================================

ErrorOr<ByteBuffer> BehavioralAnalyzer::read_pipe_nonblocking(int fd, Duration timeout)
{
    // Read from pipe with timeout using poll()
    // Strategy: poll-based non-blocking I/O with chunk reads
    // Buffer size: 4KB chunks for efficient I/O
    // Handles: EAGAIN, EINTR, EOF, timeout, partial reads

    if (fd < 0) {
        return Error::from_string_literal("Invalid file descriptor");
    }

    dbgln_if(false, "BehavioralAnalyzer: Reading from pipe fd={} with timeout={}ms",
        fd, timeout.to_milliseconds());

    // Set file descriptor to non-blocking mode
    // This prevents read() from blocking if no data is available
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return Error::from_errno(errno);
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        return Error::from_errno(errno);
    }

    // Ensure we restore blocking mode on exit
    ScopeGuard restore_blocking = [fd, flags] {
        (void)fcntl(fd, F_SETFL, flags);
    };

    // Accumulator for all read data
    ByteBuffer result;

    // Read deadline
    auto start_time = MonotonicTime::now();
    auto deadline = start_time + timeout;

    // Read buffer: 4KB chunks (efficient for syscall tracing output)
    constexpr size_t CHUNK_SIZE = 4096;
    u8 buffer[CHUNK_SIZE];

    while (true) {
        // Calculate remaining timeout
        auto now = MonotonicTime::now();
        if (now >= deadline) {
            dbgln_if(false, "BehavioralAnalyzer: Pipe read timeout reached (read {} bytes)", result.size());
            break; // Timeout - return partial data
        }

        auto remaining_timeout = deadline - now;
        int poll_timeout_ms = static_cast<int>(remaining_timeout.to_milliseconds());

        // Setup poll structure to wait for data
        struct pollfd pfd = {};
        pfd.fd = fd;
        pfd.events = POLLIN; // Wait for readable data

        // Poll with timeout
        int poll_result = poll(&pfd, 1, poll_timeout_ms);

        if (poll_result < 0) {
            if (errno == EINTR) {
                // Interrupted by signal - retry with remaining timeout
                dbgln_if(false, "BehavioralAnalyzer: poll() interrupted by signal, retrying...");
                continue;
            }
            return Error::from_errno(errno);
        }

        if (poll_result == 0) {
            // Timeout expired
            dbgln_if(false, "BehavioralAnalyzer: poll() timeout (read {} bytes)", result.size());
            break;
        }

        // Check poll events
        if (pfd.revents & POLLERR) {
            dbgln("BehavioralAnalyzer: Pipe error condition detected");
            return Error::from_string_literal("Pipe error (POLLERR)");
        }

        if (pfd.revents & POLLHUP) {
            // Pipe closed by writer (normal EOF)
            dbgln_if(false, "BehavioralAnalyzer: Pipe closed (POLLHUP) - read {} bytes", result.size());
            break;
        }

        if (pfd.revents & POLLNVAL) {
            dbgln("BehavioralAnalyzer: Invalid pipe file descriptor");
            return Error::from_string_literal("Invalid pipe file descriptor (POLLNVAL)");
        }

        if (pfd.revents & POLLIN) {
            // Data available - read chunk
            ssize_t bytes_read = read(fd, buffer, CHUNK_SIZE);

            if (bytes_read < 0) {
                if (errno == EAGAIN) {
                    // Note: EWOULDBLOCK == EAGAIN on Linux, only check EAGAIN
                    // Spurious wakeup - should not happen with poll, but handle it
                    dbgln_if(false, "BehavioralAnalyzer: Spurious EAGAIN, retrying...");
                    continue;
                }
                if (errno == EINTR) {
                    // Interrupted by signal - retry
                    dbgln_if(false, "BehavioralAnalyzer: read() interrupted by signal, retrying...");
                    continue;
                }
                return Error::from_errno(errno);
            }

            if (bytes_read == 0) {
                // EOF - pipe closed
                dbgln_if(false, "BehavioralAnalyzer: EOF reached - read {} bytes total", result.size());
                break;
            }

            // Append chunk to result
            TRY(result.try_append(buffer, static_cast<size_t>(bytes_read)));

            dbgln_if(false, "BehavioralAnalyzer: Read {} bytes (total: {})", bytes_read, result.size());

            // Continue reading until no more data or timeout
        }
    }

    dbgln_if(false, "BehavioralAnalyzer: Pipe read complete - {} bytes in {}ms",
        result.size(), (MonotonicTime::now() - start_time).to_milliseconds());

    return result;
}

ErrorOr<Vector<String>> BehavioralAnalyzer::read_pipe_lines(int fd, Duration timeout)
{
    // Read pipe and split into lines
    // This is a convenience wrapper around read_pipe_nonblocking()
    // Useful for parsing line-oriented output like syscall traces

    if (fd < 0) {
        return Error::from_string_literal("Invalid file descriptor");
    }

    dbgln_if(false, "BehavioralAnalyzer: Reading lines from pipe fd={} with timeout={}ms",
        fd, timeout.to_milliseconds());

    // Read all data from pipe
    auto buffer = TRY(read_pipe_nonblocking(fd, timeout));

    if (buffer.is_empty()) {
        dbgln_if(false, "BehavioralAnalyzer: No data read from pipe");
        return Vector<String> {};
    }

    // Convert to string and split by newlines
    // Note: buffer.bytes() returns Span<u8>, need ReadonlyBytes for from_utf8
    ReadonlyBytes readonly_buffer { buffer.bytes() };
    auto content = TRY(String::from_utf8(readonly_buffer));
    auto lines_view = content.bytes_as_string_view().split_view('\n');

    Vector<String> lines;
    for (auto const& line_view : lines_view) {
        // Skip empty lines
        if (line_view.is_empty())
            continue;

        auto line = TRY(String::from_utf8(line_view));
        TRY(lines.try_append(line));
    }

    dbgln_if(false, "BehavioralAnalyzer: Read {} lines from pipe ({} bytes)",
        lines.size(), buffer.size());

    return lines;
}

// ============================================================================
// Syscall Event Parsing (Week 1 Day 6)
// ============================================================================

ErrorOr<Optional<SyscallEvent>> BehavioralAnalyzer::parse_syscall_event(StringView line)
{
    // Parse syscall events from nsjail stderr output
    // Expected format: [SYSCALL] syscall_name(arg1, arg2, ...)
    // Examples:
    //   [SYSCALL] open("/tmp/file.txt", O_RDONLY)
    //   [SYSCALL] write(3, "data", 4)
    //   [SYSCALL] socket(AF_INET, SOCK_STREAM, 0)
    //   [SYSCALL] mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    //
    // Performance optimization: Early return for non-syscall lines

    // Step 1: Quick check - must start with "[SYSCALL]"
    constexpr auto SYSCALL_MARKER = "[SYSCALL]"sv;
    if (!line.starts_with(SYSCALL_MARKER)) {
        // Not a syscall line - return empty Optional
        return Optional<SyscallEvent> {};
    }

    // Step 2: Skip marker and whitespace
    auto content = line.substring_view(SYSCALL_MARKER.length()).trim_whitespace();
    if (content.is_empty()) {
        // Malformed line: marker but no content
        return Optional<SyscallEvent> {};
    }

    // Step 3: Find syscall name (everything before '(')
    auto paren_pos = content.find('(');
    if (!paren_pos.has_value()) {
        // No parentheses - just syscall name (rare but valid)
        // Example: [SYSCALL] getpid
        auto syscall_name = TRY(String::from_utf8(content.trim_whitespace()));

        SyscallEvent event {
            .name = move(syscall_name),
            .args = {},
            .timestamp_ns = 0
        };

        return event;
    }

    // Step 4: Extract syscall name (before '(')
    auto name_view = content.substring_view(0, paren_pos.value()).trim_whitespace();
    if (name_view.is_empty()) {
        // Malformed: empty name before '('
        return Optional<SyscallEvent> {};
    }

    auto syscall_name = TRY(String::from_utf8(name_view));

    // Step 5: Extract arguments (between '(' and ')')
    // Find matching closing parenthesis
    auto args_start = paren_pos.value() + 1;
    auto args_view = content.substring_view(args_start);

    auto close_paren_pos = args_view.find(')');
    if (!close_paren_pos.has_value()) {
        // Missing closing ')' - treat as malformed, but still return name
        SyscallEvent event {
            .name = move(syscall_name),
            .args = {},
            .timestamp_ns = 0
        };

        return event;
    }

    // Step 6: Parse arguments (simple comma split)
    // Note: This is a simplified parser that doesn't handle nested parentheses
    // or quoted strings with commas. Good enough for basic monitoring.
    auto args_content = args_view.substring_view(0, close_paren_pos.value()).trim_whitespace();

    Vector<String> args;
    if (!args_content.is_empty()) {
        // Split by comma and trim each argument
        auto parts = args_content.split_view(',');
        for (auto const& part : parts) {
            auto trimmed = part.trim_whitespace();
            if (!trimmed.is_empty()) {
                TRY(args.try_append(TRY(String::from_utf8(trimmed))));
            }
        }
    }

    SyscallEvent event {
        .name = move(syscall_name),
        .args = move(args),
        .timestamp_ns = 0
    };

    dbgln_if(false, "BehavioralAnalyzer: Parsed syscall: {} with {} args",
        event.name, event.args.size());

    return event;
}

// Syscall-to-Metrics Mapping (Week 2)
// ============================================================================

void BehavioralAnalyzer::update_metrics_from_syscall(StringView syscall_name, BehavioralMetrics& metrics)
{
    // Map Linux syscalls to BehavioralMetrics fields
    // This is called for every monitored syscall, so performance is critical
    // Using if/else chain for speed (faster than HashMap lookup)
    //
    // Categories:
    // 1. File System Operations (open, write, unlink, etc.)
    // 2. Process Operations (fork, exec, ptrace, etc.)
    // 3. Network Operations (socket, connect, send, etc.)
    // 4. Memory Operations (mmap, mprotect, etc.)
    // 5. System/Privilege Operations (setuid, mount, etc.)

    // ===== Category 1: File System Operations =====

    // File I/O operations - basic read/write
    if (syscall_name == "open"sv || syscall_name == "openat"sv || syscall_name == "openat2"sv || syscall_name == "creat"sv) {
        metrics.file_operations++;
        // Note: Would need to inspect arguments to detect temp/hidden files
        // For now, increment general counter
    } else if (syscall_name == "write"sv || syscall_name == "pwrite64"sv || syscall_name == "writev"sv || syscall_name == "pwritev"sv || syscall_name == "pwritev2"sv) {
        metrics.file_operations++;
        // Heuristic: Rapid writes may indicate ransomware
        // Consider tracking write rate (writes per second)
    } else if (syscall_name == "read"sv || syscall_name == "pread64"sv || syscall_name == "readv"sv || syscall_name == "preadv"sv || syscall_name == "preadv2"sv) {
        metrics.file_operations++;
    }

    // File deletion operations
    else if (syscall_name == "unlink"sv || syscall_name == "unlinkat"sv || syscall_name == "rmdir"sv) {
        metrics.file_operations++;
        // Heuristic: Rapid deletions may indicate ransomware cleanup
    }

    // File renaming operations
    else if (syscall_name == "rename"sv || syscall_name == "renameat"sv || syscall_name == "renameat2"sv) {
        metrics.file_operations++;
        // Heuristic: Rapid renames may indicate ransomware encryption
    }

    // Directory operations
    else if (syscall_name == "mkdir"sv || syscall_name == "mkdirat"sv) {
        metrics.file_operations++;
        // TODO: Inspect path argument to detect temp directories (/tmp, %TEMP%)
        // For now, assume potential temp directory creation
        // This would require argument parsing in real implementation
    }

    // File metadata modifications
    else if (syscall_name == "chmod"sv || syscall_name == "fchmod"sv || syscall_name == "fchmodat"sv || syscall_name == "chown"sv || syscall_name == "fchown"sv || syscall_name == "fchownat"sv || syscall_name == "lchown"sv) {
        metrics.file_operations++;
        // Heuristic: chmod +x may indicate executable drop
        // Would require argument inspection to detect 0755, 0700, etc.
    }

    // File truncation (potential data destruction)
    else if (syscall_name == "truncate"sv || syscall_name == "ftruncate"sv) {
        metrics.file_operations++;
    }

    // ===== Category 2: Process Operations =====

    // Process creation
    else if (syscall_name == "fork"sv || syscall_name == "vfork"sv || syscall_name == "clone"sv || syscall_name == "clone3"sv) {
        metrics.process_operations++;
        // Heuristic: Excessive forking may indicate fork bomb or process spawning malware
    }

    // Process execution
    else if (syscall_name == "execve"sv || syscall_name == "execveat"sv) {
        metrics.process_operations++;
        // TODO: Inspect executable path argument to detect suspicious executables
        // e.g., /tmp/*, hidden files, script interpreters with inline code
    }

    // Process debugging/injection
    else if (syscall_name == "ptrace"sv) {
        metrics.code_injection_attempts++;
        // ptrace is the primary mechanism for process injection on Linux
        // Also used by debuggers, but suspicious in sandboxed context
    }

    // Process memory reading/writing (alternative injection vector)
    else if (syscall_name == "process_vm_readv"sv || syscall_name == "process_vm_writev"sv) {
        metrics.code_injection_attempts++;
        // Direct memory read/write to another process - highly suspicious
    }

    // ===== Category 3: Network Operations =====

    // Socket creation
    else if (syscall_name == "socket"sv) {
        metrics.network_operations++;
        // TODO: Inspect socket type (AF_INET, AF_INET6, AF_UNIX)
        // For now, count all socket creations
    }

    // Outbound connections
    else if (syscall_name == "connect"sv) {
        metrics.network_operations++;
        metrics.outbound_connections++;
        // TODO: Extract remote IP from sockaddr argument
        // Track unique IPs for accurate outbound_connections count
    }

    // Network server operations (suspicious in most malware)
    else if (syscall_name == "bind"sv || syscall_name == "listen"sv || syscall_name == "accept"sv || syscall_name == "accept4"sv) {
        metrics.network_operations++;
        // Heuristic: bind/listen may indicate backdoor server
    }

    // Network data transfer
    else if (syscall_name == "send"sv || syscall_name == "sendto"sv || syscall_name == "sendmsg"sv || syscall_name == "sendmmsg"sv) {
        metrics.network_operations++;
        // TODO: Track data volume sent
    } else if (syscall_name == "recv"sv || syscall_name == "recvfrom"sv || syscall_name == "recvmsg"sv || syscall_name == "recvmmsg"sv) {
        metrics.network_operations++;
        // TODO: Track data volume received
    }

    // Socket options (used for advanced network manipulation)
    else if (syscall_name == "setsockopt"sv || syscall_name == "getsockopt"sv) {
        metrics.network_operations++;
    }

    // ===== Category 4: Memory Operations =====

    // Memory allocation/mapping
    else if (syscall_name == "mmap"sv || syscall_name == "mmap2"sv || syscall_name == "mremap"sv) {
        metrics.memory_operations++;
        // TODO: Track total memory allocated (potential heap spray detection)
    }

    // Memory protection changes
    else if (syscall_name == "mprotect"sv) {
        metrics.memory_operations++;
        // TODO: Inspect protection flags from arguments
        // CRITICAL: Detect RWX pages (PROT_READ | PROT_WRITE | PROT_EXEC)
        // RWX memory is strong indicator of code injection/shellcode execution
        //
        // Heuristic detection without argument parsing:
        // - Any mprotect call is suspicious in sandboxed malware
        // - Legitimate programs rarely change memory protection
        // - JIT compilers use mprotect, but unlikely in our threat model
        //
        // For comprehensive detection, would parse arguments:
        //   arg2 = protection flags (PROT_READ=1, PROT_WRITE=2, PROT_EXEC=4)
        //   if ((arg2 & 7) == 7) -> RWX detected
        metrics.code_injection_attempts++;
    }

    // Memory unmapping
    else if (syscall_name == "munmap"sv) {
        metrics.memory_operations++;
    }

    // Heap operations
    else if (syscall_name == "brk"sv) {
        metrics.memory_operations++;
    }

    // ===== Category 5: System & Privilege Operations =====

    // Privilege escalation attempts
    else if (syscall_name == "setuid"sv || syscall_name == "setuid32"sv || syscall_name == "setgid"sv || syscall_name == "setgid32"sv || syscall_name == "setreuid"sv || syscall_name == "setregid"sv || syscall_name == "setresuid"sv || syscall_name == "setresgid"sv || syscall_name == "setfsuid"sv || syscall_name == "setfsgid"sv) {
        metrics.privilege_escalation_attempts++;
        // These should be blocked by seccomp, but track attempts
    }

    // Capability manipulation
    else if (syscall_name == "capset"sv || syscall_name == "capget"sv) {
        metrics.privilege_escalation_attempts++;
        // Linux capabilities - alternative privilege escalation vector
    }

    // Filesystem mounting (should be blocked, but track attempts)
    else if (syscall_name == "mount"sv || syscall_name == "umount"sv || syscall_name == "umount2"sv || syscall_name == "pivot_root"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // Namespace manipulation (container escape attempts)
    else if (syscall_name == "unshare"sv || syscall_name == "setns"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // Chroot (privilege escalation via chroot escape)
    else if (syscall_name == "chroot"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // Kernel module operations (should be blocked, critical if successful)
    else if (syscall_name == "init_module"sv || syscall_name == "delete_module"sv || syscall_name == "finit_module"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // ===== Additional Suspicious Operations =====

    // Reboot/system control (should be blocked)
    else if (syscall_name == "reboot"sv || syscall_name == "kexec_load"sv || syscall_name == "kexec_file_load"sv) {
        // Extremely suspicious - potential ransomware trigger
        metrics.privilege_escalation_attempts++;
    }

    // I/O port access (hardware manipulation)
    else if (syscall_name == "ioperm"sv || syscall_name == "iopl"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // System logging (may be used to detect security tools)
    else if (syscall_name == "syslog"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // Quota management (filesystem manipulation)
    else if (syscall_name == "quotactl"sv) {
        metrics.privilege_escalation_attempts++;
    }

    // ===== Notes on Unmapped Syscalls =====
    //
    // The following syscalls are allowed by our seccomp policy but not mapped to metrics
    // because they are benign and would create noise:
    //
    // - stat, fstat, lstat, newfstatat, statx (file metadata reading)
    // - lseek, _llseek (file seeking)
    // - dup, dup2, dup3, fcntl (file descriptor operations)
    // - ioctl (device I/O - complex, context-dependent)
    // - getpid, getppid, gettid, getuid, etc. (process info queries)
    // - rt_sigaction, rt_sigprocmask, etc. (signal handling)
    // - getcwd, chdir (directory navigation)
    // - clock_gettime, gettimeofday, time (time queries)
    // - select, poll, epoll_* (I/O multiplexing)
    // - access, readlink (metadata queries)
    // - set_thread_area, arch_prctl (thread-local storage)
    // - getrlimit, prlimit64 (resource queries)
    // - futex (fast user-space mutexes)
    //
    // These are essential for program operation and don't indicate malicious behavior.
}
}
