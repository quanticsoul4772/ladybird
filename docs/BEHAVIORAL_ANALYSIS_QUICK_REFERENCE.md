# Behavioral Analysis Quick Reference Guide

**Quick links to critical sections in BEHAVIORAL_ANALYSIS_SPEC.md**

---

## Key Concepts

### What is Behavioral Analysis?
Dynamic detection of malicious patterns during file execution in an isolated sandbox, complementing static YARA signatures and ML predictions.

### Why Three Detection Methods?
```
YARA + ML + Behavioral = Complete Coverage

├─ YARA (40%): Matches known signatures (confident but limited to known threats)
├─ ML (35%): Statistical patterns (good for zero-days, some false positives)
└─ Behavioral (25%): Runtime patterns (detects novel exploits & persistence)
```

### Execution Timeline
```
0ms  ├─ File received
100ms├─ YARA signature scan
200ms├─ If unknown → ML prediction
300ms├─ If suspicious (score 0.3-0.6) → Sandbox execution
4000ms├─ Behavioral analysis complete
4500ms├─ Combined scoring & PolicyGraph cache
5000ms└─ Verdict delivered to browser
```

---

## Critical Behavioral Metrics (Ranked by Risk)

### CRITICAL Risk (0.7-0.95)

| Pattern | How to Detect | Example | Why Dangerous |
|---------|---------------|---------|---------------|
| **Credential Access** | SAM hive/keychain access | `open("/etc/shadow")` | Steal passwords, crypto keys |
| **Security Bypass** | Disable Windows Defender | Registry: `DisableBehaviorMonitoring` | Prevent detection, gain control |
| **Process Injection** | WriteProcessMemory to system process | Inject into `svchost.exe` | Rootkit, hide in system process |
| **Privilege Escalation** | setuid/kernel module load | `insmod malicious.ko` | SYSTEM-level compromise |

### HIGH Risk (0.4-0.7)

| Pattern | How to Detect | Example | Why Dangerous |
|---------|---------------|---------|---------------|
| **Ransomware** | 100+ files in <10s | Encrypt documents folder | Extortion, data loss |
| **Persistence** | Registry Run key, cron job | `HKLM\...\Run\malware` | Survives reboot |
| **Information Stealing** | Access Chrome/SSH/Documents | `open("~/.ssh/id_rsa")` | Steal data for fraud |
| **Process Chain** | >3 process depth | `exe → powershell → cmd → rundll32` | Hide, escalate privileges |

### MEDIUM Risk (0.2-0.4)

| Pattern | How to Detect | Example | Why Dangerous |
|---------|---------------|---------|---------------|
| **High Entropy Writes** | Encrypted file writes (entropy >7.0) | Write ciphertext | Ransomware or data exfiltration |
| **Suspicious Subprocess** | Obfuscated PowerShell commands | `powershell -nop -w hidden -enc ...` | Evade antivirus |
| **Archive Extraction** | 2+ levels of archive nesting | Extract → Execute → Extract | Multi-stage delivery |
| **Heap Spray** | >100 identical allocations | Alloc 0x20000 bytes 1000x | Exploit kit technique |

---

## Scoring Quick Math

### Three-Factor Score
```python
final_score = (
    yara_score * 0.40 +      # 1.0 if signature match, else 0.0
    ml_probability * 0.35 +  # 0.0-1.0 from neural network
    behavioral_score * 0.25  # 0.0-1.0 from metrics below
)

# Verdict
if final_score < 0.30:
    return "BENIGN - Safe to open"
elif final_score < 0.60:
    return "SUSPICIOUS - Review before opening"
else:
    return "MALICIOUS - Block & quarantine"
```

### Behavioral Score Components
```python
behavioral_score = (
    file_io_metrics * 0.40 +        # File modification, encryption, directory targeting
    process_metrics * 0.30 +        # Process chain, injection, subprocess commands
    memory_metrics * 0.15 +         # Heap spray, RWX memory, shellcode
    registry_metrics * 0.10 +       # Windows persistence, credential access
    platform_metrics * 0.05         # Linux privilege escalation, macOS code signing
)
```

### Example: Ransomware Verdict
```
File: ransom.exe

YARA: No signature match → score = 0.0
ML: Neural network confidence 0.72 → score = 0.72

Behavioral Analysis:
  - Files modified: 150 in 8s → 0.75
  - Entropy: 7.8 (encrypted) → 0.70
  - Document folder targeted → 0.50
  - Process depth: 2 (normal) → 0.05
  - Registry: Run key → 0.35
  → behavioral_score = 0.295

Final Score = (0.0 * 0.40) + (0.72 * 0.35) + (0.295 * 0.25)
            = 0.0 + 0.252 + 0.074
            = 0.326 → SUSPICIOUS (>0.30) ✓

User sees: "Suspicious behavior detected: rapid file modification
with encryption pattern. Block recommended."
```

---

## Implementation Roadmap

### Phase 1: Core Behavioral Analyzer (Week 1-2)
```cpp
Services/Sentinel/BehavioralAnalyzer.{h,cpp}
├─ FileIOMonitor: Track file modifications, entropy, directories
├─ ProcessMonitor: Track creation, injection, subprocess commands
├─ MemoryMonitor: Track allocation patterns, RWX regions
└─ RegistryMonitor (Windows): Track persistence, credentials

Testing:
  └─ TestBehavioralAnalyzer.cpp with mock malware patterns
```

### Phase 2: Sandbox Integration (Week 2-3)
```cpp
Services/Sentinel/Sandbox.{h,cpp}
├─ Execute file in isolated environment
├─ Invoke BehavioralAnalyzer
├─ Collect metrics for 3-4 seconds
└─ Return verdict to RequestServer

Integration Point:
  SecurityTap::inspect_download() → call sandbox if ML score 0.3-0.6
```

### Phase 3: PolicyGraph Caching (Week 3)
```sql
CREATE TABLE behavioral_analysis_cache (
    file_hash TEXT,
    behavioral_score REAL,
    combined_verdict TEXT,
    user_decision TEXT,
    expires_at DATETIME
);

Benefits:
  - Skip reanalysis of known files
  - Cache valid 24 hours
  - Reduced sandbox executions
```

### Phase 4: User Feedback Loop (Week 4)
```
User overrides verdict → Update PolicyGraph
  "I know this file is safe" → Whitelist hash, future bypass
  "This was malware" → Increase behavioral score weight
```

---

## Platform-Specific Focus Areas

### Windows Priority Metrics
1. **Registry Persistence** → Scan HKLM\Run, HKCU\Run, Scheduled Tasks
2. **Process Injection** → Monitor WriteProcessMemory + CreateRemoteThread
3. **Credential Theft** → Monitor SAM hive, Credential Manager access
4. **DLL Injection Chains** → Detect explorer.exe injection (very suspicious)

**Detection Tools**: Windows API hooks, WMI events, ETW providers

### Linux Priority Metrics
1. **Privilege Escalation** → Monitor setuid/setgid, sudo abuse, kernel modules
2. **Persistence** → Monitor /etc/cron.d, /etc/systemd/system changes
3. **Credential Theft** → Monitor /root/.ssh, /etc/shadow access
4. **Rootkit Behavior** → Monitor kernel module loading, ptrace to system processes

**Detection Tools**: strace, seccomp, inotify, auditd

### macOS Priority Metrics
1. **Code Signing Bypass** → Monitor xattr removal, signature stripping
2. **Launchd Persistence** → Monitor ~/Library/LaunchAgents, LaunchDaemons
3. **Keychain Access** → Monitor ~/Library/Keychains access
4. **Quarantine Bypass** → Monitor com.apple.quarantine xattr modification

**Detection Tools**: FSEvents, dylib interposition, code signing tools

---

## False Positive Prevention

### Built-In Whitelists

**Installer Programs**
- Microsoft Office, Windows Setup, Visual Studio → allow 100+ file mods
- Registry Run key writes → expected from installers

**System Utilities**
- Windows Defender, Antivirus → allow credential access
- Backup software → allow sensitive directory access

**Developer Tools**
- Debuggers → allow process injection into target
- Compilers → allow high entropy writes (object files)

### Hash-Based Exceptions
```
SHA256(software_installer.exe)
  → {verdict: BENIGN, reason: known_installer}
  → Cache for 24 hours
```

### Certificate Verification
```
IF (file_signed_by_trusted_publisher):
    confidence *= 1.5  // Boost confidence in benign verdict
ELSE:
    confidence *= 0.7  // Reduce confidence (generic samples)
```

---

## Performance Targets

| Metric | Target | Critical? |
|--------|--------|-----------|
| **Time per file** | <5 seconds | YES |
| **Memory per sandbox** | <100 MB | YES |
| **CPU (burst)** | Single core, 100% | OK |
| **Disk cache** | <1 GB | NO (can purge) |
| **False positives** | <1% on clean files | YES |
| **False negatives** | <10% on malware | YES |

---

## Decision Tree: Is This File Suspicious?

```
START
  ↓
[YARA Signature Match?]
  ├─ YES → MALICIOUS (confidence 0.95+) ✗
  └─ NO → continue

[ML Model Confidence > 0.8?]
  ├─ YES → MALICIOUS (confidence 0.85+) ✗
  └─ NO → continue

[ML Model Confidence 0.3-0.8?]
  ├─ YES → RUN SANDBOX (behavioral analysis)
  │   ├─ [File modification rapid?] → MALICIOUS ✗
  │   ├─ [Sensitive dirs accessed?] → SUSPICIOUS ⚠
  │   ├─ [Persistence installed?] → MALICIOUS ✗
  │   ├─ [Injection detected?] → MALICIOUS ✗
  │   └─ [No red flags?] → BENIGN ✓
  └─ NO → continue

[File signed by trusted publisher?]
  ├─ YES → BENIGN ✓
  └─ NO → SUSPICIOUS ⚠

[User override?]
  ├─ Block → MALICIOUS ✗
  ├─ Allow → BENIGN ✓
  └─ Default → SUSPICIOUS ⚠
```

---

## Integration Checklist

- [ ] **BehavioralAnalyzer** component created with all metric collectors
- [ ] **Sandbox** component created with process isolation
- [ ] **Sandbox integration** into SecurityTap workflow
- [ ] **Scoring algorithm** tests pass (verify example scenarios)
- [ ] **PolicyGraph schema** updated with behavior cache table
- [ ] **UI updates** for user notifications (in about:security)
- [ ] **Performance benchmarks** meet <5s budget
- [ ] **False positive** tests with legitimate installers (<1% FP rate)
- [ ] **Malware detection** tests with known families (>90% detection)
- [ ] **Cross-platform** tests (Windows, Linux, macOS)

---

## Common Implementation Pitfalls

### ❌ Too Aggressive (High False Positives)
```
Problem: Flagging all file modifications as ransomware
Solution: Use time windows and thresholds (100+ files in <10s, not just 50)
```

### ❌ Too Conservative (Missed Detections)
```
Problem: Only detecting well-known patterns
Solution: Include behavioral anomalies (process chain depth, entropy)
```

### ❌ Performance Timeout
```
Problem: Complex analysis exceeds 5 second budget
Solution: Parallel analysis (File I/O + Process + Memory in parallel)
          Early termination if verdict clear within 1-2 seconds
```

### ❌ FP from Legitimate Installers
```
Problem: Flagging Windows Setup, antivirus installation
Solution: Publisher certificate verification, known installer whitelist
```

### ❌ Missing Platform-Specific Behaviors
```
Problem: Windows-centric, ignores Linux/macOS threats
Solution: Implement platform-specific monitor for each OS
```

---

## Testing Strategy

### Unit Tests
```cpp
// Test individual metrics
TEST(FileIOMonitor, DetectsRapidFileModification) {
    auto monitor = create_file_monitor();
    simulate_rapid_writes(150, 8_seconds);
    ASSERT_GE(monitor.get_score(), 0.7);
}

TEST(ProcessMonitor, DetectsInjection) {
    auto monitor = create_process_monitor();
    simulate_injection_into_svchost();
    ASSERT_GE(monitor.get_score(), 0.7);
}
```

### Integration Tests
```cpp
// Test full pipeline
TEST(BehavioralAnalyzer, RansomwareDetection) {
    auto analyzer = create_analyzer();
    auto file = load_test_file("ransomware.exe");
    auto verdict = analyzer.analyze(file);

    ASSERT_EQ(verdict.type, MALICIOUS);
    ASSERT_GE(verdict.confidence, 0.80);
    ASSERT_LT(verdict.execution_time, 3_seconds);
}
```

### Regression Tests
```cpp
// Ensure no false positives
TEST(BehavioralAnalyzer, LegitimateInstallerFP) {
    auto analyzer = create_analyzer();
    auto file = load_test_file("7zip_installer.exe");
    auto verdict = analyzer.analyze(file);

    ASSERT_NE(verdict.type, MALICIOUS);
    ASSERT_LT(verdict.behavioral_score, 0.30);
}
```

---

## References

**Full Specification**: `/home/rbsmith4/ladybird/docs/BEHAVIORAL_ANALYSIS_SPEC.md`

**Related Docs**:
- MILESTONE_0.5_PLAN.md - Sandboxing feature overview
- PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md - Network behavior (Phase 6)
- SENTINEL_ARCHITECTURE.md - System architecture

**Code Architecture**:
- `Services/Sentinel/BehavioralAnalyzer.{h,cpp}` - Core component
- `Services/Sentinel/Sandbox.{h,cpp}` - Sandbox environment
- `Services/RequestServer/SecurityTap.{h,cpp}` - Integration point

---

**Last Updated**: 2025-11-01
**Milestone**: 0.5 Phase 1 - Real-time Sandboxing
**Status**: Ready for Implementation

