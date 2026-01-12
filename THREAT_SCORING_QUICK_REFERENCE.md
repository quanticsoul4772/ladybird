# Threat Scoring Algorithm - Quick Reference

**File:** `Services/Sentinel/Sandbox/BehavioralAnalyzer.cpp`
**Function:** `calculate_threat_score()` (lines 554-755)
**Status:** ✅ Enhanced with real syscall metrics

---

## Category Weights

| Category | Weight | Old Weight | Rationale |
|----------|--------|------------|-----------|
| **Privilege Escalation** | 40% | 15% | Most critical - system compromise, container escape |
| **Code Injection** | 30% | 10% | Critical - process injection, shellcode execution |
| **Network C2** | 20% | 25% | High - command & control beaconing |
| **File Operations** | 10% | 25% | Medium - ransomware indicators |

---

## Scoring Thresholds

### Privilege Escalation (40% weight)
```
1 attempt   → 0.7  → Final: 0.28 (likely malicious)
2-5 attempts → 0.85 → Final: 0.34 (persistent exploit)
6+ attempts  → 1.0  → Final: 0.40 (active exploit)
```

### Code Injection (30% weight)
```
1 attempt        → 0.6  → Final: 0.18 (suspicious)
2-3 attempts     → 0.8  → Final: 0.24 (likely malicious)
4+ attempts      → 1.0  → Final: 0.30 (active injection)
20+ mem ops      → 0.4  → Final: 0.12 (heap manipulation)
100+ mem ops     → 0.7  → Final: 0.21 (excessive manipulation)
```

### Network C2 (20% weight)
```
Beaconing pattern (ratio > 0.3, ≥3 connections) → 0.5 → Final: 0.10
Aggressive beaconing (>10 connections)           → 0.8 → Final: 0.16
DGA behavior (>10 DNS queries)                   → 0.4 → Final: 0.08
Active DGA (>50 DNS queries)                     → 0.7 → Final: 0.14
```

### File Operations (10% weight)
```
>50 ops/sec        → 0.6 → Final: 0.06 (potential ransomware)
>200 ops/sec       → 0.9 → Final: 0.09 (active ransomware)
>10 process/sec    → 0.5 → Final: 0.05 (fork bomb)
>50 process/sec    → 0.8 → Final: 0.08 (active fork bomb)
≥1 executable drop → 0.4 → Final: 0.04 (dropper)
```

---

## Rate Calculations

```cpp
execution_seconds = max(1.0f, metrics.execution_time.to_milliseconds() / 1000.0f);

// Ransomware detection
file_ops_per_second = metrics.file_operations / execution_seconds;

// Fork bomb detection
process_ops_per_second = metrics.process_operations / execution_seconds;

// C2 beaconing detection
connection_ratio = metrics.outbound_connections / metrics.network_operations;
```

---

## Pattern Detection

### C2 Beaconing
- **Indicator:** High connection-to-operation ratio
- **Threshold:** `ratio > 0.3` AND `outbound_connections ≥ 3`
- **Typical Pattern:** connect() → send() → recv() → close(), repeated
- **Score Impact:** 0.5 baseline, 0.8 if aggressive (>10 connections)

### Domain Generation Algorithm (DGA)
- **Indicator:** Excessive DNS queries
- **Threshold:** `dns_queries > 10` (potential), `> 50` (active)
- **Purpose:** Generate fallback C2 domains
- **Score Impact:** 0.4 potential, 0.7 active

### Ransomware
- **Indicator:** High file operation rate
- **Threshold:** `>50 ops/sec` (potential), `>200 ops/sec` (active)
- **Typical Rate:** 100-1000 files/second encryption
- **Score Impact:** 0.6 potential, 0.9 active

### Fork Bomb
- **Indicator:** Excessive process spawning rate
- **Threshold:** `>10 process/sec` (suspected), `>50 process/sec` (active)
- **Purpose:** Resource exhaustion DoS
- **Score Impact:** 0.5 suspected, 0.8 active

---

## Syscall Categories

### Critical (Immediate Alert)
- **Privilege Escalation:** setuid, capset, mount, unshare, setns, chroot, init_module
- **Code Injection:** ptrace, process_vm_readv, process_vm_writev, mprotect

### High Severity
- **Network Operations:** socket, connect, bind, listen, send, recv
- **File Operations:** open, write, read, unlink, rename, chmod

---

## Score Interpretation

| Range | Level | Action Required |
|-------|-------|-----------------|
| 0.0-0.3 | **Low** | Monitor only |
| 0.3-0.6 | **Medium** | Investigate, log details |
| 0.6-0.8 | **High** | Alert user, block if configured |
| 0.8-1.0 | **Critical** | Block immediately, quarantine |

---

## Example Scores

### Benign Program
```
file_operations: 5
network_operations: 0
process_operations: 1
→ Score: 0.0 (no threats)
```

### Aggressive Ransomware
```
file_operations: 500 (in 2 seconds = 250/sec)
execution_time: 2000ms
→ file_ops_per_second = 250
→ file_score = 0.9 * 0.1 = 0.09
→ Total: 0.09 (would be higher with other indicators)
```

### C2 Beacon
```
network_operations: 20
outbound_connections: 12
→ connection_ratio = 0.6
→ network_score = 0.8 * 0.2 = 0.16
→ Total: 0.16 (beaconing detected)
```

### Privilege Escalation Exploit
```
privilege_escalation_attempts: 3
→ priv_esc_score = 0.85 * 0.4 = 0.34
→ Total: 0.34 (persistent exploit, MEDIUM threat)
```

### Multi-Stage Attack
```
privilege_escalation_attempts: 2    → 0.34
code_injection_attempts: 1          → 0.18
outbound_connections: 8 (beaconing) → 0.10
file_operations: 100 (fast rate)    → 0.03
→ Total: 0.65 (HIGH threat - multiple attack vectors)
```

---

## Testing Checklist

- [ ] Unit test: Single privilege escalation attempt → score ≥ 0.28
- [ ] Unit test: Code injection (3 attempts) → score ≥ 0.24
- [ ] Unit test: Ransomware (250 ops/sec) → score ≥ 0.09
- [ ] Unit test: C2 beaconing (ratio 0.6) → score ≥ 0.16
- [ ] Unit test: Combined attack → score ≥ 0.65
- [ ] Integration test: Real ransomware sample → score > 0.8
- [ ] Integration test: C2 beacon simulator → score > 0.5
- [ ] Integration test: Privilege escalation exploit → score > 0.6
- [ ] Performance test: Scoring latency < 1ms
- [ ] Stress test: 10,000+ syscalls without overflow

---

## Key Improvements

1. ✅ **Syscall-specific weights** - Critical syscalls weighted higher
2. ✅ **Rate-based analysis** - Time-aware detection (ops/sec)
3. ✅ **Pattern detection** - Beaconing, DGA, ransomware, fork bomb
4. ✅ **Binary detection** - ANY critical syscall triggers alert
5. ✅ **Threat-based weights** - 40% priv esc, 30% injection, 20% network, 10% file

---

## Backward Compatibility

✅ **Function signature unchanged**
✅ **Return type unchanged** (ErrorOr<float>)
✅ **All callers work without modification**

---

**Last Updated:** 2025-11-02
**By:** Security Algorithm Specialist
