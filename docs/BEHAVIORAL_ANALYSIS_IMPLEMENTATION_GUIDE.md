# Behavioral Analysis Implementation Guide

**C++ API design for behavioral malware detection in Ladybird Sentinel**

---

## Overview

This guide provides implementation-ready C++ interfaces for the behavioral analysis system described in `BEHAVIORAL_ANALYSIS_SPEC.md`. Follows Ladybird coding style (CamelCase classes, snake_case functions, `m_` prefix for members).

---

## Component Architecture

```
Sandbox
  ↓
  ├─ FileIOMonitor (Services/Sentinel/FileIOMonitor.{h,cpp})
  ├─ ProcessMonitor (Services/Sentinel/ProcessMonitor.{h,cpp})
  ├─ MemoryMonitor (Services/Sentinel/MemoryMonitor.{h,cpp})
  ├─ RegistryMonitor (Windows only, Services/Sentinel/RegistryMonitor.{h,cpp})
  └─ PlatformMonitor (Linux/macOS, Services/Sentinel/PlatformMonitor.{h,cpp})
        ↓
  BehavioralAnalyzer (Services/Sentinel/BehavioralAnalyzer.{h,cpp})
        ↓
  Verdict + Scoring
```

---

## Core Interface: BehavioralAnalyzer

### Header: Services/Sentinel/BehavioralAnalyzer.h

```cpp
/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/OwnPtr.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

// Behavioral analysis metrics for malware detection in sandbox
// Milestone 0.5 Phase 1: Real-time Sandboxing
class BehavioralAnalyzer {
public:
    // ============================================================================
    // VERDICT AND SCORING
    // ============================================================================

    struct MetricScore {
        float file_io_score { 0.0f };           // File I/O pattern score (0.0-1.0)
        float process_score { 0.0f };           // Process behavior score (0.0-1.0)
        float memory_score { 0.0f };            // Memory manipulation score (0.0-1.0)
        float registry_score { 0.0f };          // Registry operations score (0.0-1.0) [Windows]
        float platform_score { 0.0f };          // Platform-specific score (0.0-1.0)

        String explanation;                     // Human-readable breakdown
    };

    enum class Verdict {
        BENIGN,         // Score < 0.30
        SUSPICIOUS,     // Score 0.30-0.60
        MALICIOUS,      // Score >= 0.60
    };

    struct BehavioralVerdict {
        Verdict verdict { Verdict::BENIGN };
        float behavioral_score { 0.0f };        // 0.0-1.0
        float confidence { 0.0f };              // Statistical confidence 0.0-1.0
        MetricScore metrics;
        String explanation;                     // Why this verdict
        u64 execution_time_ms { 0 };            // How long analysis took
    };

    // ============================================================================
    // LIFECYCLE
    // ============================================================================

    // Create behavioral analyzer
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create();
    ~BehavioralAnalyzer();

    // ============================================================================
    // ANALYSIS INTERFACE
    // ============================================================================

    // Analyze file in sandbox and return behavioral verdict
    // file_data: Complete file to analyze
    // Returns: Verdict with detailed metrics
    ErrorOr<BehavioralVerdict> analyze_file(ByteBuffer const& file_data);

    // Analyze with timeout (for resource limits)
    // timeout_ms: Maximum execution time (typically 4000ms)
    ErrorOr<BehavioralVerdict> analyze_file_with_timeout(
        ByteBuffer const& file_data,
        u64 timeout_ms
    );

    // ============================================================================
    // INDIVIDUAL METRIC ANALYSIS (for testing/debugging)
    // ============================================================================

    // File I/O Patterns
    float analyze_file_modification_rate(Vector<FileModificationEvent> const& events);
    float analyze_entropy_based_writes(Vector<FileWriteEvent> const& events);
    float analyze_sensitive_directory_access(Vector<FileAccessEvent> const& events);
    float analyze_archive_extraction(Vector<ArchiveEvent> const& events);
    float analyze_suspicious_extensions(Vector<FileCreationEvent> const& events);

    // Process Behavior
    float analyze_process_chain_depth(ProcessTree const& tree);
    float analyze_process_injection(Vector<InjectionEvent> const& events);
    float analyze_malicious_subprocess_commands(Vector<SubprocessEvent> const& events);

    // Memory Manipulation
    float analyze_heap_spray(Vector<MemoryAllocationEvent> const& events);
    float analyze_executable_memory_writes(Vector<MemoryWriteEvent> const& events);

    // Registry Operations (Windows)
    float analyze_registry_persistence(Vector<RegistryWriteEvent> const& events);
    float analyze_credential_registry_access(Vector<RegistryReadEvent> const& events);
    float analyze_security_bypass(Vector<RegistryWriteEvent> const& events);

    // Platform-specific (Linux)
    float analyze_privilege_escalation(Vector<PrivilegeEvent> const& events);
    float analyze_persistence_installation(Vector<PersistenceEvent> const& events);

    // Platform-specific (macOS)
    float analyze_code_signing_bypass(Vector<CodeSigningEvent> const& events);
    float analyze_launchd_persistence(Vector<LaunchdEvent> const& events);

    // ============================================================================
    // EVENT STRUCTURES FOR METRICS
    // ============================================================================

    struct FileModificationEvent {
        String file_path;
        u64 bytes_written { 0 };
        UnixDateTime timestamp;
        float entropy { 0.0f };                 // Shannon entropy (0.0-8.0)
    };

    struct FileWriteEvent {
        String file_path;
        ByteBuffer written_data;
        UnixDateTime timestamp;
    };

    struct FileAccessEvent {
        String file_path;
        String access_type;                     // "read", "write", "execute"
        UnixDateTime timestamp;
    };

    struct ArchiveEvent {
        String archive_path;
        String extracted_path;
        u32 nesting_level { 0 };
        UnixDateTime timestamp;
    };

    struct FileCreationEvent {
        String file_path;
        String extension;
        u64 file_size { 0 };
        UnixDateTime timestamp;
    };

    struct ProcessTree {
        struct ProcessNode {
            u32 pid { 0 };
            String executable_path;
            String command_line;
            OwnPtr<ProcessNode> parent;
            Vector<OwnPtr<ProcessNode>> children;
            u32 depth { 0 };
        };
        OwnPtr<ProcessNode> root;
    };

    struct InjectionEvent {
        u32 source_pid { 0 };
        u32 target_pid { 0 };
        String target_process;
        String injection_technique;             // "WriteProcessMemory", "APC", etc.
        UnixDateTime timestamp;
    };

    struct SubprocessEvent {
        u32 parent_pid { 0 };
        String parent_process;
        String spawned_executable;
        String command_line;
        UnixDateTime timestamp;
    };

    struct MemoryAllocationEvent {
        u64 address { 0 };
        u64 size { 0 };
        String protection;                      // "RWX", "RX", "RW", etc.
        UnixDateTime timestamp;
    };

    struct MemoryWriteEvent {
        u64 address { 0 };
        ByteBuffer data_written;
        bool is_to_executable_region { false };
        UnixDateTime timestamp;
    };

    struct RegistryWriteEvent {
        String registry_path;
        String value_name;
        String value_data;
        UnixDateTime timestamp;
    };

    struct RegistryReadEvent {
        String registry_path;
        UnixDateTime timestamp;
    };

    struct PrivilegeEvent {
        String event_type;                      // "setuid", "sudo", "insmod"
        String target_object;
        UnixDateTime timestamp;
    };

    struct PersistenceEvent {
        String persistence_type;                // "cron", "systemd", "bashrc"
        String target_path;
        UnixDateTime timestamp;
    };

    struct CodeSigningEvent {
        String event_type;                      // "remove_quarantine", "strip_signature"
        String binary_path;
        UnixDateTime timestamp;
    };

    struct LaunchdEvent {
        String plist_path;
        String event_type;                      // "create", "modify"
        bool run_at_load { false };
        UnixDateTime timestamp;
    };

    // ============================================================================
    // STATISTICS
    // ============================================================================

    struct Statistics {
        u64 total_analyses { 0 };
        u64 benign_count { 0 };
        u64 suspicious_count { 0 };
        u64 malicious_count { 0 };
        float average_score { 0.0f };
        float average_execution_time_ms { 0.0f };
        float false_positive_rate { 0.0f };     // For testing
    };

    Statistics get_statistics() const { return m_stats; }
    void reset_statistics();

private:
    BehavioralAnalyzer();

    // ============================================================================
    // INTERNAL IMPLEMENTATION
    // ============================================================================

    // Sandbox execution and event collection
    ErrorOr<Vector<FileModificationEvent>> execute_and_collect_file_events(
        ByteBuffer const& file_data
    );
    ErrorOr<Vector<SubprocessEvent>> execute_and_collect_process_events(
        ByteBuffer const& file_data
    );
    ErrorOr<Vector<MemoryAllocationEvent>> execute_and_collect_memory_events(
        ByteBuffer const& file_data
    );
    ErrorOr<Vector<RegistryWriteEvent>> execute_and_collect_registry_events(
        ByteBuffer const& file_data
    );

    // Category scoring
    float calculate_file_io_score(
        Vector<FileModificationEvent> const& file_events,
        Vector<ArchiveEvent> const& archive_events,
        Vector<FileCreationEvent> const& creation_events
    );
    float calculate_process_score(
        ProcessTree const& tree,
        Vector<InjectionEvent> const& injection_events,
        Vector<SubprocessEvent> const& subprocess_events
    );
    float calculate_memory_score(
        Vector<MemoryAllocationEvent> const& alloc_events,
        Vector<MemoryWriteEvent> const& write_events
    );
    float calculate_registry_score(
        Vector<RegistryWriteEvent> const& reg_writes,
        Vector<RegistryReadEvent> const& reg_reads
    );
    float calculate_platform_score(
        Vector<PrivilegeEvent> const& priv_events,
        Vector<PersistenceEvent> const& persist_events
    );

    // Utility functions
    static float calculate_shannon_entropy(ByteBuffer const& data);
    static bool is_sensitive_directory(StringView path);
    static bool is_system_process(StringView process_name);
    static String extract_file_extension(StringView path);
    static bool is_suspicious_extension(StringView extension);

    // Member variables
    Statistics m_stats;
    bool m_initialized { false };

    // Platform-specific handles (opaque pointers)
#if defined(OS_WINDOWS)
    void* m_sandbox_handle { nullptr };
#elif defined(OS_LINUX)
    void* m_seccomp_context { nullptr };
#elif defined(OS_MACOS)
    void* m_fsevents_stream { nullptr };
#endif
};

}  // namespace Sentinel
```

---

## File I/O Monitor: FileIOMonitor.h

```cpp
#pragma once

#include <AK/Error.h>
#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel {

class FileIOMonitor {
public:
    struct FileEvent {
        enum class Type {
            CREATED,
            MODIFIED,
            DELETED,
            ACCESSED,
        };

        String file_path;
        Type event_type { Type::CREATED };
        u64 bytes_written { 0 };
        float entropy { 0.0f };
        u64 timestamp_ms { 0 };
    };

    static ErrorOr<NonnullOwnPtr<FileIOMonitor>> create();
    ~FileIOMonitor();

    // Start/stop monitoring
    ErrorOr<void> start_monitoring();
    ErrorOr<void> stop_monitoring();

    // Collect events
    Vector<FileEvent> get_events() const { return m_events; }
    void clear_events() { m_events.clear(); }

    // Analysis shortcuts
    u32 count_rapid_modifications(u64 time_window_ms) const;
    u32 count_high_entropy_files(float min_entropy = 7.0f) const;
    u32 count_sensitive_directory_access() const;

private:
    FileIOMonitor() = default;
    Vector<FileEvent> m_events;
    bool m_monitoring { false };
};

}  // namespace Sentinel
```

---

## Integration Point: SecurityTap Update

```cpp
// In Services/RequestServer/SecurityTap.h

// Add to SecurityTap class:
ErrorOr<SecurityVerdict> inspect_download_with_behavioral(
    ByteBuffer const& file_data,
    float yara_score,
    float ml_probability
) {
    // [1] YARA signature already checked (yara_score)
    if (yara_score >= 1.0f) {
        return SecurityVerdict::MALICIOUS;
    }

    // [2] ML prediction checked (ml_probability)
    if (ml_probability >= 0.8f) {
        return SecurityVerdict::MALICIOUS;
    }

    // [3] If ML score 0.3-0.6, run behavioral analysis
    if (ml_probability >= 0.3f && ml_probability < 0.6f) {
        auto analyzer = TRY(BehavioralAnalyzer::create());
        auto behavioral = TRY(analyzer->analyze_file(file_data));

        // Combine scores
        float final_score = (
            yara_score * 0.40f +
            ml_probability * 0.35f +
            behavioral.behavioral_score * 0.25f
        );

        if (final_score >= 0.60f) {
            return SecurityVerdict::MALICIOUS;
        } else if (final_score >= 0.30f) {
            return SecurityVerdict::SUSPICIOUS;
        }
    }

    return SecurityVerdict::BENIGN;
}
```

---

## Testing Framework

```cpp
// Services/Sentinel/TestBehavioralAnalyzer.cpp

#include <LibCore/System.h>
#include <LibTest/TestCase.h>
#include "BehavioralAnalyzer.h"

using namespace Sentinel;

TEST_CASE(ransomware_detection)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    // Simulate ransomware: 150 rapid file modifications
    Vector<BehavioralAnalyzer::FileModificationEvent> events;
    for (int i = 0; i < 150; i++) {
        BehavioralAnalyzer::FileModificationEvent event {
            .file_path = TRY(String::formatted("/tmp/file_{}.docx", i)),
            .bytes_written = 1024 * 1024,
            .entropy = 7.8f,  // High entropy (encrypted)
            .timestamp = UnixDateTime::now(),
        };
        events.append(event);
    }

    // Should trigger ransomware detection
    auto score = analyzer->analyze_file_modification_rate(events);
    EXPECT_GE(score, 0.7f);
}

TEST_CASE(legitimate_installer_no_fp)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    // Simulate legitimate installer: Registry Run key write
    Vector<BehavioralAnalyzer::RegistryWriteEvent> reg_events;
    BehavioralAnalyzer::RegistryWriteEvent event {
        .registry_path = "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        .value_name = "MyApp",
        .value_data = "C:\\Program Files\\MyApp\\app.exe",
    };
    reg_events.append(event);

    // Installer pattern - should score low
    auto score = analyzer->analyze_registry_persistence(reg_events);
    EXPECT_LT(score, 0.5f);  // Not highly suspicious (expected for installer)
}

TEST_CASE(process_injection_detection)
{
    auto analyzer = BehavioralAnalyzer::create().release_value();

    // Simulate injection into system process
    Vector<BehavioralAnalyzer::InjectionEvent> events;
    BehavioralAnalyzer::InjectionEvent event {
        .source_pid = 1234,
        .target_pid = 456,
        .target_process = "svchost.exe",
        .injection_technique = "WriteProcessMemory",
    };
    events.append(event);

    auto score = analyzer->analyze_process_injection(events);
    EXPECT_GE(score, 0.7f);  // Critical risk
}

// ... more tests
```

---

## Performance Optimization

### Parallel Analysis

```cpp
// In BehavioralAnalyzer::analyze_file():

auto file_thread = Thread::create([this, file_data]() {
    return execute_and_collect_file_events(file_data);
}).release_value();

auto process_thread = Thread::create([this, file_data]() {
    return execute_and_collect_process_events(file_data);
}).release_value();

auto memory_thread = Thread::create([this, file_data]() {
    return execute_and_collect_memory_events(file_data);
}).release_value();

auto file_events = file_thread->join().release_value();
auto process_events = process_thread->join().release_value();
auto memory_events = memory_thread->join().release_value();

// Aggregate scores in parallel
// ...
```

### Early Termination

```cpp
// If critical behavior detected early, return immediately
if (process_chain_depth > 5 || injection_detected) {
    return BehavioralVerdict {
        .verdict = Verdict::MALICIOUS,
        .behavioral_score = 0.8f,
        .execution_time_ms = elapsed_ms,
    };
}
```

---

## Platform-Specific Implementation Notes

### Windows
- Use **ETW (Event Tracing for Windows)** or **WMI** for process/registry monitoring
- Hook **Windows API** functions: `CreateFileA/W`, `RegOpenKeyEx`, `WriteProcessMemory`
- Monitor **DLL loading** via `LoadLibraryA/W`

### Linux
- Use **strace** or **seccomp-bpf** for syscall interception
- Monitor **inotify** for file system changes
- Check **/proc/[pid]/maps** for memory regions
- Use **auditd** for privilege escalation detection

### macOS
- Use **FSEvents** API for file system monitoring
- Use **System Configuration** framework for network changes
- Monitor **dylib interposition** for injection detection
- Use **code signing tools** for signature verification

---

## Graceful Degradation

```cpp
// If sandbox initialization fails, behavioral analysis is optional

ErrorOr<SecurityVerdict> SecurityTap::inspect_download_safe(
    ByteBuffer const& file_data,
    float yara_score,
    float ml_probability
) {
    // Always return valid verdict even if behavioral analyzer fails
    auto behavioral_result = BehavioralAnalyzer::create();

    if (behavioral_result.is_error()) {
        dbgln("Warning: Behavioral analyzer failed to initialize");
        // Fall back to YARA + ML only (25% less detection power)
        return SecurityVerdict::from_yara_ml(yara_score, ml_probability);
    }

    auto analyzer = behavioral_result.release_value();
    auto behavioral = analyzer->analyze_file(file_data);

    if (behavioral.is_error()) {
        dbgln("Warning: Behavioral analysis failed: {}", behavioral.error());
        // Still have YARA + ML verdict
        return SecurityVerdict::from_yara_ml(yara_score, ml_probability);
    }

    // Success - use all three signals
    return combine_verdicts(yara_score, ml_probability, behavioral.value());
}
```

---

## Integration Checklist

- [ ] `BehavioralAnalyzer.h/cpp` created with core interface
- [ ] `FileIOMonitor.h/cpp` implemented
- [ ] `ProcessMonitor.h/cpp` implemented
- [ ] `MemoryMonitor.h/cpp` implemented
- [ ] `RegistryMonitor.h/cpp` implemented (Windows)
- [ ] `PlatformMonitor.h/cpp` implemented (Linux/macOS)
- [ ] `TestBehavioralAnalyzer.cpp` with >10 test cases
- [ ] Integration into `SecurityTap` (RequestServer)
- [ ] Performance benchmarks (<5s per file)
- [ ] False positive tests (<1% on legitimate software)
- [ ] CMakeLists.txt updated to build new components

---

## References

- **Full Spec**: BEHAVIORAL_ANALYSIS_SPEC.md
- **Quick Ref**: BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md
- **Sentinel Arch**: SENTINEL_ARCHITECTURE.md
- **Coding Style**: Documentation/CodingStyle.md

---

**Created**: 2025-11-01
**Status**: Implementation-Ready
**Next Step**: Begin Phase 1 Implementation (Core BehavioralAnalyzer)

