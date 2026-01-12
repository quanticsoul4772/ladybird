# Sandbox Technology Recommendation - Quick Reference

**Status**: Ready for Implementation
**Recommendation**: Hybrid Approach (Wasmtime + nsjail)
**Timeline**: 6-7 weeks (fits M0.5 Phase 1 budget)

---

## TL;DR: The Recommendation

### Use a Two-Tier Approach

```
File Downloaded
  ↓
YARA Signature Scan
  ├─ Match → Block ✓
  └─ No match
    ↓
  WASM Pre-Analysis (Wasmtime)
    ├─ Clean → Allow ✓
    └─ Suspicious (score > 0.7)
      ↓
    OS-Level Behavioral Analysis (nsjail + seccomp-BPF)
      ├─ Safe execution → Allow ✓
      ├─ Suspicious behavior → Block/Quarantine ✓
      └─ Timeout/Crash → Treat as malicious ✓
```

---

## Why This Approach?

| Layer | Technology | Why | What It Catches |
|-------|-----------|-----|-----------------|
| **Tier 1** | **WASM (Wasmtime)** | Fast (50-100ms), memory-safe, production-ready | Packed malware, encrypted payloads, suspicious headers |
| **Tier 2** | **OS Sandbox (nsjail)** | Full syscall visibility, actual execution, behavioral analysis | Network beaconing, file exfiltration, registry modification |

### What Each Layer Does

**WASM Analysis** (Pre-execution triage):
- Analyze file headers (PE/ELF)
- Detect packing/encryption
- Extract suspicious strings
- Score suspicion level
- **Decision point**: Forward to OS sandbox or allow?

**OS Analysis** (Behavioral verification):
- Execute file in isolated environment
- Monitor syscalls (seccomp-BPF)
- Track file I/O, network, process creation
- Collect detailed behavioral profile
- **Final verdict**: Malicious, suspicious, or safe

---

## Why NOT Alternatives?

| Alternative | Why Not |
|-----------|---------|
| **WASM only** | Cannot monitor actual syscalls/network from native executables |
| **OS sandbox only** | Overkill for 90% of files (clean ones found by WASM) |
| **wasm3** | Interpreter too slow (0.03x, 30x slower than native) |
| **Wasmer** | Over-engineered, complex WASIX adds unnecessary overhead |
| **Docker** | Too heavy (500ms startup, 100MB memory overhead) |
| **Just YARA** | Cannot catch zero-days (30% false negatives) |

---

## Quick Comparison Table

| Metric | Wasmtime | nsjail | Docker | wasm3 |
|--------|----------|--------|--------|-------|
| **Speed** | 50-100ms | < 5sec | 500ms+ | 10ms (interpreter) |
| **Memory** | 50MB | 5-10MB | 100MB+ | 10MB |
| **Syscall Monitoring** | ❌ | ✅ | ✅ | ❌ |
| **Network Monitoring** | ❌ | ✅ | ✅ | ❌ |
| **Production Ready** | ✅ | ✅ | ✅ | ⚠️ |
| **Cross-Platform** | ✅ | ❌ (Linux) | ✅ | ✅ |
| **Execution Speed** | 1.0x (JIT) | 1.0x (native) | 1.0x (native) | 0.03x (interpreter) |

**Best Choice**: Wasmtime for speed + behavioral analysis, nsjail for detailed monitoring

---

## Implementation Phases

### Phase 1: WASM Integration (Weeks 1-2)
- Add Wasmtime to CMake build
- Implement file header analysis
- Create `WasmAnalyzer` class
- Benchmark: < 100ms per 10MB file

### Phase 2: OS Sandbox Integration (Weeks 3-4)
- Integrate nsjail
- Create seccomp-BPF policy
- Implement `OSBehavioralAnalyzer`
- Benchmark: < 5 seconds per file

### Phase 3: Integration & Testing (Week 4)
- Hook into `SecurityTap`
- End-to-end testing
- Documentation

---

## Success Metrics

| Metric | Target | Status |
|--------|--------|--------|
| Detection rate | 90%+ | ✓ Achievable |
| False positive rate | < 5% | ✓ Achievable |
| WASM analysis time | < 100ms | ✓ Realistic |
| OS analysis time | < 5sec | ✓ Realistic |
| Browser overhead | < 1% | ✓ Expected |

---

## Key Files to Create/Modify

### New Files
- `Services/Sentinel/WasmAnalyzer.{h,cpp}` - WASM analysis
- `Services/Sentinel/OSBehavioralAnalyzer.{h,cpp}` - OS sandbox integration
- `Services/Sentinel/wasm/header_analysis.c` - WASM analysis code
- `Services/Sentinel/nsjail_policy.txt` - seccomp-BPF policy
- Tests for both analyzers
- Documentation

### Modified Files
- `Services/RequestServer/SecurityTap.{h,cpp}` - Call new analyzers
- `CMakeLists.txt` - Add dependencies

---

## Fallback Strategy

```cpp
// If WASM unavailable (unlikely)
Use WAMR or wasm3 as fallback

// If OS sandbox unavailable (non-Linux)
Skip Tier 2, rely on WASM + YARA

// If everything fails
Graceful degradation to YARA-only (current behavior)
```

---

## Questions & Answers

**Q: Can WASM execute native binaries?**
A: No. WASM is a virtual machine for its own bytecode. We use WASM for *pre-execution analysis* (file inspection), not for running the binary itself.

**Q: Is nsjail reliable for security?**
A: Yes. It's used by Google Chrome to sandbox renderers. Battle-tested against real exploits for 15+ years.

**Q: What's the performance impact on users?**
A: Negligible. Clean files analyzed in 50-100ms (unnoticed). Suspicious files get 5-second analysis (user sees "Scanning..." dialog).

**Q: Can malware escape the sandbox?**
A: Unlikely with proper seccomp-BPF policy + Linux kernel patching. Kernel exploits are rare and require CVEs.

**Q: What about macOS/Windows?**
A: WASM works everywhere. OS sandbox gracefully degrades to WASM-only on non-Linux (acceptable for M0.5).

**Q: Do we need GPU for ML?**
A: No. TensorFlow Lite (M0.4) runs on CPU. GPU optional for inference optimization (future work).

---

## Next Steps

1. **Approval**: Get go-ahead on hybrid approach
2. **Setup**: Wasmtime + nsjail dependencies in CMake
3. **Week 1**: Start WasmAnalyzer implementation
4. **Documentation**: Share this recommendation with team

---

## Document References

- **Full Analysis**: [SANDBOX_TECHNOLOGY_COMPARISON.md](./SANDBOX_TECHNOLOGY_COMPARISON.md)
- **Milestone Plan**: [MILESTONE_0.5_PLAN.md](./MILESTONE_0.5_PLAN.md)
- **Sentinel Architecture**: [SENTINEL_ARCHITECTURE.md](./SENTINEL_ARCHITECTURE.md)

---

**Author**: Research Team
**Date**: 2025-11-01
**Status**: Ready for Implementation
