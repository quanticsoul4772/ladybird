# Real-Time File Sandboxing Architecture for Ladybird Sentinel

**Version**: 0.5.0 (Real-Time Sandboxing - Phase 1)
**Last Updated**: 2025-11-01
**Audience**: Developers and System Architects
**Status**: Design Specification

**Milestone**: 0.5 Phase 1 - Real-Time Sandboxing
**Previous Milestones**:
- 0.1.0: Malware Scanning (Download Protection)
- 0.2.0: Credential Protection (Form Submission Monitoring)
- 0.3.0: Enhanced Credential Protection
- 0.4.0: Advanced Detection (ML, Fingerprinting, Phishing)

---

## Table of Contents

1. [System Overview](#system-overview)
2. [Architecture Principles](#architecture-principles)
3. [Component Specifications](#component-specifications)
4. [Data Structures](#data-structures)
5. [API Specifications](#api-specifications)
6. [Execution Flow](#execution-flow)
7. [Integration Architecture](#integration-architecture)
8. [Security Considerations](#security-considerations)
9. [Performance Requirements](#performance-requirements)
10. [Failure Handling](#failure-handling)
11. [Testing Strategy](#testing-strategy)

---

## System Overview

### High-Level Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                      Ladybird Browser                               │
│  ┌──────────────────────────────────────────────────────────┐       │
│  │  UI Layer (Qt/AppKit)                                    │       │
│  │  - SandboxVerdict Dialog (user notification)             │       │
│  │  - about:security (sandbox analysis results)             │       │
│  └──────────────────────────────────────────────────────────┘       │
│                           ↕ IPC                                     │
│  ┌──────────────────────────────────────────────────────────┐       │
│  │  WebContent Process (per tab, sandboxed)                 │       │
│  └──────────────────────────────────────────────────────────┘       │
└─────────────────────────────────────────────────────────────────────┘
                           ↕ IPC
┌─────────────────────────────────────────────────────────────────────┐
│  RequestServer Process                                              │
│  ├─ SecurityTap: YARA scanning [0.1]                              │
│  ├─ URLSecurityAnalyzer: Phishing detection [0.4]                 │
│  └─ Quarantine: File isolation [0.1]                              │
└─────────────────────────────────────────────────────────────────────┘
                           ↕ Unix Socket
┌─────────────────────────────────────────────────────────────────────┐
│  SentinelServer Daemon (Standalone)                                 │
│  ├─ YARA Rule Engine [0.1]                                         │
│  ├─ MalwareML Detector [0.4]                                       │
│  ├─ Sandbox Orchestrator [0.5] ◄────── NEW                        │
│  │   ├─ SandboxExecutor (runs files in isolated environment)       │
│  │   ├─ BehavioralAnalyzer (monitors behavior signals)             │
│  │   └─ SandboxVerdict Engine (combines signals)                   │
│  ├─ PolicyGraph: SQLite Database [0.1+]                           │
│  └─ Threat Feed: Syndicated threat intel [0.4]                    │
└─────────────────────────────────────────────────────────────────────┘
```

### Design Principles

1. **Layered Analysis**: YARA → ML → Behavioral (each layer adds confidence)
2. **Safe Defaults**: Default to blocking suspicious files
3. **User Control**: Final verdict can be overridden with PolicyGraph policies
4. **Graceful Degradation**: Browser continues if sandbox unavailable
5. **Performance**: <5 second analysis time (with 5-second timeout)
6. **Extensibility**: Pluggable behavioral metrics and sandboxing backends
7. **Privacy**: All processing local, no cloud dependencies
8. **Isolation**: Sandbox runs in separate OS process (WASM or container)

---

## Architecture Principles

### 1. Verdict Confidence Levels

Files are classified into four verdict tiers:

```
┌──────────────────────────────────────────────────────┐
│  YARA Scan (Static Analysis)                        │
│  └─ Is matched by YARA rule?                       │
│     ├─ YES → Verdict: MALWARE (high confidence)    │
│     └─ NO → Next tier                              │
└──────────────────────────────────────────────────────┘
                       ↓
┌──────────────────────────────────────────────────────┐
│  ML Analysis (Behavioral Prediction)                 │
│  └─ What is predicted malware probability?         │
│     ├─ High (>0.8) → Verdict: SUSPICIOUS           │
│     ├─ Medium (0.5-0.8) → Sandbox analysis         │
│     └─ Low (<0.5) → Verdict: BENIGN                │
└──────────────────────────────────────────────────────┘
                       ↓
┌──────────────────────────────────────────────────────┐
│  Behavioral Sandbox Analysis (Runtime Monitoring)   │
│  └─ What is the file doing in isolation?           │
│     ├─ Aggressive behavior detected → MALWARE      │
│     ├─ Suspicious behavior detected → SUSPICIOUS   │
│     └─ No behavioral anomalies → BENIGN            │
└──────────────────────────────────────────────────────┘
```

### 2. Sandboxing Strategy

**Tier 1: WASM Sandbox** (Primary - lightweight)
- JavaScript extracted from files
- DOM operations monitored
- Network access restricted
- File system access blocked
- Execution timeout: 2 seconds

**Tier 2: Container Sandbox** (Fallback - heavyweight)
- Full process isolation via container (Docker/systemd-nspawn)
- System call filtering
- Network isolation
- Resource limits (CPU, memory, disk)
- Execution timeout: 5 seconds

**Tier 3: System Integration** (Optional)
- KVM/QEMU virtualization
- Full OS isolation
- Used only for extremely suspicious files

### 3. Integration Pattern

Sandbox analysis is **optional** - only triggered for medium-confidence threats:

```
┌─ File downloaded ─────────────────────────────────┐
│                                                   │
├─ YARA scan: Match? ──── YES ──→ Block + Alert   │
│                                                   │
├─ ML analysis: P(malware) > 0.8? ──→ Block      │
│                                                   │
├─ P(malware) ∈ [0.5, 0.8]? ──────┐                │
│                                  │                │
│  NO ─ P(malware) < 0.5 ─→ Allow │                │
│                                  ↓                │
│                    Sandbox analysis (NEW)         │
│                    ├─ Timeout: 5 seconds         │
│                    ├─ Behavior scoring          │
│                    └─ Final verdict             │
│                                  │                │
│                          ┌─────────────────┐      │
│                          │  Final Verdict  │      │
│                          └─────────────────┘      │
│                                  ↓                │
│                    ┌─────────────────────────┐   │
│                    │ - Block (malware)       │   │
│                    │ - Warn (suspicious)     │   │
│                    │ - Allow (benign)        │   │
│                    │ - Quarantine (analyze)  │   │
│                    └─────────────────────────┘   │
└───────────────────────────────────────────────────┘
```

---

## Component Specifications

### Component 1: Sandbox Orchestrator

**Location**: `Services/Sentinel/Sandbox/Orchestrator.{h,cpp}`

**Responsibility**: Manage sandbox lifecycle, coordinate analysis, aggregate verdicts

**Key Methods**:
- Create sandbox instances
- Submit files for analysis
- Monitor analysis progress
- Aggregate verdicts from all detection layers
- Handle timeouts and failures

**Data Dependencies**:
- MalwareMLDetector (ML predictions)
- PolicyGraph (verdict persistence)
- Threat intelligence feeds

**Thread Safety**: Thread-safe, uses EventLoop for async operations

---

### Component 2: Sandbox Executor

**Location**: `Services/Sentinel/Sandbox/Executor.{h,cpp}`

**Responsibility**: Execute files in isolated environment, collect behavioral signals

**Key Methods**:
- Execute file in WASM sandbox
- Execute file in container sandbox
- Monitor system calls
- Collect behavioral reports
- Enforce execution timeout

**Sandboxing Backends**:

1. **WASM Executor** (primary):
   - Uses WASM bytecode execution
   - Monitors JavaScript API calls
   - No network/file system access
   - Lightweight (< 50MB memory)
   - Execution: < 2 seconds

2. **Container Executor** (fallback):
   - Uses systemd-nspawn or Docker
   - Full process isolation
   - System call filtering via seccomp
   - Resource limits via cgroups
   - Execution: < 5 seconds

**Platform Support**:
- Linux: Full support (WASM + containers)
- macOS: WASM only (via JavaScriptCore)
- Windows: Container support (via WSL2)

---

### Component 3: Behavioral Analyzer

**Location**: `Services/Sentinel/Sandbox/BehavioralAnalyzer.{h,cpp}`

**Responsibility**: Monitor and analyze runtime behavior, compute behavior score

**Key Behavioral Metrics**:

```cpp
enum class BehaviorSignal {
    // Suspicious behaviors (high risk)
    SuspiciousNetworkAccess,        // Outbound to known C2
    CredentialHarvesting,           // Reads /etc/passwd, Windows registry
    ProcessInjection,               // CreateRemoteThread, ptrace
    PersistenceAttempt,             // Writes to startup folders
    PrivilegeEscalation,            // Attempts to elevate privileges
    AntiVirtualization,             // Checks for VM detection
    AntiDebugging,                  // Detects debugger
    CodeObfuscation,                // Uses packing, encryption

    // Moderately suspicious behaviors
    FileEncryption,                 // Encrypts local files
    MassFileOperations,             // Bulk file operations (>100 files)
    RegistryModification,           // Modifies Windows registry
    ServiceDisabling,               // Disables antivirus, firewall
    TemporaryFileCreation,          // Creates many temp files

    // Low-risk behaviors (informational)
    NetworkConnection,              // Makes network connection
    FileSystemAccess,               // Reads/writes files
    ProcessCreation,                // Spawns child processes
    SystemQueryAPI,                 // Queries system information
    EnvironmentVariableAccess       // Reads environment variables
};

struct BehaviorEvent {
    UnixDateTime timestamp;
    BehaviorSignal signal;
    String description;
    Optional<String> details;  // e.g., target domain for network access
    u32 severity { 0 };        // 0-100 severity score
};
```

**Behavior Scoring Algorithm**:

```cpp
float compute_behavior_score(Vector<BehaviorEvent> const& events) {
    // Weighted scoring based on signal frequency and severity
    float suspicious_score = 0.0f;

    // Count occurrences of each signal type
    for (auto const& event : events) {
        switch (event.signal) {
            case BehaviorSignal::SuspiciousNetworkAccess:
                suspicious_score += 30.0f;  // Very suspicious
                break;
            case BehaviorSignal::CredentialHarvesting:
                suspicious_score += 25.0f;
                break;
            case BehaviorSignal::ProcessInjection:
                suspicious_score += 20.0f;
                break;
            // ... other cases ...
            case BehaviorSignal::NetworkConnection:
                suspicious_score += 2.0f;   // Low risk
                break;
            default:
                break;
        }
    }

    // Normalize to 0.0-1.0 range
    float normalized_score = AK::min(1.0f, suspicious_score / 100.0f);

    // Apply temporal decay - recent events weighted more
    if (!events.is_empty()) {
        auto now = UnixDateTime::now();
        for (auto const& event : events) {
            auto age_ms = (now - event.timestamp).to_milliseconds();
            float decay = AK::max(0.5f, 1.0f - (age_ms / 5000.0f));
            normalized_score *= decay;
        }
    }

    return normalized_score;
}
```

**Monitoring Methods**:
- ptrace(2) for system call tracing (Linux)
- dtrace/kqueue for process monitoring (macOS)
- ETW (Event Tracing for Windows) for Windows
- WASM API hooks for JavaScript execution

---

### Component 4: Verdict Engine

**Location**: `Services/Sentinel/Sandbox/VerdictEngine.{h,cpp}`

**Responsibility**: Combine YARA, ML, and behavioral signals into final verdict

**Verdict Logic**:

```cpp
enum class SandboxVerdict {
    Malware,      // Confident threat - block immediately
    Suspicious,   // Medium confidence - warn user, quarantine
    Benign,       // Likely safe - allow download
    FailedAnalysis // Analysis error - quarantine for manual review
};

struct VerdictSignals {
    // YARA scan result
    bool yara_match { false };

    // ML prediction (0.0-1.0)
    float ml_malware_probability { 0.0f };
    float ml_confidence { 0.0f };

    // Behavioral analysis (0.0-1.0)
    float behavior_score { 0.0f };
    u32 suspicious_behavior_count { 0 };

    // Previous verdicts (policy history)
    Optional<SandboxVerdict> policy_previous_verdict;
    i64 policy_hit_count { 0 };
};

SandboxVerdict compute_verdict(VerdictSignals const& signals) {
    // Priority 1: YARA match → always malware
    if (signals.yara_match) {
        return SandboxVerdict::Malware;
    }

    // Priority 2: High ML confidence → malware
    if (signals.ml_malware_probability > 0.8f && signals.ml_confidence > 0.7f) {
        return SandboxVerdict::Malware;
    }

    // Priority 3: Aggressive behavior → malware
    if (signals.behavior_score > 0.8f && signals.suspicious_behavior_count >= 3) {
        return SandboxVerdict::Malware;
    }

    // Priority 4: Combined medium confidence → suspicious
    float combined_score = (
        (signals.ml_malware_probability * 0.5f) +
        (signals.behavior_score * 0.5f)
    );

    if (combined_score > 0.6f) {
        return SandboxVerdict::Suspicious;
    }

    // Priority 5: All signals low → benign
    return SandboxVerdict::Benign;
}
```

**Verdict Thresholds**:

| Signal | Threshold | Verdict |
|--------|-----------|---------|
| YARA match | Any | Malware |
| ML probability | > 0.80 | Malware |
| Behavior score | > 0.80 | Malware |
| Combined score | > 0.60 | Suspicious |
| Default | - | Benign |

---

### Component 5: Reporter

**Location**: `Services/Sentinel/Sandbox/Reporter.{h,cpp}`

**Responsibility**: Generate reports, notify UI, log results to PolicyGraph

**Output Formats**:

1. **User Notification**:
   - UI Dialog with verdict and recommendation
   - Option to override with user action
   - Link to detailed security report

2. **Detailed Report**:
   - File metadata (name, hash, size)
   - YARA matches
   - ML prediction and feature importance
   - Behavioral analysis report
   - Confidence score and recommendation

3. **PolicyGraph Logging**:
   - Store verdict for future reference
   - Track verdicts over time
   - Support policy decision making

---

## Data Structures

### SandboxRequest

```cpp
struct SandboxRequest {
    // Identification
    String request_id;          // Unique request identifier

    // File metadata
    ByteString filename;
    ByteString mime_type;
    ByteString url;             // Origin URL
    u64 file_size { 0 };
    ByteString sha256_hash;

    // File content
    ByteBuffer file_data;

    // Analysis parameters
    enum class AnalysisLevel {
        Fast,       // WASM only, <2s
        Standard,   // WASM + behavior, <5s
        Deep,       // Container, <10s
        Full        // Full VM isolation, <30s
    };
    AnalysisLevel level { AnalysisLevel::Standard };

    // Timestamps
    UnixDateTime created_at { UnixDateTime::now() };
    UnixDateTime started_at;
    UnixDateTime completed_at;

    // Timeout configuration
    u32 timeout_ms { 5000 };    // Default 5 seconds
};
```

### BehavioralReport

```cpp
struct BehavioralReport {
    // Identification
    String sandbox_id;
    String file_hash;

    // Execution summary
    UnixDateTime start_time;
    UnixDateTime end_time;
    u32 execution_time_ms { 0 };
    bool timeout_exceeded { false };

    // Behavioral signals
    Vector<BehaviorEvent> events;
    u32 total_events { 0 };
    u32 suspicious_count { 0 };
    u32 benign_count { 0 };

    // Resource usage
    struct ResourceMetrics {
        u64 peak_memory_kb { 0 };
        f32 cpu_usage_percent { 0.0f };
        u64 disk_io_kb { 0 };
        u64 network_bytes_transferred { 0 };
    };
    ResourceMetrics resources;

    // Analysis summary
    float behavior_score { 0.0f };
    String summary_text;
    Vector<String> recommendations;
};
```

### SandboxVerdict

```cpp
struct SandboxVerdict {
    // Identification
    String verdict_id;
    String file_hash;

    // Verdict classification
    enum class Verdict {
        Malware,           // Confirmed threat
        Suspicious,        // Medium-confidence threat
        Benign,            // Likely safe
        FailedAnalysis     // Couldn't complete analysis
    };
    Verdict classification { Verdict::Benign };

    // Confidence score (0.0-1.0)
    float confidence { 0.0f };

    // Contributing factors
    struct ContributingFactors {
        bool yara_matched { false };
        Optional<float> ml_probability;
        Optional<float> behavior_score;
        Optional<String> policy_verdict;
    };
    ContributingFactors factors;

    // User action override
    bool user_override { false };
    Optional<String> override_reason;

    // Timestamps
    UnixDateTime created_at { UnixDateTime::now() };
    UnixDateTime expires_at;  // Auto-expire after 90 days

    // Recommendation
    enum class Recommendation {
        Allow,              // Safe to download
        Warn,               // Show user warning
        Quarantine,         // Isolate from system
        Block               // Prevent download
    };
    Recommendation recommended_action { Recommendation::Allow };
    String explanation;        // Human-readable explanation

    // Policy enforcement
    Optional<i64> policy_id;   // Associated PolicyGraph policy
};
```

### SandboxStatistics

```cpp
struct SandboxStatistics {
    // Counters
    u64 total_analyses { 0 };
    u64 analyses_by_verdict[4] { 0 };  // [Malware, Suspicious, Benign, FailedAnalysis]
    u64 analyses_timed_out { 0 };

    // Performance metrics
    f32 average_analysis_time_ms { 0.0f };
    f32 min_analysis_time_ms { 0.0f };
    f32 max_analysis_time_ms { 0.0f };

    // Behavioral analysis
    u64 unique_behavior_signals_observed { 0 };
    HashMap<BehaviorSignal, u64> signal_frequency;

    // Sandbox backend usage
    u64 wasm_analyses { 0 };
    u64 container_analyses { 0 };
    u64 wasm_to_container_escalations { 0 };

    // Error handling
    u64 sandbox_startup_failures { 0 };
    u64 sandbox_crashes { 0 };
    u64 connection_failures { 0 };

    // User interactions
    u64 user_overrides { 0 };
    u64 policy_overrides { 0 };
};
```

---

## API Specifications

### Sandbox Orchestrator API

```cpp
namespace Sentinel::Sandbox {

class Orchestrator {
public:
    /// Create sandbox orchestrator instance
    /// Returns ErrorOr to handle initialization failures
    static ErrorOr<NonnullOwnPtr<Orchestrator>> create();

    ~Orchestrator() = default;

    // Analysis request
    /// Submit file for real-time sandboxing analysis
    /// \param request - Analysis request with file metadata and content
    /// \return Verdict result with classification and confidence
    /// \throws Error if analysis fails or times out
    ErrorOr<SandboxVerdict> analyze_file(SandboxRequest const& request);

    /// Async version - non-blocking analysis
    /// \param request - Analysis request
    /// \param callback - Called on completion with verdict
    using AnalysisCallback = Function<void(ErrorOr<SandboxVerdict>)>;
    void analyze_file_async(
        SandboxRequest const& request,
        AnalysisCallback callback
    );

    // Verdict override
    /// Override a previous verdict (e.g., user allows blocked file)
    /// \param file_hash - SHA256 hash of file
    /// \param new_verdict - User-selected verdict
    /// \param reason - Optional explanation for override
    ErrorOr<void> override_verdict(
        ByteString const& file_hash,
        SandboxVerdict::Verdict new_verdict,
        Optional<String> reason = {}
    );

    // Statistics and monitoring
    /// Get current sandbox statistics
    SandboxStatistics get_statistics() const;

    /// Reset statistics counters
    void reset_statistics();

    /// Get active analysis count
    size_t active_analysis_count() const;

    // Configuration
    /// Set analysis timeout (milliseconds)
    void set_analysis_timeout_ms(u32 timeout_ms);

    /// Get analysis timeout
    u32 analysis_timeout_ms() const;

    /// Enable/disable sandbox analysis for specific verdict range
    void set_analysis_enabled_for_ml_range(f32 min_prob, f32 max_prob);

private:
    Orchestrator();

    ErrorOr<SandboxVerdict> execute_analysis(SandboxRequest const& request);
    ErrorOr<void> setup_sandbox_backends();

    // Components
    OwnPtr<Executor> m_executor;
    OwnPtr<BehavioralAnalyzer> m_behavior_analyzer;
    OwnPtr<VerdictEngine> m_verdict_engine;
    OwnPtr<Reporter> m_reporter;

    // Configuration
    u32 m_analysis_timeout_ms { 5000 };
    f32 m_ml_min_probability { 0.5f };
    f32 m_ml_max_probability { 0.8f };

    // Statistics
    SandboxStatistics m_statistics;

    // Active analyses
    HashMap<String, SandboxRequest> m_active_analyses;
    NonnullRefPtr<Core::EventLoop> m_event_loop;
};

} // namespace Sentinel::Sandbox
```

### Executor API

```cpp
class Executor {
public:
    static ErrorOr<NonnullOwnPtr<Executor>> create();

    ~Executor();

    /// Execute file in sandbox
    /// \param request - Analysis request
    /// \return Behavioral report with collected signals
    ErrorOr<BehavioralReport> execute(SandboxRequest const& request);

    /// Async execution
    using ExecutionCallback = Function<void(ErrorOr<BehavioralReport>)>;
    void execute_async(
        SandboxRequest const& request,
        ExecutionCallback callback
    );

    /// Stop ongoing execution
    void cancel_execution(String const& request_id);

    /// Check if sandbox is available
    bool is_available() const;

private:
    Executor();

    // Backend selection
    ErrorOr<SandboxType> select_sandbox_backend(SandboxRequest const& request);

    // WASM execution
    ErrorOr<BehavioralReport> execute_in_wasm(SandboxRequest const& request);

    // Container execution
    ErrorOr<BehavioralReport> execute_in_container(SandboxRequest const& request);

    // Backend management
    OwnPtr<WASMExecutor> m_wasm_executor;
    OwnPtr<ContainerExecutor> m_container_executor;

    u32 m_wasm_timeout_ms { 2000 };
    u32 m_container_timeout_ms { 5000 };
};
```

### Behavioral Analyzer API

```cpp
class BehavioralAnalyzer {
public:
    static ErrorOr<NonnullOwnPtr<BehavioralAnalyzer>> create();

    ~BehavioralAnalyzer() = default;

    /// Start monitoring process for behavior signals
    /// \param pid - Process ID to monitor
    /// \return Behavior monitor handle
    ErrorOr<i32> start_monitoring(pid_t pid);

    /// Stop monitoring and get report
    /// \param monitor_id - Monitor handle from start_monitoring
    /// \return Behavioral report with all detected signals
    ErrorOr<BehavioralReport> stop_monitoring(i32 monitor_id);

    /// Record behavior event
    void record_event(i32 monitor_id, BehaviorEvent const& event);

    /// Compute behavior score from events
    /// \param events - Vector of detected behavior events
    /// \return Score 0.0-1.0 (0 = benign, 1 = malicious)
    float compute_behavior_score(Vector<BehaviorEvent> const& events) const;

    /// Get monitored process count
    size_t active_monitors() const;

private:
    BehavioralAnalyzer();

    // System call tracing (Linux)
    #ifdef __linux__
    ErrorOr<void> setup_ptrace(pid_t pid);
    #endif

    // DTrace monitoring (macOS)
    #ifdef __APPLE__
    ErrorOr<void> setup_dtrace(pid_t pid);
    #endif

    HashMap<i32, Vector<BehaviorEvent>> m_active_monitors;
    i32 m_next_monitor_id { 1 };
};
```

### Verdict Engine API

```cpp
class VerdictEngine {
public:
    static ErrorOr<NonnullOwnPtr<VerdictEngine>> create();

    ~VerdictEngine() = default;

    /// Compute verdict from analysis signals
    /// \param yara_match - YARA rule match result
    /// \param ml_prediction - ML malware probability
    /// \param behavior_report - Behavioral analysis
    /// \param policy_graph - Previous verdicts from PolicyGraph
    /// \return Final verdict with confidence
    ErrorOr<SandboxVerdict> compute_verdict(
        bool yara_match,
        Optional<MalwareMLDetector::Prediction> const& ml_prediction,
        BehavioralReport const& behavior_report,
        Optional<SandboxVerdict> const& policy_previous = {}
    );

    /// Set verdict threshold
    void set_malware_confidence_threshold(f32 threshold);

    /// Set behavior score threshold
    void set_behavior_threshold(f32 threshold);

private:
    VerdictEngine();

    SandboxVerdict merge_signals(VerdictSignals const& signals) const;

    f32 m_malware_confidence_threshold { 0.8f };
    f32 m_behavior_threshold { 0.6f };
};
```

### Reporter API

```cpp
class Reporter {
public:
    static ErrorOr<NonnullOwnPtr<Reporter>> create(
        NonnullRefPtr<PolicyGraph> policy_graph
    );

    ~Reporter() = default;

    /// Generate user-facing notification
    /// \param verdict - Verdict to report
    /// \return Notification message for UI
    ErrorOr<String> generate_notification(SandboxVerdict const& verdict);

    /// Generate detailed analysis report
    /// \param verdict - Verdict with analysis data
    /// \param behavior_report - Behavioral analysis details
    /// \return HTML-formatted detailed report
    ErrorOr<String> generate_detailed_report(
        SandboxVerdict const& verdict,
        BehavioralReport const& behavior_report
    );

    /// Log verdict to PolicyGraph
    /// \param verdict - Verdict to store
    /// \return Policy ID if created
    ErrorOr<Optional<i64>> log_to_policy_graph(SandboxVerdict const& verdict);

    /// Notify UI about verdict
    void notify_ui(SandboxVerdict const& verdict);

private:
    Reporter(NonnullRefPtr<PolicyGraph> policy_graph);

    NonnullRefPtr<PolicyGraph> m_policy_graph;
};

} // namespace Sentinel::Sandbox
```

---

## Execution Flow

### Complete Analysis Workflow

```
1. DOWNLOAD COMPLETES
   │
   └─> RequestServer receives file
       ├─ Compute SHA256 hash
       └─ Create SandboxRequest metadata

2. YARA SCAN (Existing - SecurityTap)
   │
   └─> static_cast<bool> yara_match?
       ├─ YES → Verdict: MALWARE (no sandbox needed)
       └─ NO → Continue to ML analysis

3. ML PREDICTION (Existing - MalwareML)
   │
   └─> ml_probability = predict(file_features)
       ├─ > 0.8 && confidence > 0.7 → Verdict: MALWARE
       ├─ ∈ [0.5, 0.8] → Trigger sandbox analysis [NEW]
       └─ < 0.5 → Verdict: BENIGN (skip sandbox)

4. SANDBOX INITIALIZATION [NEW]
   │
   ├─> Orchestrator::analyze_file(SandboxRequest)
   │
   ├─> Select sandbox backend:
   │   ├─ JavaScript-heavy? → WASM executor (2s timeout)
   │   ├─ Executable? → Container executor (5s timeout)
   │   └─ Unknown? → WASM first, escalate if timeout
   │
   └─> Start execution monitoring

5. SANDBOX EXECUTION [NEW]
   │
   ├─> Executor::execute()
   │   ├─ Setup sandbox environment
   │   ├─ Start process in isolation
   │   └─ Start behavioral monitoring
   │
   ├─> BehavioralAnalyzer monitors syscalls:
   │   ├─ ptrace(2) on Linux
   │   ├─ dtrace on macOS
   │   └─ ETW on Windows
   │
   ├─> Collect BehaviorEvents:
   │   ├─ Every suspicious syscall recorded
   │   ├─ Timestamps and details captured
   │   └─ Resource usage tracked
   │
   └─> Timeout enforcement:
       ├─ After 2-5 seconds, kill sandbox
       └─ Analyze collected events

6. BEHAVIORAL ANALYSIS [NEW]
   │
   └─> BehavioralAnalyzer::compute_behavior_score()
       ├─ Weight suspicious behaviors: +30 points each
       ├─ Weight moderately suspicious: +10 points each
       ├─ Weight benign: +2 points each
       ├─ Apply temporal decay (older events weighted less)
       └─ Normalize to 0.0-1.0 range

7. VERDICT COMPUTATION [NEW]
   │
   └─> VerdictEngine::compute_verdict()
       ├─ Input: yara_match, ml_probability, behavior_score
       ├─ Apply thresholds:
       │  ├─ YARA match → MALWARE
       │  ├─ ML > 0.8 && confidence > 0.7 → MALWARE
       │  ├─ Behavior > 0.8 → MALWARE
       │  ├─ Combined > 0.6 → SUSPICIOUS
       │  └─ Otherwise → BENIGN
       └─ Set confidence score

8. POLICY ENFORCEMENT [NEW]
   │
   └─> Check PolicyGraph for previous verdicts:
       ├─ User override policy? → Apply override
       ├─ Similar file seen? → Check previous verdict
       └─ Adjust recommendation if needed

9. REPORTING [NEW]
   │
   ├─> Reporter::generate_notification()
   │   └─ Create user-facing message
   │
   ├─> Reporter::generate_detailed_report()
   │   └─ Create full analysis report (HTML)
   │
   ├─> Reporter::log_to_policy_graph()
   │   └─ Store verdict for future reference
   │
   └─> Reporter::notify_ui()
       └─ Send IPC message to UI process

10. USER NOTIFICATION
    │
    └─> UI shows SandboxVerdict dialog:
        ├─ File: example.exe
        ├─ Verdict: SUSPICIOUS
        ├─ Confidence: 72%
        ├─ Reason: [from behavior_report.summary_text]
        ├─ Behaviors observed:
        │  ├─ • Registry modification
        │  ├─ • Temporary file creation
        │  └─ • Network connection to 192.168.1.1
        └─ Actions: [View Report] [Block] [Quarantine] [Allow]

11. FINAL ACTION
    │
    └─> Based on user choice or recommendation:
        ├─ Block → Delete file, alert user
        ├─ Quarantine → Move to quarantine folder
        ├─ Allow → Permit download, log override
        └─ View Report → Show detailed analysis HTML
```

### Time Budget Analysis

```
Total time budget: 5000 ms (5 seconds)

Typical execution breakdown:
├─ YARA scan: 200-500 ms
├─ ML prediction: 100-300 ms
├─ WASM sandbox execution: 1000-2000 ms (with events)
├─ Container fallback: 3000-5000 ms
├─ Behavioral analysis: 100-200 ms
├─ Verdict computation: 50-100 ms
├─ PolicyGraph lookup: 10-50 ms
├─ Reporting: 50-100 ms
└─ IPC communication: 50-100 ms
   ─────────────────────────
   Total: 1560-3450 ms (average case)

Conservative estimate: 4000-4500 ms worst case
                      (WASM timeout + process cleanup + IPC)

Budget remaining: 500-1000 ms for contingencies
```

---

## Integration Architecture

### Integration with Existing Components

#### 1. Integration with SecurityTap (RequestServer)

```cpp
// Services/RequestServer/ConnectionFromClient.cpp
// Existing flow:
// 1. Download completes
// 2. SecurityTap::inspect_download() → YARA scan
// 3. If match → quarantine
// 4. If no match → release to user

// NEW: Add sandbox analysis for medium-confidence threats
ErrorOr<SecurityTap::ScanResult> result = security_tap->inspect_download(
    metadata, file_content
);

if (!result.is_error() && !result.value().is_threat) {
    // YARA negative: check ML confidence
    auto ml_prediction = TRY(ml_detector->analyze_file(file_content));

    if (ml_prediction.malware_probability >= 0.5f &&
        ml_prediction.malware_probability <= 0.8f) {
        // Medium confidence: trigger sandbox analysis
        auto sandbox_request = SandboxRequest {
            .filename = metadata.filename,
            .mime_type = metadata.mime_type,
            .url = metadata.url,
            .file_size = file_content.size(),
            .sha256_hash = metadata.sha256,
            .file_data = file_content,
        };

        auto sandbox_verdict = TRY(
            sandbox_orchestrator->analyze_file(sandbox_request)
        );

        // Incorporate sandbox verdict into final decision
        notify_user_of_verdict(sandbox_verdict);
    }
}
```

#### 2. Integration with MalwareML (Sentinel)

```cpp
// Services/Sentinel/SentinelServer.cpp
// In SentinelServer::scan_content():

// Step 1: YARA scan (existing)
auto yara_match = yara_engine.scan(content);

// Step 2: ML prediction (existing)
auto ml_prediction = TRY(m_ml_detector->analyze_file(content));

// Step 3: Conditional sandbox analysis (NEW)
if (ml_prediction.malware_probability >= 0.5f &&
    ml_prediction.malware_probability <= 0.8f) {

    auto sandbox_request = SandboxRequest {
        .file_data = ByteBuffer(content),
        .level = SandboxRequest::AnalysisLevel::Standard,
        .timeout_ms = 5000,
    };

    auto sandbox_verdict = TRY(
        m_sandbox_orchestrator->analyze_file(sandbox_request)
    );

    // Merge verdicts
    return merge_yara_ml_sandbox_verdicts(
        yara_match,
        ml_prediction,
        sandbox_verdict
    );
}
```

#### 3. Integration with PolicyGraph (Verdict Storage)

```cpp
// Services/Sentinel/Sandbox/Reporter.cpp
// Store verdict in PolicyGraph for future reference

ErrorOr<Optional<i64>> Reporter::log_to_policy_graph(
    SandboxVerdict const& verdict
) {
    auto policy = PolicyGraph::Policy {
        .rule_name = String::formatted(
            "Sandbox[{}]", verdict.classification
        ),
        .file_hash = verdict.file_hash,
        .action = to_policy_action(verdict.recommended_action),
        .match_type = PolicyGraph::PolicyMatchType::DownloadOriginFileType,
        .enforcement_action = String::formatted(
            "sandbox_verdict:{}", verdict.classification
        ),
        .created_at = UnixDateTime::now(),
        .created_by = "Sandbox"_string,
        .expires_at = UnixDateTime::now() + Duration::from_days(90),
    };

    return TRY(m_policy_graph->create_policy(policy));
}
```

#### 4. UI Integration

```cpp
// Services/WebContent/PageClient.cpp (or Tab.cpp)
// Send verdict to UI via IPC

void PageClient::on_sandbox_verdict(
    Sentinel::Sandbox::SandboxVerdict const& verdict
) {
    // Forward to UI process
    send_ipc_message<ShowSecurityAlert>({
        .title = "File Analysis Required"_string,
        .message = verdict.explanation,
        .file_name = verdict.file_hash,
        .verdict = verdict.classification,
        .confidence = verdict.confidence,
        .recommendation = verdict.recommended_action,
    });
}
```

---

## Security Considerations

### Sandbox Escape Prevention

#### 1. WASM Sandbox Isolation

**Guarantees**:
- No access to host file system (except temporary sandbox directory)
- No direct syscall access
- No host memory access
- No ability to load native libraries
- All operations confined to WASM memory

**Risk Mitigation**:
- Use well-tested WASM runtime (V8, SpiderMonkey, WasmVM)
- Disable WASM features that enable ROP:
  - No data sections
  - No memory growth beyond sandbox allocation
  - No indirect function calls to arbitrary addresses
- Strictly validate all API calls from WASM

#### 2. Container Sandbox Isolation

**Guarantees** (via systemd-nspawn):
- Network namespace isolation (no outbound access unless allowed)
- PID namespace isolation (can't see/signal host processes)
- Mount namespace isolation (no access to host filesystem)
- IPC namespace isolation (can't communicate with host)

**Risk Mitigation**:
- Run container as unprivileged user (UID > 65534)
- Enable seccomp filter to block dangerous syscalls:
  - No ptrace/execve/fork (process isolation)
  - No raw socket access
  - No module loading
- Use read-only root filesystem
- Bind mount only temp directories with limited permissions
- Enforce CPU and memory limits via cgroups
- Monitor resource usage for DoS attempts

#### 3. Time-Based Isolation

```cpp
// Enforce strict execution timeout
constexpr u32 WASM_TIMEOUT_MS = 2000;
constexpr u32 CONTAINER_TIMEOUT_MS = 5000;

// Signal timeout after deadline:
// SIGTERM at 90% of timeout
// SIGKILL at 100% of timeout
auto deadline = UnixDateTime::now() + Duration::from_milliseconds(timeout_ms);
auto sigterm_time = deadline - Duration::from_milliseconds(timeout_ms / 10);
```

#### 4. Resource Limits

```cpp
// cgroups memory limit: 512MB
// cgroups CPU shares: 256 (1/4 of host CPU)
// cgroups max processes: 64

struct SandboxResourceLimits {
    u64 memory_limit_bytes { 512 * 1024 * 1024 };
    u32 cpu_shares { 256 };
    u32 max_processes { 64 };
    u32 max_file_descriptors { 32 };
};
```

### Security Properties

| Threat | Mitigation | Confidence |
|--------|-----------|-----------|
| Sandbox escape via ROP | Address space randomization + WASM memory isolation | High |
| Privilege escalation | Run as unprivileged user + seccomp filter | High |
| Resource exhaustion | cgroups limits + timeout enforcement | High |
| Network access (container) | Network namespace isolation | High |
| File system access | Read-only root + mount namespace | Medium* |
| Process injection | seccomp ptrace filter | High |
| Anti-debugging | No debugger access in namespace | High |
| VM detection | Minimal attack surface (no /proc/cpuinfo access) | Medium |

*Container can read bound mounts - validate all input paths

### Attack Surface Reduction

1. **Principle of Least Privilege**:
   - Run sandbox as dedicated `_ladybird` user
   - No capability escalation (CAP_SYS_* blocked)
   - No setuid/setgid allowed

2. **Input Validation**:
   - All file paths validated before binding to container
   - File size limits enforced before execution
   - MIME type validation before sandbox selection

3. **Output Validation**:
   - Behavioral reports sanitized before display
   - No direct shell execution allowed
   - All system calls logged and parsed safely

4. **Logging and Auditing**:
   - All verdict decisions logged to PolicyGraph
   - Failed sandboxes logged with error details
   - User overrides tracked for audit trail

---

## Performance Requirements

### Analysis Time Budget

```
Total Budget: 5000 ms

Phase | Time | Budget | Margin
------|------|--------|-------
YARA | 200-500 ms | 500 ms | Good
ML | 100-300 ms | 400 ms | Good
Sandbox Setup | 100 ms | 200 ms | Good
Sandbox Execution | 1500-2000 ms | 2500 ms | Good
Event Processing | 200 ms | 500 ms | Good
Verdict + Reporting | 100 ms | 500 ms | Good
IPC + UI | 100 ms | 300 ms | Good
─────────────────────────────
Total Average Case: ~2200 ms
Worst Case: ~4500 ms
Reserve: 500 ms for cleanup
```

### Scalability Requirements

**Concurrent Analyses**:
- Support up to 10 concurrent sandbox analyses
- Queue additional requests (up to 100)
- Oldest queued requests time out after 30 seconds

**Backend Scaling**:
- WASM executor: Single instance per Sentinel process
- Container executor: Pool of 3-5 instances
- Auto-scale container pool if queue depth > 10

### Memory Budget

| Component | Budget | Notes |
|-----------|--------|-------|
| Orchestrator | 10 MB | Caches, queues |
| WASM executor | 50 MB | WASM runtime + analysis memory |
| Container executor | 100 MB | Per container * 5 |
| Behavioral analyzer | 20 MB | Event buffers |
| Verdict engine | 5 MB | Thresholds, rules |
| Total | ~500 MB | Acceptable for daemon |

---

## Failure Handling

### Failure Modes and Recovery

#### 1. Sandbox Unavailable

```cpp
// If sandbox orchestrator fails to initialize:
// - Log error with severity ERROR
// - Continue using YARA + ML only (graceful degradation)
// - Skip sandbox analysis stage
// - Return "FailedAnalysis" verdict only if ML is inconclusive

ErrorOr<SandboxVerdict> Orchestrator::analyze_file(
    SandboxRequest const& request
) {
    // Try sandbox analysis
    auto sandbox_result = execute_analysis(request);

    if (sandbox_result.is_error()) {
        dbgln("Sandbox analysis failed: {}", sandbox_result.error());

        // Graceful degradation: use ML verdict as fallback
        if (m_fallback_ml_available) {
            return create_verdict_from_ml_only(request);
        }

        return SandboxVerdict {
            .classification = SandboxVerdict::FailedAnalysis,
            .confidence = 0.0f,
            .recommended_action = SandboxVerdict::Recommendation::Quarantine,
            .explanation = "Unable to complete analysis. File quarantined for manual review."_string,
        };
    }

    return sandbox_result.release_value();
}
```

#### 2. Sandbox Timeout

```cpp
// File takes too long to analyze:
// - Kill sandbox process at deadline
// - Analyze partial behavioral report
// - Mark verdict as "partial" in results

struct BehavioralReport {
    bool timeout_exceeded { false };
    u32 execution_time_ms { 0 };
    // ... rest of report ...
};

// Verdict computation adjusts confidence if partial:
if (behavior_report.timeout_exceeded) {
    final_verdict.confidence *= 0.8f;  // Reduce confidence by 20%
    final_verdict.explanation +=
        " (Analysis timeout - partial results)"_string;
}
```

#### 3. Sandbox Crash

```cpp
// If sandbox process crashes:
// - Capture exit code and signal
// - Log crash details
// - Escalate threat level (suspicious = likely crash-inducing exploit)
// - Attempt clean restart

class Executor {
private:
    ErrorOr<BehavioralReport> execute_in_container(
        SandboxRequest const& request
    ) {
        auto process = TRY(spawn_sandbox_process());

        // Wait with timeout
        auto status = TRY(wait_with_timeout(
            process.pid, request.timeout_ms
        ));

        // Check for crash
        if (WIFSIGNALED(status)) {
            dbgln("Sandbox crashed with signal: {}", WTERMSIG(status));

            auto report = BehavioralReport {
                .timeout_exceeded = true,  // Treat crash as timeout
                .summary_text = String::formatted(
                    "Sandbox process crashed with signal {}",
                    WTERMSIG(status)
                ),
            };

            return report;
        }

        return build_report_from_events();
    }
};
```

#### 4. PolicyGraph Unavailable

```cpp
// If PolicyGraph can't be written:
// - Log warning
// - Continue with verdict
// - Lose ability to track verdicts over time
// - Accept verdict anyway (user safety first)

ErrorOr<Optional<i64>> Reporter::log_to_policy_graph(
    SandboxVerdict const& verdict
) {
    if (!m_policy_graph) {
        dbgln("PolicyGraph unavailable - verdict not logged");
        return Optional<i64> {};  // Return empty, don't fail
    }

    return m_policy_graph->create_policy(policy);
}
```

### Error Recovery Strategy

```
Error Level | Action | User Impact
------------|--------|-------------
Warning | Log, continue | None - background
Error | Quarantine file | File blocked (safe)
Fatal | Block browser feature | User notified, alternative provided
```

**Recovery Priority**:
1. User safety (always block questionable files)
2. Browser functionality (degrade gracefully)
3. Data persistence (PolicyGraph can be empty)
4. Performance (timeouts are better than hangs)

---

## Testing Strategy

### Unit Tests

```cpp
// Services/Sentinel/TestSandbox.cpp

void test_sandbox_verdict_computation() {
    auto engine = TRY(VerdictEngine::create());

    // Test 1: YARA match → MALWARE
    auto verdict = TRY(engine.compute_verdict(
        /* yara_match= */ true,
        /* ml_prediction= */ {},
        /* behavior= */ {}
    ));
    ASSERT_EQ(verdict.classification, SandboxVerdict::Malware);

    // Test 2: ML high probability → MALWARE
    auto ml_pred = MalwareMLDetector::Prediction {
        .malware_probability = 0.85f,
        .confidence = 0.8f,
    };
    verdict = TRY(engine.compute_verdict(
        /* yara_match= */ false,
        ml_pred,
        /* behavior= */ {}
    ));
    ASSERT_EQ(verdict.classification, SandboxVerdict::Malware);

    // Test 3: Combined medium → SUSPICIOUS
    auto behavior = BehavioralReport {
        .behavior_score = 0.65f,
        .suspicious_count = 2,
    };
    ml_pred.malware_probability = 0.6f;
    verdict = TRY(engine.compute_verdict(false, ml_pred, behavior));
    ASSERT_EQ(verdict.classification, SandboxVerdict::Suspicious);
}

void test_behavior_timeout() {
    auto executor = TRY(Executor::create());

    auto request = SandboxRequest {
        .file_data = create_infinite_loop_binary(),
        .timeout_ms = 1000,
        .level = SandboxRequest::AnalysisLevel::Fast,
    };

    auto report = TRY(executor.execute(request));
    ASSERT(report.timeout_exceeded);
}

void test_sandbox_escape_prevention() {
    auto executor = TRY(Executor::create());

    // Test 1: Try to read host /etc/passwd
    auto request = SandboxRequest {
        .file_data = create_file_read_binary("/etc/passwd"),
        .timeout_ms = 2000,
    };

    auto report = TRY(executor.execute(request));
    ASSERT(!report.events.any([](auto const& e) {
        return e.signal == BehaviorSignal::CredentialHarvesting;
    }));

    // Test 2: Try to execute syscall directly
    request.file_data = create_raw_syscall_binary();
    report = TRY(executor.execute(request));
    ASSERT_FALSE(WIFEXITED(report.execution_result));  // Should crash
}
```

### Integration Tests

```cpp
void test_end_to_end_analysis() {
    // Setup
    auto orchestrator = TRY(Orchestrator::create());

    // Test with benign file
    auto benign_request = SandboxRequest {
        .filename = "document.pdf"_string,
        .file_data = create_valid_pdf(),
    };

    auto verdict = TRY(orchestrator.analyze_file(benign_request));
    ASSERT_EQ(verdict.classification, SandboxVerdict::Benign);

    // Test with suspicious file
    auto suspicious_request = SandboxRequest {
        .filename = "suspicious.exe"_string,
        .file_data = create_suspicious_executable(),
    };

    verdict = TRY(orchestrator.analyze_file(suspicious_request));
    ASSERT_EQ(verdict.classification, SandboxVerdict::Suspicious);
    ASSERT(verdict.confidence > 0.5f);

    // Verify logged to PolicyGraph
    auto stats = orchestrator.get_statistics();
    ASSERT_EQ(stats.total_analyses, 2);
}
```

### Performance Tests

```cpp
void test_analysis_latency() {
    auto orchestrator = TRY(Orchestrator::create());

    Vector<u32> latencies;

    for (int i = 0; i < 100; ++i) {
        auto request = SandboxRequest {
            .file_data = generate_random_file(1 * 1024 * 1024),  // 1MB
        };

        auto start = UnixDateTime::now();
        auto verdict = TRY(orchestrator.analyze_file(request));
        auto end = UnixDateTime::now();

        latencies.append((end - start).to_milliseconds());
    }

    auto average = std::accumulate(latencies.begin(), latencies.end(), 0)
        / latencies.size();
    auto max = *std::max_element(latencies.begin(), latencies.end());

    ASSERT(average < 2500);  // Average < 2.5 seconds
    ASSERT(max < 5000);      // Max < 5 seconds
}
```

### Security Tests

```cpp
void test_sandbox_file_isolation() {
    auto executor = TRY(Executor::create());

    // Create a sensitive file on host
    auto host_file = TRY(File::open("/tmp/sensitive.txt", File::OpenMode::Write));
    TRY(host_file->write_entire_buffer("SECRET_DATA"sv));

    // Try to read from within sandbox
    auto request = SandboxRequest {
        .file_data = create_file_reader_binary("/tmp/sensitive.txt"),
        .timeout_ms = 2000,
    };

    auto report = TRY(executor.execute(request));

    // Should not be able to read host file
    ASSERT(!report.events.any([](auto const& e) {
        return e.description.contains("SECRET_DATA"sv);
    }));
}
```

---

## Future Enhancements

### Phase 2: Extended Behavior Analysis

- **WebGL Fingerprinting Detection**: Monitor canvas API calls
- **Audio Context Fingerprinting**: Track audio API usage
- **Navigator Spoofing**: Detect attempts to identify user agent
- **DOM API Monitoring**: Track DOM manipulation patterns
- **Network Traffic Analysis**: Inspect HTTP/HTTPS requests

### Phase 3: Multi-Backend Sandboxing

- **KVM/QEMU Integration**: Full OS-level isolation
- **Kubernetes Pod Isolation**: Distributed sandbox clusters
- **Browser-Native Sandbox**: Leverage Chromium sandbox
- **Custom Container Runtimes**: Optimized for malware analysis

### Phase 4: ML Integration

- **Behavioral ML Model**: Predict malware from behavior patterns
- **Anomaly Detection**: Statistical detection of unusual behavior
- **Threat Correlation**: Link related verdicts
- **Federated Learning**: Share threat signals across deployments

### Phase 5: User Experience

- **Interactive Sandbox**: Allow user to observe sandbox execution
- **Behavior Explanations**: Natural language descriptions of suspicious activity
- **Policy Templates**: Pre-built policies for common scenarios
- **Threat Education**: Educate users about detected threats

---

## References

### Related Documents

- [SENTINEL_ARCHITECTURE.md](./SENTINEL_ARCHITECTURE.md) - Overall Sentinel design
- [SENTINEL_POLICY_GUIDE.md](./SENTINEL_POLICY_GUIDE.md) - Policy graph usage
- [FINGERPRINTING_DETECTION_ARCHITECTURE.md](./FINGERPRINTING_DETECTION_ARCHITECTURE.md) - Browser fingerprinting detection

### External References

- Linux seccomp: https://man7.org/linux/man-pages/man2/seccomp.2.html
- systemd-nspawn: https://man7.org/linux/man-pages/man1/systemd-nspawn.1.html
- WASM security: https://webassembly.org/docs/security/
- ptrace security: https://man7.org/linux/man-pages/man2/ptrace.2.html

---

## Appendix A: Configuration Example

```cpp
// Example Sentinel configuration for sandboxing

constexpr struct SandboxConfig {
    // Analysis triggers
    f32 ml_min_probability = 0.50f;  // Trigger sandbox above this
    f32 ml_max_probability = 0.80f;  // Don't sandbox above this

    // Execution limits
    u32 wasm_timeout_ms = 2000;
    u32 container_timeout_ms = 5000;
    u32 max_file_size_bytes = 100 * 1024 * 1024;  // 100MB

    // Resource limits
    u64 container_memory_mb = 512;
    u32 container_cpu_shares = 256;
    u32 container_max_processes = 64;

    // Behavior scoring
    f32 behavior_malware_threshold = 0.80f;
    f32 behavior_suspicious_threshold = 0.60f;

    // Verdict thresholds
    f32 combined_malware_threshold = 0.70f;
    f32 combined_suspicious_threshold = 0.50f;

    // Caching
    size_t verdict_cache_size = 10000;
    Duration verdict_cache_ttl = Duration::from_days(90);

} SANDBOX_CONFIG;
```

---

## Appendix B: Example Analysis Report

```
FILE ANALYSIS REPORT
═══════════════════════════════════════════════════════════════

File: Download_2025-11-01.exe
Hash: 8d3f5e8c2a1b4f9c7e5d3a1b9f8c6e4a
Size: 2.3 MB
MIME Type: application/octet-stream
Source: https://example.com/software

ANALYSIS RESULTS
───────────────────────────────────────────────────────────────

1. YARA SCAN (Static Analysis)
   Status: ✗ No matches
   Time: 345 ms

2. ML ANALYSIS (Behavioral Prediction)
   Malware Probability: 62.5%
   Confidence: 0.78
   Status: ⚠ Medium confidence
   Time: 210 ms

3. SANDBOX ANALYSIS (Runtime Monitoring)
   Status: ✓ Completed
   Execution Time: 1823 ms
   Timeout: No

   Behaviors Observed:
   • Registry modification (HKLM\Software\Microsoft\Windows\Run)
   • Temporary file creation (C:\Windows\Temp\*)
   • Network connection to 192.168.1.100:8080
   • Process creation (svchost.exe spawn)

   Suspicious Signals: 4
   Score: 0.67 (Suspicious)

FINAL VERDICT
───────────────────────────────────────────────────────────────

Classification: SUSPICIOUS
Confidence: 0.70 (High)
Recommendation: QUARANTINE

Reasoning:
- ML prediction (62.5%) + Behavioral analysis (0.67) suggest
  suspicious but not definitive malware activity
- Registry and process creation patterns consistent with
  potentially unwanted behavior
- Network connection to private IP unusual for legitimate software

SUGGESTED ACTION
───────────────────────────────────────────────────────────────

⚠ QUARANTINE file for manual review by security team
  - Allow only if from trusted source
  - Monitor system if user chooses to override

Quarantine Location: ~/.local/share/Ladybird/Quarantine/
Quarantine Policy: Auto-delete after 30 days
Review Status: Pending manual analysis

═══════════════════════════════════════════════════════════════
```

