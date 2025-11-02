# Sandbox Integration Guide

**Milestone**: 0.5 Phase 1 - Real-Time Sandboxing
**Status**: Production-Ready (Mock/Stub Implementation)
**Target Audience**: Ladybird developers integrating sandbox into request pipeline
**Last Updated**: 2025-11-01

---

## Table of Contents

1. [Quick Start (5 Minutes)](#quick-start-5-minutes)
2. [Architecture Overview](#architecture-overview)
3. [API Reference](#api-reference)
4. [Integration Patterns](#integration-patterns)
5. [Testing Guide](#testing-guide)
6. [Performance Tuning](#performance-tuning)
7. [Troubleshooting](#troubleshooting)
8. [Future Roadmap](#future-roadmap)

---

## Quick Start (5 Minutes)

### Minimal Integration Example

```cpp
#include <Services/Sentinel/Sandbox/Orchestrator.h>

using namespace Sentinel::Sandbox;

// 1. Create orchestrator (once at startup)
auto orchestrator = TRY(Orchestrator::create());

// 2. Analyze suspicious file
ByteBuffer file_data = /* downloaded file */;
auto result = TRY(orchestrator->analyze_file(file_data, "suspicious.exe"_string));

// 3. Check verdict
if (result.is_malicious()) {
    // Block and quarantine
    dbgln("MALWARE DETECTED: {}", result.verdict_explanation);
    quarantine_file(file_data);
} else if (result.is_suspicious()) {
    // Warn user
    show_security_alert(result.verdict_explanation);
} else {
    // Allow download
    save_file(file_data);
}

// 4. View detailed scores (optional)
dbgln("YARA: {:.2f}, ML: {:.2f}, Behavioral: {:.2f}",
    result.yara_score, result.ml_score, result.behavioral_score);
```

### RequestServer Integration Pattern

Add sandbox analysis between YARA scan and final verdict:

```cpp
// Services/RequestServer/ConnectionFromClient.cpp

ErrorOr<void> ConnectionFromClient::handle_download_complete(
    ByteBuffer const& file_data,
    String const& filename)
{
    // Step 1: YARA scan (existing)
    auto yara_result = TRY(m_security_tap->scan(file_data));
    if (yara_result.is_threat) {
        return quarantine_file(file_data, "YARA match"_string);
    }

    // Step 2: ML prediction (existing)
    auto ml_prediction = TRY(m_ml_detector->predict(file_data));

    // Step 3: Sandbox analysis (NEW - for medium confidence)
    if (ml_prediction.probability >= 0.5f && ml_prediction.probability <= 0.8f) {
        auto sandbox_result = TRY(m_sandbox_orchestrator->analyze_file(
            file_data, filename
        ));

        // Combine verdicts
        if (sandbox_result.is_malicious()) {
            return quarantine_file(file_data, sandbox_result.verdict_explanation);
        }

        if (sandbox_result.is_suspicious()) {
            // Alert user but allow override
            TRY(send_security_alert(sandbox_result));
        }
    }

    // Allow download
    return save_file(file_data, filename);
}
```

### Basic Usage Patterns

```cpp
// Pattern 1: Synchronous analysis (default)
auto result = TRY(orchestrator->analyze_file(file_data, "file.exe"_string));

// Pattern 2: With custom timeout
SandboxConfig config;
config.timeout = Duration::from_milliseconds(2000);
auto fast_orchestrator = TRY(Orchestrator::create(config));

// Pattern 3: With ML features pre-computed (optimization)
auto features = TRY(ml_detector->extract_features(file_data));
auto result = TRY(orchestrator->analyze_file_with_features(
    file_data, filename, features
));

// Pattern 4: Graceful degradation
auto result_or_error = orchestrator->analyze_file(file_data, filename);
if (result_or_error.is_error()) {
    dbgln("Sandbox unavailable: {}", result_or_error.error());
    // Fallback to YARA + ML only
    return handle_fallback_analysis(file_data);
}
```

---

## Architecture Overview

### Two-Tier Design

The sandbox infrastructure uses a **progressive escalation** strategy:

```
┌─────────────────────────────────────────────────────────────────┐
│ Tier 1: WASM Sandbox (Fast Pre-Analysis)                       │
│ ────────────────────────────────────────────────────────────    │
│ • Lightweight JavaScript extraction                             │
│ • WASM bytecode execution                                       │
│ • Timeout: <100ms                                               │
│ • Detection: Basic string patterns, API calls                   │
│ • Use Case: 80% of files analyzed here                          │
│                                                                  │
│ Decision:                                                        │
│   ├─ Clean (score < 0.3) → ALLOW ✅                            │
│   ├─ Malicious (score > 0.7) → BLOCK ❌                        │
│   └─ Suspicious (0.3-0.7) → Escalate to Tier 2 ⚠️              │
└─────────────────────────────────────────────────────────────────┘
                             ↓ (20% escalated)
┌─────────────────────────────────────────────────────────────────┐
│ Tier 2: Native Sandbox (Deep OS-Level Analysis)                 │
│ ────────────────────────────────────────────────────────────    │
│ • Full process isolation (nsjail/systemd-nspawn)                │
│ • System call monitoring via ptrace/seccomp                     │
│ • Timeout: <5s                                                  │
│ • Detection: Behavioral analysis, file I/O, process injection   │
│ • Use Case: Deep analysis of edge cases                         │
│                                                                  │
│ Capabilities:                                                    │
│   • File I/O pattern monitoring                                 │
│   • Process creation chains                                     │
│   • Memory manipulation detection                               │
│   • Registry/persistence checks (Windows)                       │
│   • Network initialization tracking                             │
└─────────────────────────────────────────────────────────────────┘
```

### Component Relationships

```
┌──────────────────────────────────────────────────────────────┐
│                     Orchestrator                             │
│  (Coordinates analysis pipeline)                             │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 1. YARA Pattern Matching (40% weight)              │    │
│  │    • Static signature detection                     │    │
│  │    • Match confidence: 0.0 or 1.0                   │    │
│  └─────────────────────────────────────────────────────┘    │
│                        ↓                                     │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 2. ML Feature Extraction (35% weight)              │    │
│  │    • TensorFlow Lite model prediction               │    │
│  │    • Confidence: 0.0-1.0                            │    │
│  └─────────────────────────────────────────────────────┘    │
│                        ↓                                     │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 3. Sandbox Execution (25% weight)                   │    │
│  │    ┌─────────────────┐    ┌────────────────────┐   │    │
│  │    │ WasmExecutor    │───▶│ BehavioralAnalyzer │   │    │
│  │    └─────────────────┘    └────────────────────┘   │    │
│  │              Tier 1              Tier 2             │    │
│  └─────────────────────────────────────────────────────┘    │
│                        ↓                                     │
│  ┌─────────────────────────────────────────────────────┐    │
│  │ 4. VerdictEngine                                    │    │
│  │    • Combines all signals                           │    │
│  │    • Calculates composite score                     │    │
│  │    • Determines threat level                        │    │
│  └─────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────┘
```

### Data Flow

```
File Download Completes
         │
         ├──▶ SHA256 Hash Calculation
         │
         ├──▶ PolicyGraph Cache Lookup
         │    └─ Found? → Return cached verdict ✅
         │
         ├──▶ YARA Scan (200-500ms)
         │    └─ Match? → MALICIOUS (1.0) ❌
         │
         ├──▶ ML Prediction (100-300ms)
         │    ├─ P(malware) < 0.5 → CLEAN ✅
         │    ├─ P(malware) > 0.8 → MALICIOUS ❌
         │    └─ 0.5 ≤ P(malware) ≤ 0.8 → Continue ⚠️
         │
         ├──▶ Tier 1 WASM Sandbox (<100ms)
         │    ├─ Extract features
         │    ├─ Monitor API calls
         │    └─ Score: 0.0-1.0
         │
         ├──▶ Tier 2 Native Sandbox (<5s) [if needed]
         │    ├─ Process isolation
         │    ├─ Behavioral monitoring
         │    └─ Score: 0.0-1.0
         │
         ├──▶ VerdictEngine Aggregation
         │    ├─ Composite = YARA(40%) + ML(35%) + Behavioral(25%)
         │    ├─ Confidence calculation
         │    └─ Threat level determination
         │
         ├──▶ PolicyGraph Storage
         │    └─ Cache verdict for 24 hours
         │
         └──▶ Final Action
              ├─ Clean (<0.3) → Allow download
              ├─ Suspicious (0.3-0.6) → Alert user
              └─ Malicious (>0.6) → Quarantine
```

---

## API Reference

### Orchestrator::analyze_file()

**Main entry point for sandbox analysis.**

```cpp
ErrorOr<SandboxResult> analyze_file(
    ByteBuffer const& file_data,
    String const& filename
);
```

**Parameters**:
- `file_data`: Raw file content to analyze
- `filename`: Original filename (used for heuristics)

**Returns**: `SandboxResult` with threat assessment

**Errors**:
- `ETIMEDOUT`: Analysis exceeded timeout
- `ENOMEM`: Insufficient memory for sandbox
- `ENOTSUP`: Sandbox backend unavailable
- `EIO`: Sandbox execution failed

**Performance**:
- Tier 1 only: 50-100ms (80% of files)
- Tier 1 + Tier 2: 1-5 seconds (20% of files)
- Timeout: Configurable, default 5s

**Example**:
```cpp
auto result = TRY(orchestrator->analyze_file(file_data, "document.pdf"_string));

switch (result.threat_level) {
case SandboxResult::ThreatLevel::Clean:
    dbgln("File is clean (confidence: {:.2f})", result.confidence);
    break;
case SandboxResult::ThreatLevel::Suspicious:
    dbgln("Suspicious: {}", result.verdict_explanation);
    show_user_warning(result);
    break;
case SandboxResult::ThreatLevel::Malicious:
    dbgln("MALWARE DETECTED: {}", result.verdict_explanation);
    quarantine_file(file_data);
    break;
case SandboxResult::ThreatLevel::Critical:
    dbgln("CRITICAL THREAT: {}", result.verdict_explanation);
    block_and_alert(file_data);
    break;
}
```

---

### SandboxResult Structure

**Comprehensive analysis results with multi-layer scoring.**

```cpp
struct SandboxResult {
    enum class ThreatLevel {
        Clean = 0,      // Score < 0.30
        Suspicious = 1, // Score 0.30-0.60
        Malicious = 2,  // Score 0.60-0.85
        Critical = 3    // Score > 0.85
    };

    ThreatLevel threat_level;
    float confidence;                     // 0.0-1.0
    Vector<String> detected_behaviors;    // E.g., "Rapid file modification"
    Vector<String> triggered_rules;       // E.g., "YARA:Ransomware.WannaCry"
    Duration execution_time;
    String verdict_explanation;           // User-facing message

    // Scoring breakdown
    float yara_score;        // 0.0 or 1.0 (40% weight)
    float ml_score;          // 0.0-1.0 (35% weight)
    float behavioral_score;  // 0.0-1.0 (25% weight)
    float composite_score;   // Final weighted score

    // Behavioral metrics
    u32 file_operations;     // File I/O count
    u32 process_operations;  // Process creation count
    u32 network_operations;  // Network connection attempts
    u32 registry_operations; // Registry modifications (Windows)
    u32 memory_operations;   // Suspicious memory operations

    // Convenience methods
    bool is_malicious() const;  // threat_level >= Malicious
    bool is_suspicious() const; // threat_level >= Suspicious
};
```

**Field Descriptions**:

- **threat_level**: Categorical verdict (Clean/Suspicious/Malicious/Critical)
- **confidence**: How certain we are (0.0-1.0). High confidence (>0.8) means all layers agree
- **detected_behaviors**: Human-readable list of suspicious activities
- **triggered_rules**: Specific detection rules that matched
- **verdict_explanation**: User-facing summary (e.g., "File modifies many documents rapidly")
- **composite_score**: Final score combining all layers: `YARA(40%) + ML(35%) + Behavioral(25%)`

**Example Usage**:
```cpp
auto result = TRY(orchestrator->analyze_file(file_data, "installer.exe"_string));

dbgln("Threat Level: {}", static_cast<int>(result.threat_level));
dbgln("Confidence: {:.2f}", result.confidence);
dbgln("Composite Score: {:.2f}", result.composite_score);
dbgln("Breakdown: YARA={:.2f}, ML={:.2f}, Behavioral={:.2f}",
    result.yara_score, result.ml_score, result.behavioral_score);

for (auto const& behavior : result.detected_behaviors) {
    dbgln("  - {}", behavior);
}
```

---

### SandboxConfig Options

**Configure sandbox behavior and resource limits.**

```cpp
struct SandboxConfig {
    Duration timeout { Duration::from_seconds(5) };
    bool enable_tier1_wasm { true };
    bool enable_tier2_native { true };
    bool allow_network { false };
    bool allow_filesystem { false };
    u64 max_memory_bytes { 128 * 1024 * 1024 }; // 128 MB
};
```

**Field Descriptions**:

- **timeout**: Maximum total analysis time (default 5s)
  - Tier 1 uses ~20% of timeout
  - Tier 2 uses remaining time
  - If exceeded, returns partial results with lower confidence

- **enable_tier1_wasm**: Enable fast WASM pre-analysis
  - Recommended: `true` (catches 80% of threats quickly)
  - Disable only if Wasmtime unavailable

- **enable_tier2_native**: Enable deep native sandbox
  - Recommended: `true` for production
  - Disable for testing or unsupported platforms

- **allow_network**: Allow sandboxed file to make network connections
  - Recommended: `false` (isolate network)
  - Enable only for testing network-based malware

- **allow_filesystem**: Allow sandboxed file to access filesystem
  - Recommended: `false` (read-only temp directory only)
  - Enable to test file wipers/ransomware

- **max_memory_bytes**: Memory limit for sandbox (default 128 MB)
  - Prevents DoS via memory exhaustion
  - Typical: 64-256 MB

**Example Configurations**:

```cpp
// Fast analysis (for low-risk files)
SandboxConfig fast_config;
fast_config.timeout = Duration::from_milliseconds(1000);
fast_config.enable_tier2_native = false;

// Deep analysis (for suspicious files)
SandboxConfig deep_config;
deep_config.timeout = Duration::from_seconds(10);
deep_config.max_memory_bytes = 256 * 1024 * 1024;

// Testing configuration (allow monitoring)
SandboxConfig test_config;
test_config.allow_network = true;
test_config.allow_filesystem = true;
test_config.timeout = Duration::from_seconds(30);
```

---

### Error Handling

**All sandbox operations return `ErrorOr<T>` for graceful degradation.**

```cpp
auto result_or_error = orchestrator->analyze_file(file_data, filename);

if (result_or_error.is_error()) {
    auto error = result_or_error.error();

    switch (error.code()) {
    case ETIMEDOUT:
        dbgln("Sandbox timeout - analysis incomplete");
        // Use partial results or fallback to YARA+ML
        break;

    case ENOMEM:
        dbgln("Insufficient memory for sandbox");
        // Reduce max_memory_bytes or skip sandbox
        break;

    case ENOTSUP:
        dbgln("Sandbox backend unavailable");
        // Fall back to YARA+ML only (graceful degradation)
        return analyze_without_sandbox(file_data);

    case EIO:
        dbgln("Sandbox execution failed: {}", error.string_literal());
        // Log error and quarantine for manual review
        break;

    default:
        dbgln("Unknown error: {}", error.string_literal());
    }

    // Graceful degradation: continue with YARA+ML
    return handle_fallback_analysis(file_data);
}

auto result = result_or_error.release_value();
// Use result normally
```

**Best Practices**:

1. **Always check for errors** - Don't assume sandbox is available
2. **Log errors for debugging** - Track sandbox availability issues
3. **Gracefully degrade** - Fall back to YARA+ML if sandbox fails
4. **Conservative default** - If sandbox unavailable and ML uncertain, quarantine
5. **Never crash** - Sandbox failure should not prevent browser operation

---

## Integration Patterns

### Pattern 1: RequestServer Download Flow

**Integrate sandbox into download security pipeline.**

```cpp
// Services/RequestServer/ConnectionFromClient.cpp

class ConnectionFromClient {
private:
    OwnPtr<Sentinel::Sandbox::Orchestrator> m_sandbox_orchestrator;
    OwnPtr<Sentinel::MalwareMLDetector> m_ml_detector;
    OwnPtr<SecurityTap> m_security_tap;
    OwnPtr<PolicyGraph> m_policy_graph;

public:
    ErrorOr<void> initialize_security() {
        // Initialize components
        m_security_tap = TRY(SecurityTap::create());
        m_ml_detector = TRY(MalwareMLDetector::create());

        // Initialize sandbox orchestrator
        SandboxConfig config;
        config.timeout = Duration::from_seconds(5);
        m_sandbox_orchestrator = TRY(Orchestrator::create(config));

        m_policy_graph = TRY(PolicyGraph::create());
        return {};
    }

    ErrorOr<void> on_download_complete(
        ByteBuffer const& file_data,
        String const& filename,
        String const& origin_url)
    {
        // Calculate file hash
        auto hash = TRY(calculate_sha256(file_data));

        // Step 1: Check PolicyGraph cache
        auto cached_policy = m_policy_graph->lookup_by_hash(hash);
        if (cached_policy.has_value()) {
            return apply_cached_policy(cached_policy.value());
        }

        // Step 2: YARA scan (existing)
        auto yara_result = TRY(m_security_tap->scan(file_data));
        if (yara_result.is_threat) {
            TRY(quarantine_file(file_data, hash, "YARA signature match"_string));
            TRY(store_verdict_in_policy_graph(hash, SandboxResult::ThreatLevel::Malicious));
            return {};
        }

        // Step 3: ML prediction (existing)
        auto ml_prediction = TRY(m_ml_detector->predict(file_data));

        if (ml_prediction.probability < 0.5f) {
            // Low risk - allow
            return save_file(file_data, filename);
        }

        if (ml_prediction.probability > 0.8f) {
            // High risk - block
            TRY(quarantine_file(file_data, hash, "ML high confidence malware"_string));
            TRY(store_verdict_in_policy_graph(hash, SandboxResult::ThreatLevel::Malicious));
            return {};
        }

        // Step 4: Medium confidence - run sandbox analysis
        auto sandbox_result_or_error = m_sandbox_orchestrator->analyze_file(
            file_data, filename
        );

        if (sandbox_result_or_error.is_error()) {
            dbgln("Sandbox unavailable: {}", sandbox_result_or_error.error());
            // Fallback: conservative action for uncertain files
            return show_user_confirmation_dialog(file_data, ml_prediction);
        }

        auto sandbox_result = sandbox_result_or_error.release_value();

        // Step 5: Final verdict
        if (sandbox_result.is_malicious()) {
            TRY(quarantine_file(file_data, hash, sandbox_result.verdict_explanation));
            TRY(store_verdict_in_policy_graph(hash, sandbox_result.threat_level));
        } else if (sandbox_result.is_suspicious()) {
            TRY(show_security_alert(sandbox_result));
            TRY(store_verdict_in_policy_graph(hash, sandbox_result.threat_level));
        } else {
            TRY(save_file(file_data, filename));
            TRY(store_verdict_in_policy_graph(hash, SandboxResult::ThreatLevel::Clean));
        }

        return {};
    }
};
```

---

### Pattern 2: PolicyGraph Integration

**Store sandbox verdicts for reuse and user overrides.**

```cpp
ErrorOr<void> store_verdict_in_policy_graph(
    String const& file_hash,
    SandboxResult const& result)
{
    auto policy = PolicyGraph::Policy {
        .rule_name = String::formatted("Sandbox[{}]",
            threat_level_to_string(result.threat_level)),
        .file_hash = file_hash,
        .action = verdict_to_policy_action(result.threat_level),
        .match_type = PolicyGraph::PolicyMatchType::FileHash,
        .enforcement_action = result.verdict_explanation,
        .metadata = String::formatted(
            "composite={:.2f},confidence={:.2f}",
            result.composite_score, result.confidence
        ),
        .created_at = UnixDateTime::now(),
        .created_by = "Sandbox"_string,
        .expires_at = UnixDateTime::now() + Duration::from_days(30),
    };

    auto policy_id = TRY(m_policy_graph->create_policy(policy));
    dbgln("Stored sandbox verdict: policy_id={}", policy_id);

    return {};
}

ErrorOr<Optional<PolicyGraph::Policy>> lookup_cached_verdict(
    String const& file_hash)
{
    // Query PolicyGraph for existing verdict
    auto policy = m_policy_graph->lookup_by_hash(file_hash);

    if (!policy.has_value()) {
        return Optional<PolicyGraph::Policy> {};
    }

    // Check if policy expired
    if (policy->expires_at < UnixDateTime::now()) {
        dbgln("Cached verdict expired, reanalyzing");
        return Optional<PolicyGraph::Policy> {};
    }

    dbgln("Using cached verdict: {}", policy->rule_name);
    return policy;
}
```

---

### Pattern 3: Alert Handling

**Send IPC alerts to UI for user notification.**

```cpp
ErrorOr<void> show_security_alert(SandboxResult const& result)
{
    // Build JSON alert payload
    JsonObjectBuilder alert_builder;
    alert_builder.add("type"sv, "sandbox_verdict"sv);
    alert_builder.add("threat_level"sv, static_cast<int>(result.threat_level));
    alert_builder.add("confidence"sv, result.confidence);
    alert_builder.add("explanation"sv, result.verdict_explanation);
    alert_builder.add("composite_score"sv, result.composite_score);

    // Add detected behaviors
    JsonArrayBuilder behaviors_builder;
    for (auto const& behavior : result.detected_behaviors) {
        behaviors_builder.add(behavior);
    }
    alert_builder.add("behaviors"sv, behaviors_builder.build());

    // Add scoring breakdown
    JsonObjectBuilder scores_builder;
    scores_builder.add("yara"sv, result.yara_score);
    scores_builder.add("ml"sv, result.ml_score);
    scores_builder.add("behavioral"sv, result.behavioral_score);
    alert_builder.add("scores"sv, scores_builder.build());

    auto alert_json = alert_builder.build();

    // Send via IPC to UI process
    TRY(send_ipc_message<DidReceiveSecurityAlert>({
        .severity = result.is_malicious() ? "critical"_string : "warning"_string,
        .alert_data = alert_json.to_string(),
    }));

    return {};
}
```

---

### Pattern 4: Statistics and Monitoring

**Track sandbox performance and effectiveness.**

```cpp
void log_sandbox_statistics_periodically()
{
    auto stats = m_sandbox_orchestrator->get_statistics();

    dbgln("=== Sandbox Statistics ===");
    dbgln("Total files analyzed: {}", stats.total_files_analyzed);
    dbgln("Tier 1 executions: {} ({:.1f}%)",
        stats.tier1_executions,
        100.0 * stats.tier1_executions / stats.total_files_analyzed);
    dbgln("Tier 2 executions: {} ({:.1f}%)",
        stats.tier2_executions,
        100.0 * stats.tier2_executions / stats.total_files_analyzed);
    dbgln("Malicious detected: {} ({:.1f}%)",
        stats.malicious_detected,
        100.0 * stats.malicious_detected / stats.total_files_analyzed);
    dbgln("Timeouts: {}", stats.timeouts);
    dbgln("Average Tier 1 time: {} ms", stats.average_tier1_time.to_milliseconds());
    dbgln("Average Tier 2 time: {} ms", stats.average_tier2_time.to_milliseconds());
    dbgln("Average total time: {} ms", stats.average_total_time.to_milliseconds());

    // Alert if too many timeouts
    if (stats.timeouts > stats.total_files_analyzed / 10) {
        dbgln("WARNING: High timeout rate ({:.1f}%) - consider increasing timeout",
            100.0 * stats.timeouts / stats.total_files_analyzed);
    }

    // Alert if Tier 2 usage too high
    if (stats.tier2_executions > stats.total_files_analyzed / 3) {
        dbgln("WARNING: High Tier 2 usage ({:.1f}%) - consider tuning ML thresholds",
            100.0 * stats.tier2_executions / stats.total_files_analyzed);
    }
}
```

---

## Testing Guide

### Running TestSandbox

**The comprehensive test suite validates all sandbox components.**

```bash
# Build and run test binary
cd /path/to/ladybird
cmake --build Build/release --target TestSandbox
./Build/release/bin/TestSandbox
```

**Expected output:**
```
====================================
  Sandbox Infrastructure Tests
====================================
  Milestone 0.5 Phase 1
  Real-time Sandboxing
====================================

=== Test 1: WasmExecutor Creation ===
  WasmExecutor created successfully
  Execution time: 15 ms
  YARA score: 0.00
  ML score: 0.05
✅ PASSED: WasmExecutor basic execution

=== Test 2: WasmExecutor Malicious Detection ===
  YARA score: 0.45
  ML score: 0.62
  Detected behaviors: 4
✅ PASSED: WasmExecutor detects malicious patterns

[... more tests ...]

====================================
  Test Summary
====================================
  Passed: 8
  Failed: 0
  Total:  8
====================================

✅ All tests PASSED

NOTE: This is using mock/stub implementations.
Real Wasmtime and nsjail integration coming in Phase 1b.
```

---

### Writing New Tests

**Add tests to `Services/Sentinel/TestSandbox.cpp`:**

```cpp
// Test custom behavior detection
static void test_ransomware_detection()
{
    print_section("Test: Ransomware Pattern Detection"sv);

    SandboxConfig config;
    auto orchestrator = TRY(Orchestrator::create(config));

    // Create ransomware-like data
    ByteBuffer ransomware_data;

    // PE header
    ransomware_data.append("MZ"sv.bytes());

    // Ransomware strings
    ransomware_data.append("CryptLocker"sv.bytes());
    ransomware_data.append(".encrypted"sv.bytes());
    ransomware_data.append("bitcoin"sv.bytes());

    // High entropy data (simulated encryption)
    for (size_t i = 0; i < 5000; i++)
        ransomware_data.append(static_cast<u8>(i * 137 + 42));

    auto result = TRY(orchestrator->analyze_file(
        ransomware_data, "document.pdf.exe"_string
    ));

    // Verify detection
    ASSERT(result.is_malicious());
    ASSERT(result.behavioral_score > 0.6f);
    ASSERT(result.detected_behaviors.size() > 0);

    log_pass("Ransomware pattern detected correctly"sv);
}
```

**Add test to main function:**
```cpp
ErrorOr<int> ladybird_main(Main::Arguments)
{
    // ... existing tests ...
    test_ransomware_detection();

    return tests_failed > 0 ? 1 : 0;
}
```

---

### Performance Benchmarking

**Measure sandbox performance under load:**

```cpp
static void benchmark_throughput()
{
    print_section("Benchmark: Throughput Test"sv);

    SandboxConfig config;
    auto orchestrator = TRY(Orchestrator::create(config));

    // Generate 100 test files
    Vector<ByteBuffer> test_files;
    for (size_t i = 0; i < 100; i++) {
        ByteBuffer data;
        for (size_t j = 0; j < 10000; j++)
            data.append(static_cast<u8>(i + j));
        test_files.append(move(data));
    }

    // Measure total time
    auto start = MonotonicTime::now();

    for (size_t i = 0; i < test_files.size(); i++) {
        auto result = orchestrator->analyze_file(
            test_files[i],
            String::formatted("file{}.bin", i)
        );
        if (result.is_error()) {
            dbgln("Analysis failed: {}", result.error());
        }
    }

    auto duration = MonotonicTime::now() - start;

    // Calculate throughput
    auto avg_time = duration.to_milliseconds() / test_files.size();
    auto files_per_second = 1000.0 / avg_time;

    printf("  Files analyzed: %zu\n", test_files.size());
    printf("  Total time: %ld ms\n", duration.to_milliseconds());
    printf("  Average time per file: %ld ms\n", avg_time);
    printf("  Throughput: %.2f files/second\n", files_per_second);

    // Verify statistics
    auto stats = orchestrator->get_statistics();
    printf("  Statistics: tier1=%lu, tier2=%lu, timeouts=%lu\n",
        stats.tier1_executions, stats.tier2_executions, stats.timeouts);

    if (avg_time < 200) {
        log_pass("Throughput acceptable"sv);
    } else {
        log_fail("Throughput test"sv, "Average time too high"sv);
    }
}
```

---

## Performance Tuning

### Timeout Configuration

**Adjust timeouts based on use case:**

```cpp
// Conservative (safer but slower)
SandboxConfig conservative;
conservative.timeout = Duration::from_seconds(10);

// Balanced (default)
SandboxConfig balanced;
balanced.timeout = Duration::from_seconds(5);

// Aggressive (faster but less thorough)
SandboxConfig aggressive;
aggressive.timeout = Duration::from_seconds(2);
aggressive.enable_tier2_native = false;  // Tier 1 only
```

**Timeout Guidelines**:
- **User downloads**: 5s (default) - users tolerate short wait
- **Background scans**: 10s - more thorough analysis acceptable
- **Real-time scanning**: 2s - minimize latency
- **Batch processing**: 30s - deep analysis for forensics

---

### Memory Limits

**Adjust memory allocation based on workload:**

```cpp
// Low memory environment (embedded, mobile)
SandboxConfig low_memory;
low_memory.max_memory_bytes = 64 * 1024 * 1024;  // 64 MB

// Standard desktop
SandboxConfig standard;
standard.max_memory_bytes = 128 * 1024 * 1024;  // 128 MB

// High-performance server
SandboxConfig high_perf;
high_perf.max_memory_bytes = 512 * 1024 * 1024;  // 512 MB
```

**Memory Usage by Component**:
- WasmExecutor: 20-50 MB
- BehavioralAnalyzer: 10-30 MB
- VerdictEngine: 5-10 MB
- Overhead: 10-20 MB
- **Total**: 45-110 MB typical

---

### Tier 1 vs Tier 2 Trade-offs

**Tier 1 (WASM) Advantages**:
- Fast (<100ms)
- Low memory footprint
- Cross-platform
- Safe (WASM sandboxing)

**Tier 1 Limitations**:
- Limited to JavaScript/WASM analysis
- Cannot execute native binaries
- No OS-level behavioral monitoring

**Tier 2 (Native) Advantages**:
- Full behavioral analysis
- Real process execution
- OS-level monitoring (syscalls, registry, etc.)
- Detects advanced malware

**Tier 2 Limitations**:
- Slower (1-5 seconds)
- Higher memory usage
- Platform-dependent (nsjail on Linux)
- More complex setup

**Optimization Strategy**:

```cpp
// Start with Tier 1 for all files
if (tier1_result.composite_score < 0.3) {
    // Clean - no need for Tier 2
    return tier1_result;
}

if (tier1_result.composite_score > 0.7) {
    // Clearly malicious - no need for Tier 2
    return tier1_result;
}

// Only escalate ambiguous cases (0.3-0.7) to Tier 2
return run_tier2_analysis(file_data);
```

**Expected Distribution**:
- ~70% of files: Clean (Tier 1 only, <100ms)
- ~10% of files: Clearly malicious (Tier 1 only, <100ms)
- ~20% of files: Ambiguous (Tier 1 + Tier 2, 1-5s)

---

## Troubleshooting

### Common Errors

#### Error: "Sandbox backend unavailable" (ENOTSUP)

**Symptoms**:
```
Sandbox analysis failed: Operation not supported
```

**Causes**:
1. Wasmtime not installed (Tier 1)
2. nsjail not available (Tier 2)
3. Platform not supported

**Solutions**:

```bash
# Check Wasmtime availability
which wasmtime
# Install if missing: https://wasmtime.dev/

# Check nsjail availability (Linux only)
which nsjail
# Install: apt install nsjail

# Verify sandbox initialization
./Build/release/bin/TestSandbox
```

**Workaround** (graceful degradation):
```cpp
auto result_or_error = orchestrator->analyze_file(file_data, filename);
if (result_or_error.is_error() && result_or_error.error().code() == ENOTSUP) {
    dbgln("Sandbox unavailable, using YARA+ML only");
    // Fall back to existing detection layers
    return analyze_without_sandbox(file_data);
}
```

---

#### Error: "Analysis timeout exceeded" (ETIMEDOUT)

**Symptoms**:
```
Sandbox analysis failed: Connection timed out
Stats: timeouts = 15 / 100 files (15%)
```

**Causes**:
1. Timeout too short for complex files
2. CPU overload
3. Infinite loop in sandboxed file

**Solutions**:

```cpp
// Increase timeout
SandboxConfig config;
config.timeout = Duration::from_seconds(10);  // Was 5s

// Or disable Tier 2 for faster analysis
config.enable_tier2_native = false;

auto orchestrator = TRY(Orchestrator::create(config));
```

**Analysis**:
```cpp
auto stats = orchestrator->get_statistics();
if (stats.timeouts > stats.total_files_analyzed * 0.05) {  // >5% timeout rate
    dbgln("WARNING: High timeout rate - increase timeout or disable Tier 2");
}
```

---

#### Error: "Insufficient memory" (ENOMEM)

**Symptoms**:
```
Sandbox analysis failed: Cannot allocate memory
Average memory usage: 450 MB
```

**Causes**:
1. `max_memory_bytes` too high
2. Memory leak in sandbox component
3. System memory exhaustion

**Solutions**:

```cpp
// Reduce memory limit
SandboxConfig config;
config.max_memory_bytes = 64 * 1024 * 1024;  // Reduce to 64 MB

// Or analyze files in batches
for (auto& file : files) {
    auto result = TRY(orchestrator->analyze_file(file.data, file.name));
    // Allow GC between analyses
}
```

---

### Debug Logging

**Enable verbose sandbox logging:**

```bash
# Set environment variable
export SANDBOX_DEBUG=1
./Build/release/bin/Ladybird

# Or at runtime
SANDBOX_DEBUG=1 ./Build/release/bin/Ladybird
```

**Log output:**
```
[Sandbox] Orchestrator created with timeout=5000ms
[Sandbox] Analyzing file: suspicious.exe (size=2.3 MB)
[Sandbox] Tier 1 WASM execution started
[Sandbox] WASM score: 0.62, behaviors: 4
[Sandbox] Escalating to Tier 2 (ambiguous score)
[Sandbox] Tier 2 native execution started
[Sandbox] Behavioral score: 0.71
[Sandbox] Verdict: SUSPICIOUS (composite=0.67, confidence=0.82)
[Sandbox] Analysis completed in 3245 ms
```

**Add logging to integration code:**
```cpp
#define SANDBOX_DEBUG_LOG(fmt, ...) \
    dbgln("[Sandbox] " fmt, ##__VA_ARGS__)

auto result = TRY(orchestrator->analyze_file(file_data, filename));
SANDBOX_DEBUG_LOG("Analysis result: threat_level={}, score={:.2f}",
    static_cast<int>(result.threat_level), result.composite_score);
```

---

### Graceful Degradation

**Always implement fallback logic:**

```cpp
ErrorOr<void> analyze_file_safely(ByteBuffer const& file_data, String const& filename)
{
    // Try sandbox analysis
    auto sandbox_result = m_sandbox_orchestrator->analyze_file(file_data, filename);

    if (sandbox_result.is_error()) {
        auto error = sandbox_result.error();

        // Log the specific error
        dbgln("Sandbox analysis failed ({}): {}",
            error.code(), error.string_literal());

        // Fall back to YARA + ML
        auto yara_result = TRY(m_security_tap->scan(file_data));
        if (yara_result.is_threat) {
            return quarantine_file(file_data, "YARA match"_string);
        }

        auto ml_result = TRY(m_ml_detector->predict(file_data));
        if (ml_result.probability > 0.7f) {
            return quarantine_file(file_data, "ML high confidence"_string);
        }

        // Conservative action for medium confidence
        if (ml_result.probability > 0.5f) {
            return show_user_warning(ml_result);
        }

        // Allow low-risk files
        return save_file(file_data, filename);
    }

    // Use sandbox result
    return handle_sandbox_result(sandbox_result.release_value());
}
```

**Never let sandbox failure crash the browser:**
```cpp
// ❌ BAD - crashes on error
auto result = MUST(orchestrator->analyze_file(file_data, filename));

// ✅ GOOD - graceful degradation
auto result_or_error = orchestrator->analyze_file(file_data, filename);
if (result_or_error.is_error()) {
    return handle_fallback(file_data);
}
```

---

## Future Roadmap

### Phase 1b: Wasmtime Integration (Q1 2026)

**Objective**: Replace WASM stubs with real Wasmtime execution.

**Tasks**:
- Integrate Wasmtime C API
- Implement JavaScript extraction from files
- Add WASM module compilation and execution
- Monitor WASM API calls (DOM, Canvas, etc.)
- Performance optimization (<100ms target)

**API Changes**: None (drop-in replacement)

---

### Phase 1c: nsjail Integration (Q1 2026)

**Objective**: Replace native sandbox stubs with real nsjail isolation.

**Tasks**:
- Integrate nsjail for process isolation
- Implement seccomp syscall filtering
- Add ptrace-based behavioral monitoring
- Resource limits via cgroups
- Performance optimization (<5s target)

**Platform Support**:
- Linux: Full support (nsjail)
- macOS: Sandbox-exec fallback
- Windows: WSL2 + nsjail or native sandbox API

---

### Phase 2: Production ML Models (Q2 2026)

**Objective**: Train production malware detection models.

**Current State**: Mock ML scores (random 0.0-1.0)

**Future**:
- Train TensorFlow Lite model on real malware dataset
- Feature engineering (entropy, imports, strings, etc.)
- Federated learning for privacy-preserving updates
- Model versioning and A/B testing

---

### Phase 3: Advanced Behavioral Analysis (Q2 2026)

**Objective**: Expand behavioral detection capabilities.

**New Features**:
- Ransomware detection (file encryption patterns)
- Information stealer detection (credential access)
- Rootkit detection (privilege escalation)
- Cryptominer detection (CPU usage patterns)

**API Additions**:
```cpp
struct BehavioralMetrics {
    u32 files_modified;
    float avg_file_entropy;
    u32 sensitive_dir_accesses;
    u32 privilege_escalation_attempts;
    // ...
};
```

---

### Phase 4: User Experience Improvements (Q3 2026)

**Objective**: Better user communication and control.

**Features**:
- Interactive sandbox UI (observe execution)
- Natural language explanations of threats
- User-customizable policies
- Threat education and recommendations
- Visual behavior graphs

**Example**:
```
┌─────────────────────────────────────────────────────┐
│ ⚠️  Suspicious File Detected                        │
├─────────────────────────────────────────────────────┤
│                                                     │
│ File: installer.exe                                 │
│ Threat Level: Suspicious (72% confidence)           │
│                                                     │
│ Why this is suspicious:                             │
│ • Modified 150 files in 8 seconds (ransomware?)    │
│ • High entropy data written (encryption?)           │
│ • Persistence registry key created                  │
│                                                     │
│ Recommendation: Block and quarantine                │
│                                                     │
│ [View Details] [Quarantine] [Trust This File]      │
└─────────────────────────────────────────────────────┘
```

---

## References

### Related Documentation

- **SANDBOX_ARCHITECTURE.md**: Detailed system architecture
- **BEHAVIORAL_ANALYSIS_SPEC.md**: Behavioral metrics specification
- **SANDBOX_TECHNOLOGY_COMPARISON.md**: Technology evaluation
- **SENTINEL_ARCHITECTURE.md**: Overall Sentinel system design
- **SENTINEL_POLICY_GUIDE.md**: PolicyGraph integration

### External Resources

- **Wasmtime**: https://wasmtime.dev/
- **nsjail**: https://github.com/google/nsjail
- **TensorFlow Lite**: https://www.tensorflow.org/lite
- **YARA Rules**: https://yara.readthedocs.io/

### Example Code

- **TestSandbox.cpp**: Comprehensive test suite (`Services/Sentinel/TestSandbox.cpp`)
- **Orchestrator Implementation**: `Services/Sentinel/Sandbox/Orchestrator.{h,cpp}`
- **RequestServer Integration**: `Services/RequestServer/ConnectionFromClient.cpp`

---

**Document Version**: 1.0
**Last Updated**: 2025-11-01
**Status**: Production-Ready (Mock Implementation)
**Next Review**: Phase 1b completion (Wasmtime integration)

---

## Quick Reference Card

### Essential Imports
```cpp
#include <Services/Sentinel/Sandbox/Orchestrator.h>
using namespace Sentinel::Sandbox;
```

### Basic Analysis
```cpp
auto orchestrator = TRY(Orchestrator::create());
auto result = TRY(orchestrator->analyze_file(file_data, filename));
if (result.is_malicious()) quarantine_file();
```

### Custom Configuration
```cpp
SandboxConfig config;
config.timeout = Duration::from_seconds(2);
config.enable_tier2_native = false;
auto orchestrator = TRY(Orchestrator::create(config));
```

### Error Handling
```cpp
auto result_or_error = orchestrator->analyze_file(file_data, filename);
if (result_or_error.is_error()) {
    return handle_fallback(file_data);
}
```

### Statistics
```cpp
auto stats = orchestrator->get_statistics();
dbgln("Analyzed: {}, Malicious: {}",
    stats.total_files_analyzed, stats.malicious_detected);
```

### Performance Targets
- **Tier 1**: <100ms (80% of files)
- **Tier 2**: <5s (20% of files)
- **Memory**: <128 MB per sandbox
- **False Positive Rate**: <1%
