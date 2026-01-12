# WASM Malware Analysis Module - Quick Reference

**Version**: 0.5.0 | **Status**: Design Complete ✅

## At a Glance

**Purpose**: Fast (<100ms), sandboxed malware analysis for suspicious downloads

**Technology**: Rust → WASM (wasm32-unknown-unknown) → Wasmtime sandbox

**Size**: ~53KB optimized WASM module

**Memory**: 128MB limit, <10MB typical usage

---

## Module Interface

### Exported Functions (Host → WASM)

```c
uint32_t get_version(void);                      // Returns: 0x00050000
int32_t allocate(uint32_t size);                 // Returns: ptr or 0 (OOM)
void deallocate(int32_t ptr, uint32_t size);
int32_t analyze_file(int32_t ptr, uint32_t len); // Returns: result_ptr or 0
```

### Imported Functions (WASM → Host)

```c
void log(uint32_t level, int32_t msg_ptr, uint32_t msg_len);
uint64_t current_time_ms(void);
```

### Result Structure

```c
struct AnalysisResult {
    float yara_score;           // 0.0-1.0 (pattern-based)
    float ml_score;             // 0.0-1.0 (feature-based)
    uint32_t detected_patterns; // Count
    uint64_t execution_time_us; // Microseconds
    uint32_t error_code;        // 0 = success
    uint32_t padding;
} __attribute__((packed));
```

---

## Detection Functions

| Function | Purpose | Performance |
|----------|---------|-------------|
| `calculate_entropy()` | Shannon entropy (0-8) | <10ms/1MB |
| `match_patterns()` | Boyer-Moore substring search | <20ms/1MB |
| `analyze_pe_header()` | PE structure validation | <5ms |
| `detect_strings()` | URLs, IPs, APIs | <15ms/1MB |
| `compute_yara_score()` | Pattern severity weighting | O(1) |
| `compute_ml_score()` | Feature-based heuristic | O(1) |

**Total Target**: <100ms for 1MB file

---

## Pattern Database (24 Patterns)

**High Severity** (score: 15):
- `VirtualAlloc`, `VirtualProtect`, `WriteProcessMemory`, `CreateRemoteThread`

**Medium Severity** (score: 5):
- `http://`, `https://`, `cmd.exe`, `powershell`

**Low Severity** (score: 2):
- `eval(`, `exec(`, `base64`

**Matching**: Case-insensitive, Boyer-Moore-Horspool algorithm

---

## Scoring Algorithm

### YARA Score
```
score = Σ (pattern_severity × match_count)
normalized = min(score / 150.0, 1.0)
```

### ML Score (Heuristic)
```
score = 0
if entropy > 7.0: score += 0.3
if entropy > 6.0: score += 0.15
if pe_anomalies > 20: score += 0.25
if suspicious_strings > 50: score += 0.3
if code_ratio < 0.2 or > 0.9: score += 0.15
return min(score, 1.0)
```

---

## Build Instructions

### Rust Project Setup
```bash
cargo new --lib malware_analyzer_wasm
cd malware_analyzer_wasm

# Configure Cargo.toml
cat >> Cargo.toml <<EOF
[lib]
crate-type = ["cdylib"]

[profile.release]
opt-level = "z"      # Size optimization
lto = true
codegen-units = 1
panic = "abort"
strip = true
