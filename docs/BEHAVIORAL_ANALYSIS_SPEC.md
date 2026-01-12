# Behavioral Analysis Specification for Sandboxed File Execution

**Version**: 1.0
**Date**: 2025-11-01
**Status**: Design Document - Implementation Ready
**Milestone**: 0.5 Phase 1 - Real-time Sandboxing
**Author**: Malware Analysis Specialist

---

## Executive Summary

This specification defines comprehensive behavioral analysis metrics for detecting malicious file behavior in a sandbox environment before user interaction. The system augments existing YARA signature-based detection and ML-based predictions with dynamic behavioral analysis, providing zero-day protection through pattern recognition.

**Goals**:
- Detect malware behavior automatically (ransomware, stealers, miners, worms)
- Minimize false positives while maximizing true positive detection rate
- Complete analysis within 5 seconds per suspicious file
- Cross-platform support (Linux, macOS, Windows)
- Integration with existing Sentinel detection pipeline (YARA + ML)

**Key Principles**:
1. **Behavioral Analysis is Complementary**: Works alongside YARA rules and ML models, not a replacement
2. **Progressive Escalation**: File I/O patterns detected first (fastest), network patterns analyzed in parallel
3. **Graceful Degradation**: If sandbox unavailable, YARA + ML verdict still valid
4. **Privacy-First**: All analysis local, no external data sent beyond PolicyGraph caching

---

## Table of Contents

1. [Overview and Objectives](#overview-and-objectives)
2. [Behavioral Metrics](#behavioral-metrics)
3. [Scoring Algorithm](#scoring-algorithm)
4. [Detection Rules](#detection-rules)
5. [Platform-Specific Implementations](#platform-specific-implementations)
6. [Integration Architecture](#integration-architecture)
7. [Performance Considerations](#performance-considerations)
8. [Testing and Validation](#testing-and-validation)
9. [Appendix: Reference Thresholds](#appendix-reference-thresholds)

---

## Overview and Objectives

### Scope

This specification covers behavioral analysis for:
- **Executable Files**: PE (Windows), ELF (Linux), Mach-O (macOS)
- **Scripts**: PowerShell, Bash, Batch files
- **Documents**: Macros (DOCX, XLSM), PDFs with embedded scripts
- **Archives**: Self-extracting archives, installers

Explicitly **excluded** from this phase:
- Live process injection into existing processes
- Browser-specific behaviors (handled by existing FormMonitor, FingerprintingDetector)
- Network behaviors (handled by Phase 6 C2Detector, DNSAnalyzer)

### Analysis Pipeline

```
File Download
    ↓
[1] YARA Signature Scan (existing)
    ↓ (if benign)
Return: CLEAN
    ↓ (if suspicious/unknown)
[2] ML Prediction (existing MalwareML)
    ↓ (confidence score ≥ 0.3)
[3] BEHAVIORAL SANDBOX EXECUTION (NEW)
    ├─ File I/O Patterns (200ms)
    ├─ Process Creation (100ms)
    ├─ Memory Manipulation (100ms)
    ├─ Registry Operations (Windows, 100ms)
    └─ Network Initialization (100ms)
    ↓
[4] Combined Score Calculation
    ├─ YARA weight: 40% (high confidence)
    ├─ ML weight: 35% (good accuracy)
    └─ Behavioral weight: 25% (zero-day detection)
    ↓
[5] Verdict
    ├─ Benign: score < 0.30
    ├─ Suspicious: 0.30 ≤ score < 0.60
    └─ Malicious: score ≥ 0.60
    ↓
[6] User Notification + PolicyGraph Storage
```

**Execution Time Budget**: 5 seconds total (1s overhead for file setup)
- Sandbox initialization: 500ms
- Behavioral analysis: 3-4 seconds
- Result aggregation & IPC: 500ms

---

## Behavioral Metrics

### 1. File I/O Patterns

Files are the primary target of malware. Behavioral analysis focuses on suspicious modification patterns that legitimate software rarely exhibits.

#### Metric 1.1: Rapid File Modification (Ransomware)

**Name**: `file_modification_rate`
**Detection Focus**: Ransomware, file stealers, disk wipers
**Measurement**: Files modified within time window

```
Pattern: 100+ files modified in <10 seconds
Baseline: Normal installers modify <20 files/10s
         Windows updates modify <50 files/10s
         Antivirus installers modify <30 files/10s
```

**Scoring Formula**:
```
if (modified_files > 100 AND time_window < 10s):
    score += min(0.8, (modified_files / 50) * 0.05)  // 0.8 max
elif (modified_files > 50 AND time_window < 20s):
    score += min(0.4, (modified_files / 100) * 0.02) // 0.4 max
```

**Risk Level**: **HIGH (0.8)**
- Ransomware typically modifies 100+ files in encrypted format
- Behavior: Sequential file access, rapid writes, pattern extensions (.locked, .encrypted, .crypto)

**Whitelist Exceptions**:
- Windows Setup.exe: Known modification of 100+ system files
- macOS updates: Allow up to 500 files in 30 seconds
- Linux package managers: Allow bulk directory operations

#### Metric 1.2: Encryption-Like Operations

**Name**: `entropy_based_file_writes`
**Detection Focus**: Ransomware, stealers using obfuscation
**Measurement**: File entropy before/after, high-entropy writes

```
Pattern: Writing high-entropy data (>7.0/8.0 Shannon entropy)
         Legitimate pattern: System files (entropy 3-6), text (entropy 4-5)
         Suspicious pattern: Random bytes, encrypted blobs (entropy >7.0)
```

**Scoring Formula**:
```
high_entropy_files = count(files where entropy > 7.0)
if (high_entropy_files > 10):
    score += min(0.7, (high_entropy_files / 20) * 0.04)
```

**Risk Level**: **HIGH (0.7)**
- Ransomware encrypts files → writes ciphertext (entropy ~8.0)
- Stealers compress/encrypt stolen data before exfiltration

**False Positive Mitigation**:
- Media files already have high entropy (images, videos)
- Legitimate encrypted database files (.db encrypted)
- Skip files with known media extensions (.jpg, .mp4, .zip)

#### Metric 1.3: Suspicious Directory Targeting

**Name**: `sensitive_directory_access`
**Detection Focus**: Information stealers, credential harvesters
**Measurement**: Access patterns to sensitive locations

```
Suspicious Directories:
  Linux:
    - /root/.ssh           (SSH keys, private keys)
    - /root/.gnupg         (GPG keys)
    - /home/*/.aws         (AWS credentials)
    - /var/www/html        (Website files)

  Windows:
    - C:\Users\*\AppData\Local\Google\Chrome\User Data  (Browser data)
    - C:\Users\*\Documents                              (User documents)
    - C:\ProgramData\Application State                  (App credentials)
    - C:\$Recycle.Bin                                   (Deleted files - wipers)

  macOS:
    - ~/Library/Keychains  (Keychain - password store)
    - ~/Library/Safari     (Safari history, passwords)
    - ~/.ssh               (SSH keys)
```

**Scoring Formula**:
```
for each sensitive_directory in targets:
    file_count = count(files accessed in directory)
    if (file_count > THRESHOLD[directory]):
        score += WEIGHT[directory] * min(1.0, file_count / (2 * THRESHOLD))

Example:
  - SSH key directory accessed (1 file) → +0.4 (high confidence, rare legitimate access)
  - Chrome AppData accessed (50+ files) → +0.5 (common for stealers)
  - Document folder scanned (100+ files in 2s) → +0.3 (file wiper pattern)
```

**Risk Level**: **CRITICAL (0.8-0.9)** for credential directories

#### Metric 1.4: Archive/Self-Extract Operations

**Name**: `archive_extraction_pattern`
**Detection Focus**: Worms, multi-stage downloaders
**Measurement**: Extraction and execution of nested archives

```
Pattern: Extract archive → execute contained executable → repeat
Example: nested.zip → malware.exe → malware_payload.zip → inject.exe
```

**Scoring Formula**:
```
extraction_chain_depth = count(archive.extract() calls)
if (extraction_chain_depth > 2):
    score += min(0.6, extraction_chain_depth * 0.2)
```

**Risk Level**: **MEDIUM-HIGH (0.6)**
- Legitimate installers rarely exceed 2 levels
- Multi-stage malware uses deep chains to evade static analysis

#### Metric 1.5: Unusual File Extensions

**Name**: `suspicious_file_extensions`
**Detection Focus**: Disguised executables, polymorphic malware
**Measurement**: File creation with unusual extensions

```
Highly Suspicious Extensions:
  .exe.txt, .docx.exe, .pdf.exe     (Double extension)
  .scr, .com, .bat, .ps1            (Script formats often abused)
  .vbs, .jse, .wsh                  (Windows Script Host)
  .jar, .apk                        (Bundled payloads - context dependent)

Risk Score:
  Double extension: 0.6
  Script file creation: 0.4
  HTML shortcut creation (.lnk → cmd.exe): 0.7
```

**Scoring Formula**:
```
for each file_created:
    ext = file.extension
    if (ext in HIGHLY_SUSPICIOUS):
        score += RISK[ext]
    elif (ext in SUSPICIOUS and file.has_double_extension()):
        score += 0.6
```

**Risk Level**: **MEDIUM (0.4-0.7)**

---

### 2. Process Behavior

Malware often spawns child processes for privilege escalation, injection, or multi-component attacks.

#### Metric 2.1: Process Creation Chain

**Name**: `process_creation_chain_depth`
**Detection Focus**: Privilege escalation, multi-stage attacks, worms
**Measurement**: Process spawn hierarchy

```
Legitimate Pattern:
  Setup.exe (depth=0)
  └─ MSIExec.exe (depth=1)
     └─ rundll32.exe (depth=2)

Malware Pattern:
  malware.exe (depth=0)
  ├─ powershell.exe (depth=1)       [Downloaded payload]
  │  └─ rundll32.exe (depth=2)      [Inject into legitimate binary]
  │     └─ svchost.exe (depth=3)    [Escalated privileges]
  │        └─ explorer.exe (depth=4) [Remote payload execution]
  └─ cmd.exe (depth=1)              [Lateral movement]
```

**Scoring Formula**:
```
chain_depth = max_depth_in_process_tree
if (chain_depth > 5):
    score += min(0.8, (chain_depth - 5) * 0.1)  // High escalation attempts
elif (chain_depth > 3):
    score += min(0.4, (chain_depth - 3) * 0.15) // Moderate escalation
```

**Risk Level**: **HIGH (0.4-0.8)**
- Legitimate software rarely exceeds depth 3
- Malware uses deep chains to evade monitoring

#### Metric 2.2: Suspicious Process Injection

**Name**: `process_injection_attempts`
**Detection Focus**: Rootkits, in-memory malware, banking trojans
**Measurement**: WriteProcessMemory, CreateRemoteThread calls

```
Injection Techniques:
  1. Classic DLL Injection (WriteProcessMemory → CreateRemoteThread)
  2. APC Queue Injection (QueueUserAPC)
  3. SetWindowsHookEx (window message hooks)
  4. Registry RunOnce injection
  5. Module Override (.local DLL planting)
```

**Scoring Formula**:
```
injection_attempt = detect_writeprocessmemory() OR detect_createremotethread()
if (injection_attempt AND target NOT in whitelist):
    score += 0.7  // Very strong indicator

for each injection_target:
    if (is_system_process(target)):  // svchost, explorer, lsass
        score += 0.3 (additional)     // Privilege escalation attempt
```

**Risk Level**: **CRITICAL (0.7-0.9)**
- Almost never done by legitimate software
- Hallmark of rootkits and advanced malware

**Whitelist Exceptions**:
- Windows Defender launching analysis processes
- Debuggers (WinDbg) attaching to processes
- Legitimate DRM/antitamper solutions (limited cases)

#### Metric 2.3: Suspicious Subprocess Commands

**Name**: `malicious_subprocess_arguments`
**Detection Focus**: Command injection, PowerShell exploits, lateral movement
**Measurement**: Process creation arguments

```
Highly Suspicious Commands:
  cmd.exe /c powershell -enc <BASE64>        [Obfuscated PowerShell]
  powershell -nop -w hidden -c ...           [No-op, no window - stealth]
  wmic process call create "..."             [WMI remote execution]
  psexec -s -i cmd.exe                       [PsExec with SYSTEM]
  schtasks /create /tn "..." /tr "..."       [Scheduled task persistence]
  reg add HKLM\Software\Microsoft\Windows\Run [Registry persistence]
```

**Scoring Formula**:
```
for each subprocess_command:
    if (command in HIGHLY_SUSPICIOUS_COMMANDS):
        score += 0.5
    elif (command.contains("powershell") AND command.contains("encoded")):
        score += 0.4  // Base64 often obfuscates malware
    elif (command.contains("wmic") AND target_not_localhost()):
        score += 0.6  // Lateral movement pattern
```

**Risk Level**: **MEDIUM-HIGH (0.4-0.6)**

---

### 3. Memory Manipulation

Memory manipulation indicates advanced malware trying to evade detection or achieve code execution.

#### Metric 3.1: Heap/Stack Spraying

**Name**: `memory_spray_detection`
**Detection Focus**: Exploit kits, JIT spray attacks, heap overflow
**Measurement**: Repetitive memory allocation patterns

```
Pattern: Allocate same size buffer repeatedly (for heap layout)
Example: Loop allocating 0x20000 byte buffers 1000x = 32MB allocation
Baseline: Normal programs allocate varied sizes
```

**Scoring Formula**:
```
allocation_sizes = collect(all malloc/alloc calls)
mode_size = most_frequent_size(allocation_sizes)
mode_count = count(allocations with mode_size)

if (mode_count > 100 AND mode_size > 0x20000):
    score += min(0.6, (mode_count / 200) * 0.04)
```

**Risk Level**: **MEDIUM-HIGH (0.6)**
- Legitimate: Normal memory allocation (varied sizes)
- Malware: Spray heap with predictable size for exploit

#### Metric 3.2: Executable Memory Writes

**Name**: `executable_memory_writes`
**Detection Focus**: JIT spray, ROP gadgets, code caves
**Measurement**: Writing executable code to memory, then executing

```
Suspicious Pattern:
  1. Allocate RWX (read-write-execute) memory
  2. Write shellcode via memcpy/memmove
  3. Execute via function pointer or ret2shellcode
```

**Scoring Formula**:
```
rwx_allocations = count(VirtualAlloc with PAGE_EXECUTE_READWRITE)
if (rwx_allocations > 1):
    score += min(0.8, rwx_allocations * 0.3)

memcpy_to_executable = detect(memcpy/memmove to RWX region)
if (memcpy_to_executable):
    score += 0.4
```

**Risk Level**: **CRITICAL (0.8)**
- Rare in legitimate software
- Strong indicator of shellcode or exploit

---

### 4. Registry Modifications (Windows)

Registry is key for persistence, credential stealing, and system compromise on Windows.

#### Metric 4.1: Persistence Mechanisms

**Name**: `registry_persistence_patterns`
**Detection Focus**: Rootkits, trojans, spyware
**Measurement**: Registry keys associated with persistence

```
Persistence Locations (Windows):
  Run Keys:
    HKLM\Software\Microsoft\Windows\CurrentVersion\Run
    HKLM\Software\Wow6432Node\Microsoft\Windows\CurrentVersion\Run
    HKCU\Software\Microsoft\Windows\CurrentVersion\Run

  RunOnce (executes once):
    HKLM\Software\Microsoft\Windows\CurrentVersion\RunOnce
    HKCU\Software\Microsoft\Windows\CurrentVersion\RunOnce

  Startup Folder:
    C:\Users\*\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup

  Scheduled Tasks:
    System creates with SYSTEM privileges (indirect via schtasks)

  Winlogon Notification:
    HKLM\System\CurrentControlSet\Control\Winlogon\Notify

  Shell Extensions:
    HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Shell Folders
    HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers
```

**Scoring Formula**:
```
for each persistence_registry_access:
    registry_hive = get_registry_hive(access.path)

    if (registry_hive == "RUN_KEY"):
        score += 0.7  // Very strong indicator
    elif (registry_hive == "RUNONCE_KEY"):
        score += 0.5
    elif (registry_hive == "SCHEDULED_TASK"):
        score += 0.6
```

**Risk Level**: **CRITICAL (0.5-0.7)**
- Legitimate installers use Run keys, but usually sign them
- Signature verification can differentiate installer from malware

#### Metric 4.2: Credential Theft Indicators

**Name**: `credential_registry_access`
**Detection Focus**: Info-stealers, credential harvesters
**Measurement**: Access to password/credential storage locations

```
Credential Locations:
  - HKLM\SAM (Windows user password hashes)
  - HKLM\SYSTEM (DPAPI key material)
  - HKCU\Software\Microsoft\Internet Explorer\IntelliForms\Storage2
    (Saved passwords for web forms)
  - Third-party password managers via file system
```

**Scoring Formula**:
```
if (accesses SAM OR SYSTEM hive):
    score += 0.8  // Credential theft attempt

if (accesses IntelliForms Storage):
    score += 0.5
```

**Risk Level**: **CRITICAL (0.5-0.8)**

#### Metric 4.3: Security Software Disablement

**Name**: `security_bypass_registry_operations`
**Detection Focus**: Antivirus evasion, defense disablement
**Measurement**: Disabling Windows Defender, UAC, firewalls

```
Bypass Targets:
  - HKLM\Software\Policies\Microsoft\Windows Defender
    DisableRealtimeMonitoring, DisableBehaviorMonitoring
  - HKLM\Software\Microsoft\Windows Defender
  - HKCU\Software\Policies\Microsoft\Windows\CurrentVersion\Internet Settings
    (Proxy bypass settings)
```

**Scoring Formula**:
```
if (modifies_defender_policy OR disables_firewall):
    score += 0.9  // Defense evasion is strong malware indicator
```

**Risk Level**: **CRITICAL (0.9)**
- Nearly universal in malware
- Rare in legitimate software

---

### 5. Platform-Specific Patterns

#### 5.1 Linux-Specific Behaviors

**Metric 5.1.1: Privilege Escalation Attempts**
```
Suspicious patterns:
  - setuid/setgid bit manipulation
  - sudo without password
  - /etc/sudoers modification
  - Kernel module loading (insmod, modprobe)
  - Debugger attachment (ptrace to system processes)

Scoring:
  sudo modification: 0.8
  sudoers edit: 0.7
  kernel module load: 0.8
  ptrace system process: 0.6
```

**Metric 5.1.2: Persistence in cron/systemd**
```
Suspicious patterns:
  - Write to /etc/cron.d/
  - Create /etc/systemd/system/ unit files
  - Modify ~/.bashrc or ~/.bash_profile
  - Modify /etc/profile or /etc/profile.d/

Scoring:
  Cron persistence: 0.7
  Systemd unit install: 0.8
  Shell rc modification: 0.5
```

#### 5.2 macOS-Specific Behaviors

**Metric 5.2.1: Launchd Persistence**
```
Suspicious patterns:
  - Create ~/Library/LaunchAgents/*.plist
  - Create /Library/LaunchDaemons/*.plist
  - Add RunAtLoad = true

Scoring:
  LaunchAgent creation: 0.7
  LaunchDaemon creation: 0.8
```

**Metric 5.2.2: Code Signing Bypass**
```
Suspicious patterns:
  - Modifying quarantine attributes (xattr com.apple.quarantine)
  - Removing code signature verification
  - Patching binary to bypass notarization checks

Scoring:
  Remove quarantine: 0.4
  Code signature removal: 0.8
```

---

## Scoring Algorithm

### Overall Architecture

The behavioral scoring system uses **weighted multi-factor analysis** to combine signals from:
1. YARA signatures (40% weight)
2. ML predictions (35% weight)
3. Behavioral analysis (25% weight)

This section defines the behavioral component.

### Behavioral Score Calculation

#### Step 1: Metric Collection Phase

During sandbox execution (3-4 seconds), capture metrics:

```cpp
struct BehavioralMetrics {
    // File I/O
    u32 files_modified { 0 };
    float avg_file_entropy { 0.0f };
    u32 sensitive_dir_accesses { 0 };
    u32 archive_extractions { 0 };
    u32 suspicious_extensions { 0 };

    // Process
    u32 process_chain_depth { 0 };
    bool injection_detected { false };
    u32 suspicious_subprocess_count { 0 };

    // Memory
    u32 heap_spray_allocations { 0 };
    bool executable_memory_writes { false };

    // Registry (Windows)
    u32 persistence_registry_writes { 0 };
    u32 credential_registry_accesses { 0 };
    bool security_bypass_detected { false };

    // Platform-specific
    bool privilege_escalation_attempted { false };
    bool persistence_installed { false };
    bool code_signing_bypassed { false };
};
```

#### Step 2: Individual Metric Scoring

Each metric produces value in [0.0, 1.0]:

```cpp
struct MetricScore {
    float file_io_score { 0.0f };
    float process_score { 0.0f };
    float memory_score { 0.0f };
    float registry_score { 0.0f };
    float platform_score { 0.0f };

    String explanation;  // Human-readable breakdown
};
```

**File I/O Category Scoring**:
```
file_io_score = (
    metric_file_modification_rate() * 0.35 +
    metric_entropy_based_writes() * 0.25 +
    metric_sensitive_directory_access() * 0.25 +
    metric_archive_extraction() * 0.10 +
    metric_suspicious_extensions() * 0.05
)
```

**Process Category Scoring**:
```
process_score = (
    metric_process_chain_depth() * 0.40 +
    metric_injection_attempts() * 0.50 +
    metric_malicious_subprocess_commands() * 0.10
)
```

**Memory Category Scoring**:
```
memory_score = (
    metric_heap_spray() * 0.40 +
    metric_executable_writes() * 0.60
)
```

**Registry Category Scoring (Windows only)**:
```
registry_score = (
    metric_persistence_registry() * 0.35 +
    metric_credential_access() * 0.35 +
    metric_security_bypass() * 0.30
)
```

#### Step 3: Behavioral Score Aggregation

Combine category scores with weights:

```
raw_behavioral_score = (
    file_io_score * 0.40 +
    process_score * 0.30 +
    memory_score * 0.15 +
    registry_score * 0.10 +  // 0.0 on non-Windows
    platform_score * 0.05
)
```

**Behavioral Score Range**: [0.0, 1.0]
- 0.0 = No suspicious behaviors detected
- 1.0 = Extremely suspicious behaviors (likely malware)

#### Step 4: Confidence Adjustment

Adjust score based on statistical confidence:

```
confidence = min(1.0, metric_count / expected_metric_count)
// More metrics → higher confidence in verdict

if (confidence < 0.5):  // Too few metrics collected
    behavioral_score *= 0.5  // Conservative penalty
```

#### Step 5: Combined Multi-Factor Score

Integrate behavioral with existing YARA + ML:

```
final_score = (
    yara_score * 0.40 +
    ml_score * 0.35 +
    behavioral_score * 0.25
)

where:
    yara_score = 1.0 if signature match, 0.0 otherwise
    ml_score = malware_probability from MalwareML (0.0-1.0)
    behavioral_score = result from Step 4 (0.0-1.0)
```

### Verdict Thresholds

Final verdict determination:

```
if (final_score < 0.30):
    verdict = BENIGN
    confidence = min(yara_confidence, 1.0 - final_score)

elif (final_score < 0.60):
    verdict = SUSPICIOUS
    confidence = abs(final_score - 0.45) / 0.15
    recommendation = "Review before opening" or "Allow with caution"

else (final_score >= 0.60):
    verdict = MALICIOUS
    confidence = min(1.0, final_score)
    recommendation = "Block and quarantine"
```

### Confidence Scoring (0.0-1.0)

```
Confidence Calculation:
  - High confidence (>0.90): Multiple strong signals or signature match
  - Medium confidence (0.70-0.90): Behavioral patterns, ML agreement
  - Low confidence (<0.70): Single weak signal or conflicting indicators

Example:
  YARA signature match = 0.95 confidence (strong)
  ML probability 0.8 = 0.80 confidence (good)
  Behavioral patterns only = 0.65 confidence (promising but unconfirmed)
```

### False Positive Minimization

#### Whitelisting Strategies

**Hash-Based Whitelisting**:
- Cache benign file hashes in PolicyGraph
- Skip reanalysis of known-good files
- Reduces redundant sandbox execution

**Signature-Based Whitelisting**:
- Files signed by known publishers (Microsoft, Apple, etc.)
- Verified installer packages with valid certificates
- Reduces false positives on legitimate installers

**Behavioral Whitelisting**:
- Windows installers allowed: RUN key writes (common pattern)
- Antivirus allowed: Process injection into specific targets
- System updaters allowed: Rapid file modifications in system directories

### Example Scoring Scenarios

#### Scenario 1: Detected Ransomware

```
File: suspicious.exe (unknown executable)

Metrics Collected:
  - Files modified: 150 in 8 seconds → 0.75 (ransomware pattern)
  - Entropy: 7.8 (encrypted data) → 0.70 (high entropy)
  - Sensitive dirs: Documents folder accessed → 0.50
  - Process depth: 3 levels → 0.20 (moderate)
  - No injection detected → 0.0
  - Windows: Persistence registry writes → 0.60

Behavioral Score:
  file_io = (0.75 * 0.35 + 0.70 * 0.25 + 0.50 * 0.25 + 0.0 + 0.0) = 0.625
  process = (0.20 * 0.40 + 0.0 * 0.50 + 0.0 * 0.10) = 0.08
  memory = 0.0
  registry = (0.60 * 0.35 + 0.0 * 0.35 + 0.0 * 0.30) = 0.21
  platform = 0.0

  behavioral_score = 0.625 * 0.40 + 0.08 * 0.30 + 0.0 * 0.15 + 0.21 * 0.10 + 0.0 = 0.295

External Scores:
  YARA: Unknown file (no signature) → 0.0
  ML: Probability 0.72 → 0.72

Final Score:
  = 0.0 * 0.40 + 0.72 * 0.35 + 0.295 * 0.25
  = 0.252 + 0.074
  = 0.326 (SUSPICIOUS - score > 0.30)

Confidence: 0.82 (multiple signals agree)

Action: Notify user → "Suspicious behavior detected: file encryption pattern,
rapid file modification. Block recommended."
```

#### Scenario 2: Windows Setup Installer (False Positive Test)

```
File: software_installer.exe (legitimate Windows installer)

Metrics Collected:
  - Files modified: 80 in 15 seconds → 0.30 (installer pattern, not ransomware)
  - Entropy: 5.2 (mixed code/data) → 0.15 (normal for installer)
  - System directory access (allowed) → 0.0
  - Process depth: 2 levels → 0.05 (normal)
  - DLL injection into MSIExec (allowed) → 0.0
  - Registry RUN key write → 0.35 (installer, but monitored)

Behavioral Score:
  file_io = (0.30 * 0.35 + 0.15 * 0.25 + 0.0 * 0.25 + 0.0) = 0.142
  process = (0.05 * 0.40 + 0.0 * 0.50) = 0.02
  memory = 0.0
  registry = (0.35 * 0.35) = 0.122

  behavioral_score = 0.142 * 0.40 + 0.02 * 0.30 + 0.0 + 0.122 * 0.10 = 0.071

External Scores:
  YARA: Signed by Microsoft → 0.0 (benign)
  ML: Probability 0.15 → 0.15

Final Score:
  = 0.0 * 0.40 + 0.15 * 0.35 + 0.071 * 0.25
  = 0.0525 + 0.018
  = 0.071 (BENIGN - score < 0.30)

Action: Allow installation.
```

#### Scenario 3: Information Stealer

```
File: document_viewer.exe (trojanized app)

Metrics Collected:
  - Moderate file access (30 files) → 0.15
  - Normal entropy (5.0) → 0.10
  - Accessed Chrome AppData (password store) → 0.55
  - Archive extraction (2 levels) → 0.40
  - Process depth: 4 levels → 0.35
  - Process injection into svchost → 0.70
  - Memory spray detected → 0.50
  - No registry tampering → 0.0

Behavioral Score:
  file_io = (0.15 * 0.35 + 0.10 * 0.25 + 0.55 * 0.25 + 0.40 * 0.10) = 0.289
  process = (0.35 * 0.40 + 0.70 * 0.50 + 0.0) = 0.490
  memory = 0.50
  registry = 0.0
  platform = 0.0

  behavioral_score = 0.289 * 0.40 + 0.490 * 0.30 + 0.50 * 0.15 = 0.442

External Scores:
  YARA: Unknown → 0.0
  ML: Probability 0.55 → 0.55

Final Score:
  = 0.0 * 0.40 + 0.55 * 0.35 + 0.442 * 0.25
  = 0.1925 + 0.1105
  = 0.303 (SUSPICIOUS)

Action: Block and quarantine.
```

---

## Detection Rules

This section provides explicit if-then logic for common malware families.

### DR-1: Ransomware Detection

```
IF (
  (files_modified > 100 AND time_window < 10s) OR
  (avg_file_entropy > 7.0 AND files_modified > 20) OR
  (modify_many_different_extensions AND process_chain_depth <= 2)
) THEN
  verdict = MALICIOUS
  family = Ransomware
  confidence = min(1.0, number_of_triggered_conditions * 0.35)
```

**Example Families**: WannaCry, Ryuk, LockBit, DarkSide

### DR-2: Information Stealer Detection

```
IF (
  (sensitive_directory_access > 3) OR
  (accesses AND {SSH_KEYS, CHROME_DATA, PASSWORDS}) OR
  (archive_extraction > 1 AND process_injection)
) AND (
  memory_spray OR process_chain_depth >= 3
) THEN
  verdict = SUSPICIOUS or MALICIOUS
  family = Information Stealer
  confidence = 0.70-0.90
```

**Example Families**: Emotet, Trickbot, Zbot, Agent Tesla

### DR-3: Cryptominer Detection

```
IF (
  (process_chain_depth >= 3) AND
  (cpu_intensive_loop_detected) AND
  (NOT(legitimate_app_signature))
) OR (
  (writes_to_autostart_paths > 2) AND
  (spawns_cmd.exe OR powershell.exe)
) THEN
  verdict = SUSPICIOUS
  family = Cryptominer
  confidence = 0.60-0.80
```

**Note**: CPU monitoring requires runtime tracking (may exceed 5s budget)

### DR-4: Rootkit/Bootkit Detection

```
IF (
  (kernel_module_load OR system_driver_install) OR
  (modifies_MBR OR modifies_UEFI) OR
  (modifies_SAM_hive OR disables_windows_defender)
) THEN
  verdict = MALICIOUS
  family = Rootkit
  confidence = 0.95+
```

### DR-5: Worm/Propagation Detection

```
IF (
  (process_creation_high_rate > 10/second) OR
  (copies_self_to_network_paths) OR
  (modifies_system_files_for_execution)
) THEN
  verdict = MALICIOUS
  family = Worm
  confidence = 0.85-0.95
```

### DR-6: Downloader Detection

```
IF (
  (spawns_cmd OR powershell) AND
  (process_arguments_contain_url_pattern) AND
  (archive_extraction > 1)
) OR (
  (network_initialization_early) AND
  (downloads_executable)
) THEN
  verdict = SUSPICIOUS
  family = Downloader
  confidence = 0.65-0.85
```

---

## Platform-Specific Implementations

### Windows Implementation

#### File I/O Monitoring

```cpp
// Hook points via Windows API
- CreateFileA/W → track file opens
- WriteFile/Ex → track writes
- RegOpenKeyEx, RegSetValueEx → registry operations
- GetFileAttributesEx → file enumeration

Data Collected:
  - File path, size, access type
  - Entropy of written data
  - Registry hive and key names
  - Access frequency and timing
```

#### Process Monitoring

```cpp
// Windows API hooks
- CreateProcessA/W → new processes
- CreateRemoteThread → injection detection
- WriteProcessMemory → memory manipulation
- VirtualAlloc → memory allocation patterns
- SetWindowsHookEx → window hook installation

Metrics:
  - Parent-child relationships
  - Command line arguments
  - Target process
  - Memory protection flags
```

#### Registry Monitoring

```cpp
// Registry API hooks
- RegOpenKeyEx → open access
- RegSetValueEx → write operations
- RegQueryValueEx → read operations
- RegDeleteKeyEx → deletion

Persistence Checks:
  - Monitor HKLM\...\Run and RunOnce
  - Monitor HKCU\...\Run
  - Track scheduled task creation
```

### Linux Implementation

#### File I/O Monitoring

```cpp
// System call interception via strace/seccomp
- openat(AT_CLOEXEC) → file opens
- write, writev → writes to files
- statx → file metadata
- unlinkat → file deletion

Data Collected:
  - File descriptor numbers
  - Path names (via /proc/*/fd symlinks)
  - Write patterns
  - Entropy of written data
```

#### Process Monitoring

```cpp
// System call monitoring
- clone/fork/vfork → process creation
- execve → execution
- ptrace → debugger attachment
- prctl → privilege changes

Metrics:
  - Process relationships
  - Command line arguments
  - Privilege escalation attempts
  - Debugger attachment patterns
```

#### Privilege Escalation Detection

```cpp
// Monitor for:
- setuid/setgid bit changes via chmod
- sudo execution without password (SUDO_ASKPASS bypass)
- /etc/sudoers modification
- Kernel module loading (insmod, modprobe)

Detection:
  - Compare file permissions before/after
  - Monitor capability changes via prctl(PR_SET_KEEPCAPS, ...)
  - Detect sudo abuse patterns
```

#### Persistence Detection

```cpp
// Monitor for:
- Writes to /etc/cron.d/*
- Creation of /etc/systemd/system/*.service files
- Modification of ~/.bashrc, ~/.bash_profile
- Modification of /etc/profile.d/*.sh

Scoring:
  - Cron persistence: score += 0.70
  - Systemd unit: score += 0.80
  - Shell rc: score += 0.50
```

### macOS Implementation

#### File I/O Monitoring

```cpp
// FSEvents API + syscall tracing
- open, openat → file opens
- write, writev → writes
- unlink, unlinkat → deletion
- chmod, chown → permission changes

Focus Areas:
  - ~/Library/Keychains (password store)
  - ~/Library/Safari (browser data)
  - ~/.ssh (SSH keys)
  - /etc/passwd, /etc/shadow equivalents
```

#### Code Signing & Notarization Bypass

```cpp
// Monitor for:
- Modification of com.apple.quarantine xattr
- Binary patching to bypass notarization checks
- Removal of code signatures (codesign --remove-signature)
- Execution of unsigned binaries

Scoring:
  - Remove quarantine: score += 0.40
  - Code signature removal: score += 0.80
  - Notarization bypass: score += 0.70
```

#### Launchd Persistence

```cpp
// Monitor for:
- Creation of ~/Library/LaunchAgents/*.plist
- Creation of /Library/LaunchDaemons/*.plist
- Modification of existing plist files
- Addition of RunAtLoad = true or KeepAlive = true

Scoring:
  - LaunchAgent creation: score += 0.70
  - LaunchDaemon creation: score += 0.80
  - Suspicious plist content: additional +0.20
```

---

## Integration Architecture

### Integration with YARA Scanning

**Sequence**:
1. RequestServer receives suspicious download
2. SecurityTap → YARA scan (existing)
3. If no match → ML prediction (existing)
4. If ML confidence < 0.5 or score 0.3-0.6:
   - Sandbox execution + behavioral analysis (NEW)
5. Combine all three signals

**Implementation**:
```cpp
// In SecurityTap::inspect_download()
ErrorOr<SecurityVerdict> result = TRY(yara_scan(file_data));

if (result.verdict == SecurityVerdict::UNKNOWN &&
    ml_detector &&
    ml_prediction.confidence < 0.7) {

    // Behavioral analysis provides additional confidence
    auto behavioral = TRY(sandbox_analyzer->analyze(file_data));
    result.behavioral_score = behavioral.score;
    result.combined_score = combine_scores(
        yara_score,
        ml_prediction.probability,
        behavioral.score
    );
}

return result;
```

### Integration with PolicyGraph

**Storage Schema**:
```sql
-- New table for behavioral analysis cache
CREATE TABLE behavioral_analysis_cache (
    file_hash TEXT PRIMARY KEY,
    sha256 TEXT,
    file_size INTEGER,
    analysis_timestamp DATETIME,

    -- Cached metrics
    behavioral_score REAL,
    file_io_score REAL,
    process_score REAL,
    memory_score REAL,
    registry_score REAL,
    confidence REAL,

    -- Metadata
    user_decision TEXT,  -- allow, block, quarantine
    decision_timestamp DATETIME,
    decision_reason TEXT,

    -- TTL
    expires_at DATETIME
);

-- Query pattern
SELECT * FROM behavioral_analysis_cache
WHERE sha256 = ? AND expires_at > NOW();
```

**Usage**:
- On subsequent download of same file: Return cached verdict
- Cache valid for 24 hours (or until suspicious file reopened)
- User can manually refresh verdict if file updated

### Integration with ML Models

**Confidence Combination**:
```
If YARA signature matches:
    final_confidence = 1.0  // Highest confidence
    behavioral_score = ignored (signature definitive)

Else if ML probability > 0.8:
    final_confidence = 0.85-0.95
    behavioral_score = boost confidence, not change verdict

Else if behavioral patterns strong:
    final_confidence = 0.65-0.85
    behavioral_score = primary signal

Else if conflict between ML and behavior:
    final_confidence = 0.50-0.65
    verdict = SUSPICIOUS (require user input)
```

---

## Performance Considerations

### Execution Time Budget

**Total: 5 seconds per suspicious file**

Breakdown:
```
Sandbox initialization:        500ms
  - Process setup
  - File system mount
  - Network isolation

Behavioral monitoring:       2000-3000ms
  - File I/O tracking        500ms
  - Process monitoring       500ms
  - Memory monitoring        500ms
  - Registry monitoring      300ms (Windows)
  - Network initialization   200ms

Result aggregation:           500ms
  - Score calculation
  - PolicyGraph write
  - IPC to browser

Buffer:                       500ms
  - Unexpected delays
  - GC/cleanup
```

### Optimization Strategies

#### Parallel Analysis

Run independent analyses in parallel:
```
├─ File I/O patterns (200ms)
├─ Process creation (200ms)
├─ Memory allocation (150ms)
└─ Registry ops (150ms) [Windows]
```

Total with parallelism: 500ms (vs 700ms sequential)

#### Early Termination

Stop analysis if verdict clear:
```cpp
if (severity_level >= CRITICAL) {
    // Ransomware detected (rapid file modification)
    return MALICIOUS immediately;  // 200ms total
}

if (no_suspicious_behavior_in_first_500ms) {
    // Likely benign
    return BENIGN immediately;  // 500ms total
}

// Continue detailed analysis for edge cases
```

#### Streaming Metrics

For large files, analyze in chunks:
```cpp
for each file_chunk (1MB):
    - Calculate chunk entropy
    - Update file modification tracking
    - Check for early termination

    if (exceeded_time_budget):
        return early_verdict;
```

### Resource Usage

**Memory**: 50-100 MB per sandbox
- Isolated file system: 20MB
- Process tracking: 10MB
- Metrics cache: 5-10MB
- Buffer: 15-30MB

**CPU**: Single core, short burst
- 100% CPU utilization for 1-3 seconds
- No significant load on main browser process

**Disk**: 500MB-1GB per analysis cache
- Quarantine storage
- Behavioral analysis logs (optional)
- Cache eviction: LRU after 7 days

---

## Testing and Validation

### Test Cases

#### TC-1: Ransomware Detection

```
Test File: Simulated ransomware (non-malicious for testing)
  - 150 files modified in 8 seconds
  - High entropy writes (7.8/8.0)
  - Targeted file extensions (.docx, .xlsx, .pdf)

Expected Result:
  - Behavioral score: 0.60-0.75
  - Final verdict: MALICIOUS (if no FP whitelist)
  - Confidence: >0.80

Pass Criteria:
  - Correct classification
  - Execution time <2 seconds
```

#### TC-2: Legitimate Installer FP Test

```
Test File: Windows installer (e.g., 7-Zip setup)
  - 100+ files modified (normal for installer)
  - Registry Run key modified
  - DLL injection into MSIExec

Expected Result:
  - Behavioral score: 0.15-0.30
  - Final verdict: BENIGN
  - Reason: Known installer signature or whitelisting

Pass Criteria:
  - No false positive
  - Execution time <1 second (whitelisted)
```

#### TC-3: Information Stealer

```
Test File: Trojanized document viewer
  - Accesses Chrome AppData
  - Process injection into svchost
  - Memory spray detected

Expected Result:
  - Behavioral score: 0.40-0.60
  - Final verdict: SUSPICIOUS or MALICIOUS
  - Confidence: 0.70-0.85
```

#### TC-4: Cryptominer

```
Test File: Trojanized utility
  - High CPU usage
  - Spawns multiple child processes
  - Writes to autostart paths

Expected Result:
  - Behavioral score: 0.50-0.70
  - Final verdict: SUSPICIOUS
  - Confidence: 0.70-0.80
```

### Metrics for Validation

```
Detection Accuracy:
  - True Positive Rate (TPR): >90% on known malware
  - True Negative Rate (TNR): >99% on clean samples
  - False Positive Rate (FPR): <1% on legitimate software

Performance:
  - Average analysis time: <2 seconds
  - P95 analysis time: <4 seconds
  - P99 analysis time: <5 seconds

Resource Usage:
  - Memory per sandbox: <100MB
  - CPU: Single core, burst only
  - Disk for cache: <1GB
```

---

## Appendix: Reference Thresholds

### Summary Table

| Metric | Threshold | Risk Score | Notes |
|--------|-----------|-----------|-------|
| **File Modification Rate** | >100 files/10s | 0.75 | Ransomware pattern |
| **High Entropy Writes** | >7.0 Shannon entropy | 0.70 | Encryption pattern |
| **Sensitive Dir Access** | SSH keys accessed | 0.80 | Information stealer |
| **Process Chain Depth** | >5 levels | 0.80 | Privilege escalation |
| **Process Injection** | Any non-whitelisted | 0.70 | Rootkit/banker pattern |
| **Memory Spray** | >100 same-size allocs | 0.60 | Exploit kit pattern |
| **Executable Memory** | Write + execute | 0.80 | Shellcode pattern |
| **Registry Persistence** | Run key modification | 0.70 | Trojan pattern |
| **Credential Access** | SAM hive read | 0.90 | Information stealer |
| **Security Bypass** | Defender disable | 0.95 | Defense evasion |
| **Privilege Escalation** | setuid/setgid mod | 0.85 | Linux rootkit |
| **Persistence Install** | cron/systemd write | 0.75 | Persistence pattern |
| **Code Signing Bypass** | Remove quarantine | 0.70 | evasion on macOS |

### Scoring Weights

```
Final Score Calculation:
  YARA weight: 40%    (signature match = 1.0, else 0.0)
  ML weight: 35%      (malware_probability from model)
  Behavioral weight: 25%  (this specification)

Behavioral Category Weights:
  File I/O: 40%
  Process: 30%
  Memory: 15%
  Registry: 10% (Windows only)
  Platform-specific: 5%

Verdict Thresholds:
  Benign: <0.30
  Suspicious: 0.30-0.60
  Malicious: ≥0.60
```

### Common Whitelist Exceptions

```
Installers:
  - Microsoft Office Setup
  - Windows Updates
  - Visual Studio installer
  - Antivirus installers
  - System utilities (WinRAR, 7-Zip)

System Programs:
  - Windows Defender
  - System Restore
  - Disk Defragmentation
  - Windows Update Service

Legitimate DLL Injection:
  - Debuggers (Visual Studio, WinDbg)
  - Antivirus products
  - Browser plugins (within sandbox context)
```

---

## References and Further Reading

### Related Specifications
- PHASE_6_NETWORK_BEHAVIORAL_ANALYSIS_ARCHITECTURE.md
- SENTINEL_ARCHITECTURE.md
- MILESTONE_0.5_PLAN.md

### External Standards
- MITRE ATT&CK Framework (attack techniques)
- YARA Rule Documentation
- TensorFlow Lite Model Optimization

### Implementation Components
- Services/Sentinel/BehavioralAnalyzer.{h,cpp} (to be created)
- Services/Sentinel/Sandbox.{h,cpp} (to be created)
- Services/RequestServer/TrafficMonitor.{h,cpp} (Phase 6)

---

**Document Version**: 1.0
**Created**: 2025-11-01
**Status**: Design Phase - Ready for Implementation
**Next Steps**: Phase 1 Implementation Sprint (2-3 weeks)

