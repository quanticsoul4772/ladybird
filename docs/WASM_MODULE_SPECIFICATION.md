# WASM Malware Analysis Module Specification

**Version**: 0.5.0 (Sentinel Sandboxing - Phase 1a)
**Last Updated**: 2025-11-01
**Status**: Design Specification
**Target**: Milestone 0.5 Phase 1 - Tier 1 WASM Sandbox

---

## Table of Contents

1. [Overview](#overview)
2. [Module Interface Specification](#module-interface-specification)
3. [Detection Functions](#detection-functions)
4. [Data Structures](#data-structures)
5. [Memory Layout](#memory-layout)
6. [Pattern Matching Engine](#pattern-matching-engine)
7. [Scoring Algorithm](#scoring-algorithm)
8. [Implementation Plan](#implementation-plan)
9. [Performance Requirements](#performance-requirements)
10. [Testing Strategy](#testing-strategy)
11. [Integration Guide](#integration-guide)

---

## Overview

### Purpose

The WASM malware analysis module provides **fast, sandboxed file analysis** (Tier 1) for suspicious downloads. It runs inside the Wasmtime sandbox with strict resource limits and timeout enforcement.

### Design Goals

1. **Security**: Complete isolation from host system (no file I/O, no network)
2. **Performance**: <100ms total execution time per file
3. **Accuracy**: Match existing stub heuristics (~70% detection rate)
4. **Portability**: Cross-platform (Linux, macOS, Windows via Wasmtime)
5. **Simplicity**: Minimal host ↔ WASM interface to reduce overhead

### Architecture Context

```
┌──────────────────────────────────────────────────────────┐
│  Sentinel Server (Host Process)                          │
│  ├─ WasmExecutor (Milestone 0.5 Phase 1)                 │
│  │   ├─ Wasmtime Runtime                                 │
│  │   ├─ Load malware_analyzer.wasm module                │
│  │   └─ Execute with 128MB memory limit                  │
│  │                                                        │
│  │   ┌────────────────────────────────────────────────┐  │
│  │   │ WASM Module (malware_analyzer.wasm)            │  │
│  │   │ ┌────────────────────────────────────────────┐ │  │
│  │   │ │ Exported Functions:                        │ │  │
│  │   │ │ • analyze_file(ptr, len) -> result_ptr     │ │  │
│  │   │ │ • get_version() -> u32                     │ │  │
│  │   │ │ • allocate(size) -> ptr                    │ │  │
│  │   │ │ • deallocate(ptr, size)                    │ │  │
│  │   │ └────────────────────────────────────────────┘ │  │
│  │   │ ┌────────────────────────────────────────────┐ │  │
│  │   │ │ Detection Functions:                       │ │  │
│  │   │ │ • calculate_entropy(data) -> f32           │ │  │
│  │   │ │ • match_patterns(data) -> u32              │ │  │
│  │   │ │ • analyze_pe_header(data) -> PEAnalysis    │ │  │
│  │   │ │ • detect_strings(data) -> StringReport     │ │  │
│  │   │ │ • compute_yara_score(data) -> f32          │ │  │
│  │   │ │ • compute_ml_score(features) -> f32        │ │  │
│  │   │ └────────────────────────────────────────────┘ │  │
│  │   │ ┌────────────────────────────────────────────┐ │  │
│  │   │ │ Imported Functions (from host):            │ │  │
│  │   │ │ • log(level: u32, ptr: i32, len: i32)      │ │  │
│  │   │ │ • current_time_ms() -> u64                 │ │  │
│  │   │ └────────────────────────────────────────────┘ │  │
│  │   └────────────────────────────────────────────────┘  │
│  │                                                        │
│  ├─ MalwareML (ML scoring, host-side)                    │
│  ├─ VerdictEngine (combines YARA + ML + WASM results)    │
│  └─ PolicyGraph (verdict storage)                        │
└──────────────────────────────────────────────────────────┘
```

### Relationship to Existing Components

The WASM module **replaces** the stub implementation in `WasmExecutor::execute_stub()` with real sandboxed analysis:

**Before (Stub)**:
- `calculate_yara_heuristic()` - Simple pattern matching
- `calculate_ml_heuristic()` - Basic entropy calculation
- `detect_suspicious_patterns()` - PE header detection

**After (WASM)**:
- Same algorithms, but executed in Wasmtime sandbox
- Complete host isolation (no file I/O, no syscalls)
- Timeout enforcement via fuel limit
- Memory-safe execution

---

## Module Interface Specification

### Exported Functions

The WASM module exports these functions for host invocation:

```wat
;; WebAssembly Text Format (WAT)
(module
  ;; Memory allocation (linear memory, 128MB max)
  (memory (export "memory") 2048)  ;; 2048 pages * 64KB = 128MB

  ;; Version information
  (func (export "get_version") (result i32)
    i32.const 0x00050000  ;; Version 0.5.0
  )

  ;; Allocate memory for file data
  ;; @param size: bytes to allocate
  ;; @return ptr: pointer to allocated memory (0 if OOM)
  (func (export "allocate") (param i32) (result i32)
    ;; Implementation uses WASM linear memory allocator
  )

  ;; Deallocate memory
  ;; @param ptr: pointer to memory
  ;; @param size: size in bytes
  (func (export "deallocate") (param i32 i32)
    ;; Implementation frees linear memory
  )

  ;; Main analysis entry point
  ;; @param file_ptr: pointer to file data in linear memory
  ;; @param file_len: length of file data in bytes
  ;; @return result_ptr: pointer to AnalysisResult structure
  ;;         (0 if analysis failed)
  (func (export "analyze_file") (param i32 i32) (result i32)
    ;; Implementation calls detection functions
  )
)
```

### Imported Functions

The host provides these functions to the WASM module:

```wat
(module
  ;; Logging function (for debugging)
  ;; @param level: 0=debug, 1=info, 2=warn, 3=error
  ;; @param msg_ptr: pointer to UTF-8 message string
  ;; @param msg_len: length of message in bytes
  (import "env" "log" (func $log (param i32 i32 i32)))

  ;; Get current timestamp (milliseconds since epoch)
  ;; @return timestamp: current time in milliseconds
  (import "env" "current_time_ms" (func $current_time_ms (result i64)))
)
```

### C API Wrapper (for Rust/C implementation)

```c
// malware_analyzer.h - WASM module API

#ifndef MALWARE_ANALYZER_H
#define MALWARE_ANALYZER_H

#include <stdint.h>
#include <stdbool.h>

// Version information
#define WASM_MODULE_VERSION_MAJOR 0
#define WASM_MODULE_VERSION_MINOR 5
#define WASM_MODULE_VERSION_PATCH 0

// Result structure (must match WASM memory layout)
typedef struct {
    float yara_score;           // 0.0-1.0
    float ml_score;             // 0.0-1.0
    uint32_t detected_patterns; // Count of matched patterns
    uint64_t execution_time_us; // Execution time in microseconds
    uint32_t error_code;        // 0 = success, non-zero = error
    uint32_t padding;           // Ensure 8-byte alignment
} __attribute__((packed)) AnalysisResult;

// Exported functions (called by host via Wasmtime)
uint32_t get_version(void);
int32_t allocate(uint32_t size);
void deallocate(int32_t ptr, uint32_t size);
int32_t analyze_file(int32_t file_ptr, uint32_t file_len);

// Imported functions (provided by host)
extern void log(uint32_t level, int32_t msg_ptr, uint32_t msg_len);
extern uint64_t current_time_ms(void);

#endif // MALWARE_ANALYZER_H
```

---

## Detection Functions

### 1. Shannon Entropy Calculation

**Purpose**: Measure file randomness/compression (high entropy = packed/encrypted)

**Algorithm**:
```rust
fn calculate_entropy(data: &[u8]) -> f32 {
    if data.is_empty() {
        return 0.0;
    }

    // Count byte frequency
    let mut frequency = [0u32; 256];
    for &byte in data {
        frequency[byte as usize] += 1;
    }

    // Calculate Shannon entropy
    let data_len = data.len() as f32;
    let mut entropy = 0.0f32;

    for &count in &frequency {
        if count > 0 {
            let probability = (count as f32) / data_len;
            entropy -= probability * probability.log2();
        }
    }

    entropy // Range: 0.0 (all same byte) to 8.0 (perfectly random)
}
```

**Performance**: O(n) - single pass over file data
**Target**: <10ms for 1MB file

### 2. Pattern Matching (YARA-like)

**Purpose**: Detect suspicious strings/byte patterns

**Algorithm**:
```rust
// Suspicious pattern database (compiled into WASM)
static PATTERNS: &[&str] = &[
    // Windows APIs (malware indicators)
    "VirtualAlloc",
    "VirtualProtect",
    "WriteProcessMemory",
    "CreateRemoteThread",
    "LoadLibrary",
    "GetProcAddress",

    // Network indicators
    "http://",
    "https://",
    "ftp://",

    // Command execution
    "cmd.exe",
    "powershell",
    "bash",
    "/bin/sh",

    // Ransomware indicators
    "encrypt",
    "decrypt",
    "bitcoin",
    "wallet",

    // Obfuscation
    "eval(",
    "exec(",
    "base64",
];

fn match_patterns(data: &[u8]) -> u32 {
    let mut match_count = 0u32;

    // Convert data to lowercase ASCII for case-insensitive matching
    let mut normalized = Vec::with_capacity(data.len());
    for &byte in data {
        if byte.is_ascii_alphabetic() {
            normalized.push(byte.to_ascii_lowercase());
        } else if byte.is_ascii_alphanumeric() || byte == b'/' || byte == b'.' {
            normalized.push(byte);
        }
    }

    // Boyer-Moore-Horspool substring search for each pattern
    for pattern in PATTERNS {
        if contains_substring(&normalized, pattern.as_bytes()) {
            match_count += 1;
        }
    }

    match_count
}

// Fast substring search (Boyer-Moore-Horspool)
fn contains_substring(haystack: &[u8], needle: &[u8]) -> bool {
    if needle.is_empty() || needle.len() > haystack.len() {
        return false;
    }

    // Build bad character table
    let mut bad_char = [needle.len(); 256];
    for (i, &byte) in needle.iter().enumerate().take(needle.len() - 1) {
        bad_char[byte as usize] = needle.len() - 1 - i;
    }

    // Search
    let mut pos = 0;
    while pos <= haystack.len() - needle.len() {
        let mut i = needle.len() - 1;
        loop {
            if haystack[pos + i] != needle[i] {
                pos += bad_char[haystack[pos + needle.len() - 1] as usize];
                break;
            }
            if i == 0 {
                return true;
            }
            i -= 1;
        }
    }

    false
}
```

**Performance**: O(n*m) worst case, O(n/m) average with BMH optimization
**Target**: <20ms for 1MB file

### 3. PE Header Analysis

**Purpose**: Validate Windows executable structure, detect anomalies

**Algorithm**:
```rust
struct PEAnalysis {
    is_valid_pe: bool,
    anomaly_count: u32,
    section_count: u32,
    entropy_by_section: [f32; 16], // Max 16 sections
}

fn analyze_pe_header(data: &[u8]) -> PEAnalysis {
    let mut result = PEAnalysis {
        is_valid_pe: false,
        anomaly_count: 0,
        section_count: 0,
        entropy_by_section: [0.0; 16],
    };

    // Check minimum size
    if data.len() < 64 {
        return result;
    }

    // Check DOS header "MZ"
    if data[0] != b'M' || data[1] != b'Z' {
        return result;
    }

    // Read PE offset from DOS header (at 0x3C)
    let pe_offset = read_u32_le(data, 0x3C) as usize;

    if pe_offset + 4 > data.len() {
        result.anomaly_count += 10; // Invalid PE offset
        return result;
    }

    // Check PE signature "PE\0\0"
    if &data[pe_offset..pe_offset+4] != b"PE\0\0" {
        result.anomaly_count += 10; // Invalid PE signature
        return result;
    }

    result.is_valid_pe = true;

    // Parse COFF header
    let coff_offset = pe_offset + 4;
    if coff_offset + 20 > data.len() {
        result.anomaly_count += 5;
        return result;
    }

    // Read number of sections
    result.section_count = read_u16_le(data, coff_offset + 2) as u32;

    // Anomaly: too many sections (>10 is unusual)
    if result.section_count > 10 {
        result.anomaly_count += 5;
    }

    // Parse optional header size
    let optional_header_size = read_u16_le(data, coff_offset + 16) as usize;

    // Anomaly: missing optional header
    if optional_header_size == 0 {
        result.anomaly_count += 5;
    }

    // Calculate section table offset
    let section_table_offset = coff_offset + 20 + optional_header_size;

    // Analyze each section
    for i in 0..result.section_count.min(16) {
        let section_offset = section_table_offset + (i as usize * 40);
        if section_offset + 40 > data.len() {
            break;
        }

        // Read section virtual size and raw data offset
        let virtual_size = read_u32_le(data, section_offset + 8);
        let raw_data_ptr = read_u32_le(data, section_offset + 20) as usize;
        let raw_data_size = read_u32_le(data, section_offset + 16) as usize;

        // Anomaly: virtual size != raw size (packing)
        if virtual_size > 0 && (virtual_size as usize) != raw_data_size {
            result.anomaly_count += 2;
        }

        // Calculate entropy for this section
        if raw_data_ptr + raw_data_size <= data.len() {
            let section_data = &data[raw_data_ptr..raw_data_ptr + raw_data_size];
            result.entropy_by_section[i as usize] = calculate_entropy(section_data);

            // Anomaly: high entropy section (>7.5 = likely packed)
            if result.entropy_by_section[i as usize] > 7.5 {
                result.anomaly_count += 3;
            }
        }
    }

    result
}

// Helper: read little-endian u32
fn read_u32_le(data: &[u8], offset: usize) -> u32 {
    u32::from_le_bytes([
        data[offset],
        data[offset + 1],
        data[offset + 2],
        data[offset + 3],
    ])
}

// Helper: read little-endian u16
fn read_u16_le(data: &[u8], offset: usize) -> u16 {
    u16::from_le_bytes([data[offset], data[offset + 1]])
}
```

**Performance**: O(1) - fixed-size header parsing
**Target**: <5ms for any file size

### 4. Suspicious String Detection

**Purpose**: Count URLs, IPs, suspicious APIs

**Algorithm**:
```rust
struct StringReport {
    url_count: u32,
    ip_count: u32,
    api_count: u32,
    total_score: u32,
}

fn detect_strings(data: &[u8]) -> StringReport {
    let mut report = StringReport {
        url_count: 0,
        ip_count: 0,
        api_count: 0,
        total_score: 0,
    };

    // Convert to string view for pattern matching
    let text = String::from_utf8_lossy(data);

    // Count URLs (http://, https://, ftp://)
    report.url_count += count_occurrences(&text, "http://") as u32;
    report.url_count += count_occurrences(&text, "https://") as u32;
    report.url_count += count_occurrences(&text, "ftp://") as u32;

    // Count IP addresses (simplified regex: \d+\.\d+\.\d+\.\d+)
    report.ip_count = count_ip_addresses(&text);

    // Count suspicious Windows API calls
    static API_NAMES: &[&str] = &[
        "VirtualAlloc",
        "CreateRemoteThread",
        "WriteProcessMemory",
        "LoadLibrary",
        "GetProcAddress",
    ];

    for api in API_NAMES {
        if text.contains(api) {
            report.api_count += 1;
        }
    }

    // Scoring
    report.total_score = (report.url_count * 2) +
                         (report.ip_count * 3) +
                         (report.api_count * 5);

    report
}

fn count_occurrences(text: &str, pattern: &str) -> usize {
    text.matches(pattern).count()
}

fn count_ip_addresses(text: &str) -> u32 {
    let mut count = 0u32;
    let bytes = text.as_bytes();

    let mut i = 0;
    while i < bytes.len() {
        if bytes[i].is_ascii_digit() {
            // Simple state machine for xxx.xxx.xxx.xxx
            let mut dots = 0;
            let mut j = i;
            while j < bytes.len() && (bytes[j].is_ascii_digit() || bytes[j] == b'.') {
                if bytes[j] == b'.' {
                    dots += 1;
                }
                j += 1;
            }
            if dots == 3 {
                count += 1;
                i = j;
            }
        }
        i += 1;
    }

    count
}
```

**Performance**: O(n) - single pass
**Target**: <15ms for 1MB file

---

## Data Structures

### AnalysisResult (Main Output)

```rust
// Must be repr(C) for WASM ↔ host ABI compatibility
#[repr(C)]
#[derive(Debug, Clone)]
pub struct AnalysisResult {
    // Scores
    pub yara_score: f32,        // 0.0-1.0 (pattern-based)
    pub ml_score: f32,          // 0.0-1.0 (feature-based)

    // Metadata
    pub detected_patterns: u32, // Number of matched patterns
    pub execution_time_us: u64, // Microseconds

    // Error handling
    pub error_code: u32,        // 0 = success, non-zero = error
    pub padding: u32,           // Ensure 8-byte alignment
}

// Error codes
pub const ERROR_SUCCESS: u32 = 0;
pub const ERROR_OOM: u32 = 1;           // Out of memory
pub const ERROR_INVALID_FILE: u32 = 2; // Invalid file data
pub const ERROR_TIMEOUT: u32 = 3;       // Execution timeout
pub const ERROR_UNKNOWN: u32 = 4;       // Unknown error
```

### MLFeatures (Internal)

```rust
// Features extracted for ML scoring
#[derive(Debug, Clone)]
struct MLFeatures {
    file_size: u64,
    entropy: f32,
    pe_anomalies: u32,
    suspicious_strings: u32,
    code_ratio: f32,
    import_count: u32,
}

impl MLFeatures {
    fn to_normalized_vector(&self) -> [f32; 6] {
        [
            (self.file_size as f32) / 100_000_000.0,  // Normalize by 100MB
            self.entropy / 8.0,                        // Entropy is 0-8
            (self.pe_anomalies as f32) / 100.0,        // Max ~100 anomalies
            (self.suspicious_strings as f32) / 1000.0, // Max ~1000 strings
            self.code_ratio,                           // Already 0-1
            (self.import_count as f32) / 500.0,        // Max ~500 imports
        ]
    }
}
```

### PatternMatch (Internal)

```rust
#[derive(Debug, Clone)]
struct PatternMatch {
    pattern_id: u32,     // Index into PATTERNS array
    offset: u32,         // Byte offset in file
    severity: u8,        // 1=low, 2=medium, 3=high
}
```

---

## Memory Layout

### WASM Linear Memory Organization

```
┌─────────────────────────────────────────────────────────┐
│ WASM Linear Memory (128MB max = 2048 pages × 64KB)     │
├─────────────────────────────────────────────────────────┤
│ 0x00000000 - 0x000000FF : Stack (256 bytes)             │  ← Fixed
│ 0x00000100 - 0x000001FF : Global variables (256 bytes)  │  ← Fixed
│ 0x00000200 - 0x00000FFF : Pattern database (3.5KB)      │  ← Fixed
│ 0x00001000 - 0x00001FFF : ML weights (4KB)              │  ← Fixed
│ 0x00002000 - 0x00002FFF : Working buffers (4KB)         │  ← Fixed
├─────────────────────────────────────────────────────────┤
│ 0x00003000 - 0x07FFFFFF : Heap (dynamic allocation)     │  ← Grows up
│   ├─ File data buffer (up to 10MB)                      │
│   ├─ Frequency tables (256 × 4 bytes)                   │
│   ├─ Pattern match results (variable)                   │
│   └─ AnalysisResult (32 bytes)                          │
└─────────────────────────────────────────────────────────┘
```

### Memory Allocator

Simple bump allocator (no fragmentation):

```rust
static mut HEAP_START: usize = 0x3000;
static mut HEAP_PTR: usize = 0x3000;
static HEAP_END: usize = 0x08000000; // 128MB

#[no_mangle]
pub extern "C" fn allocate(size: u32) -> i32 {
    unsafe {
        let size_aligned = (size as usize + 7) & !7; // 8-byte alignment
        let ptr = HEAP_PTR;
        let new_ptr = ptr + size_aligned;

        if new_ptr > HEAP_END {
            return 0; // OOM
        }

        HEAP_PTR = new_ptr;
        ptr as i32
    }
}

#[no_mangle]
pub extern "C" fn deallocate(ptr: i32, size: u32) {
    // Bump allocator doesn't support individual frees
    // Memory reset on module reload
}

// Reset allocator (called at start of each analysis)
pub fn reset_heap() {
    unsafe {
        HEAP_PTR = HEAP_START;
    }
}
```

---

## Pattern Matching Engine

### Pattern Database Format

Patterns are compiled into the WASM module as a compact binary format:

```rust
// Pattern database (read-only data section)
struct PatternDatabase {
    pattern_count: u32,
    patterns: [Pattern; MAX_PATTERNS],
}

struct Pattern {
    severity: u8,        // 1=low, 2=medium, 3=high
    length: u8,          // Pattern length in bytes
    data: [u8; 64],      // Pattern bytes (max 64 bytes)
}

// Compile-time pattern database
const PATTERN_DB: PatternDatabase = PatternDatabase {
    pattern_count: 24,
    patterns: [
        // High severity (score: 15)
        Pattern { severity: 3, length: 12, data: *b"VirtualAlloc\0\0..." },
        Pattern { severity: 3, length: 15, data: *b"VirtualProtect\0..." },
        Pattern { severity: 3, length: 18, data: *b"WriteProcessMemory..." },
        Pattern { severity: 3, length: 18, data: *b"CreateRemoteThread..." },

        // Medium severity (score: 5)
        Pattern { severity: 2, length: 7, data: *b"http://..." },
        Pattern { severity: 2, length: 8, data: *b"https://..." },
        Pattern { severity: 2, length: 7, data: *b"cmd.exe..." },
        Pattern { severity: 2, length: 10, data: *b"powershell..." },

        // Low severity (score: 2)
        Pattern { severity: 1, length: 5, data: *b"eval(..." },
        Pattern { severity: 1, length: 5, data: *b"exec(..." },
        // ... remaining patterns ...
    ],
};
```

### Pattern Matching Implementation

```rust
fn match_all_patterns(data: &[u8]) -> Vec<PatternMatch> {
    let mut matches = Vec::new();

    // Normalize data for case-insensitive matching
    let normalized = normalize_ascii(data);

    for (pattern_id, pattern) in PATTERN_DB.patterns.iter()
        .take(PATTERN_DB.pattern_count as usize)
        .enumerate()
    {
        let needle = &pattern.data[..pattern.length as usize];

        // Find all occurrences
        let mut offset = 0;
        while let Some(pos) = find_substring(&normalized[offset..], needle) {
            matches.push(PatternMatch {
                pattern_id: pattern_id as u32,
                offset: (offset + pos) as u32,
                severity: pattern.severity,
            });
            offset += pos + needle.len();
        }
    }

    matches
}

fn normalize_ascii(data: &[u8]) -> Vec<u8> {
    data.iter()
        .map(|&b| if b.is_ascii_alphabetic() { b.to_ascii_lowercase() } else { b })
        .collect()
}
```

---

## Scoring Algorithm

### YARA Heuristic Score

```rust
fn compute_yara_score(pattern_matches: &[PatternMatch]) -> f32 {
    let mut score = 0.0f32;

    // Weight by severity
    for m in pattern_matches {
        score += match m.severity {
            3 => 15.0,  // High severity
            2 => 5.0,   // Medium severity
            1 => 2.0,   // Low severity
            _ => 0.0,
        };
    }

    // Normalize to 0.0-1.0 range
    // Threshold: 10 patterns of max severity (10 * 15 = 150)
    (score / 150.0).min(1.0)
}
```

### ML Feature Score

```rust
fn compute_ml_score(features: &MLFeatures) -> f32 {
    // Simplified heuristic ML scoring (matches stub implementation)
    let mut score = 0.0f32;

    // High entropy suggests packing/encryption
    if features.entropy > 7.0 {
        score += 0.3;
    } else if features.entropy > 6.0 {
        score += 0.15;
    }

    // PE anomalies
    if features.pe_anomalies > 20 {
        score += 0.25;
    } else if features.pe_anomalies > 5 {
        score += 0.1;
    }

    // Suspicious strings
    if features.suspicious_strings > 50 {
        score += 0.3;
    } else if features.suspicious_strings > 20 {
        score += 0.15;
    }

    // Code ratio (too high or too low is suspicious)
    if features.code_ratio < 0.2 || features.code_ratio > 0.9 {
        score += 0.15;
    }

    score.min(1.0)
}
```

### Combined Analysis

```rust
#[no_mangle]
pub extern "C" fn analyze_file(file_ptr: i32, file_len: u32) -> i32 {
    let start_time = current_time_ms();

    // Reset heap for each analysis
    reset_heap();

    // Get file data slice
    let file_data = unsafe {
        core::slice::from_raw_parts(file_ptr as *const u8, file_len as usize)
    };

    // Extract features
    let entropy = calculate_entropy(file_data);
    let pattern_matches = match_all_patterns(file_data);
    let pe_analysis = analyze_pe_header(file_data);
    let string_report = detect_strings(file_data);

    // Build ML features
    let features = MLFeatures {
        file_size: file_len as u64,
        entropy,
        pe_anomalies: pe_analysis.anomaly_count,
        suspicious_strings: string_report.total_score,
        code_ratio: 0.5, // Simplified
        import_count: 0,  // Simplified
    };

    // Compute scores
    let yara_score = compute_yara_score(&pattern_matches);
    let ml_score = compute_ml_score(&features);

    // Allocate result
    let result_ptr = allocate(core::mem::size_of::<AnalysisResult>() as u32);
    if result_ptr == 0 {
        log_error("Out of memory allocating result");
        return 0;
    }

    // Write result
    let result = unsafe {
        &mut *(result_ptr as *mut AnalysisResult)
    };

    result.yara_score = yara_score;
    result.ml_score = ml_score;
    result.detected_patterns = pattern_matches.len() as u32;
    result.execution_time_us = (current_time_ms() - start_time) * 1000;
    result.error_code = ERROR_SUCCESS;
    result.padding = 0;

    result_ptr
}
```

---

## Implementation Plan

### Phase 1: Rust Implementation (Week 1-2)

**Technology**: Rust + `wasm32-unknown-unknown` target

**Rationale**:
- Memory safety without GC overhead
- Excellent WASM support via `cargo build --target wasm32-unknown-unknown`
- No runtime dependencies (no_std)
- Fast execution

**Setup**:
```bash
# Install Rust WASM toolchain
rustup target add wasm32-unknown-unknown

# Create project
cargo new --lib malware_analyzer_wasm
cd malware_analyzer_wasm

# Configure for WASM
cat >> Cargo.toml <<EOF
[lib]
crate-type = ["cdylib"]

[profile.release]
opt-level = "z"      # Optimize for size
lto = true           # Link-time optimization
codegen-units = 1    # Better optimization
panic = "abort"      # Smaller binary
strip = true         # Remove debug symbols

[dependencies]
# No external dependencies (pure Rust)
EOF
```

**File Structure**:
```
malware_analyzer_wasm/
├─ src/
│  ├─ lib.rs              # Module exports
│  ├─ entropy.rs          # Shannon entropy
│  ├─ patterns.rs         # Pattern matching
│  ├─ pe_analysis.rs      # PE header parser
│  ├─ strings.rs          # Suspicious string detection
│  ├─ scoring.rs          # YARA + ML scoring
│  ├─ memory.rs           # Linear memory allocator
│  └─ types.rs            # Data structures
├─ tests/
│  └─ integration_test.rs # WASM test harness
├─ benches/
│  └─ perf.rs             # Performance benchmarks
├─ Cargo.toml
└─ build.sh               # Build script
```

**Build Script** (`build.sh`):
```bash
#!/bin/bash
set -e

# Build optimized WASM module
cargo build --target wasm32-unknown-unknown --release

# Copy to Sentinel assets
cp target/wasm32-unknown-unknown/release/malware_analyzer_wasm.wasm \
   ../../Services/Sentinel/assets/malware_analyzer.wasm

# Optimize with wasm-opt (optional)
if command -v wasm-opt &> /dev/null; then
    wasm-opt -Oz -o \
        ../../Services/Sentinel/assets/malware_analyzer.wasm \
        target/wasm32-unknown-unknown/release/malware_analyzer_wasm.wasm
    echo "Optimized with wasm-opt"
fi

# Print module size
ls -lh ../../Services/Sentinel/assets/malware_analyzer.wasm
```

### Phase 2: Wasmtime Integration (Week 2-3)

**Update `WasmExecutor.cpp`**:

```cpp
// Services/Sentinel/Sandbox/WasmExecutor.cpp

#ifdef ENABLE_WASMTIME
#include <wasmtime.h>

ErrorOr<WasmExecutionResult> WasmExecutor::execute_wasmtime(
    ByteBuffer const& file_data,
    String const& filename
) {
    // 1. Create Wasmtime engine and store
    wasm_engine_t* engine = wasm_engine_new();
    wasmtime_store_t* store = wasmtime_store_new(engine, nullptr, nullptr);

    // 2. Configure fuel (timeout mechanism)
    wasmtime_store_add_fuel(store, 10'000'000);  // ~100ms of execution

    // 3. Load WASM module
    auto wasm_path = "/usr/share/ladybird/malware_analyzer.wasm"_string;
    auto module_bytes = TRY(read_file(wasm_path));

    wasmtime_module_t* module = nullptr;
    wasmtime_error_t* error = wasmtime_module_new(
        engine,
        module_bytes.data(),
        module_bytes.size(),
        &module
    );

    if (error) {
        return Error::from_string_literal("Failed to load WASM module");
    }

    // 4. Define imported functions
    wasmtime_func_t log_func;
    wasmtime_func_new(store, &log_type, log_callback, nullptr, nullptr, &log_func);

    wasmtime_func_t time_func;
    wasmtime_func_new(store, &time_type, time_callback, nullptr, nullptr, &time_func);

    wasmtime_extern_t imports[] = {
        { .kind = WASMTIME_EXTERN_FUNC, .of = { .func = log_func } },
        { .kind = WASMTIME_EXTERN_FUNC, .of = { .func = time_func } },
    };

    // 5. Instantiate module
    wasmtime_instance_t instance;
    error = wasmtime_instance_new(store, module, imports, 2, &instance, nullptr);

    if (error) {
        return Error::from_string_literal("Failed to instantiate WASM module");
    }

    // 6. Get exported functions
    wasmtime_func_t allocate_func, analyze_func, deallocate_func;
    TRY(get_export(store, &instance, "allocate"_string, &allocate_func));
    TRY(get_export(store, &instance, "analyze_file"_string, &analyze_func));
    TRY(get_export(store, &instance, "deallocate"_string, &deallocate_func));

    // 7. Get WASM memory
    wasmtime_memory_t memory;
    TRY(get_export(store, &instance, "memory"_string, &memory));

    // 8. Allocate file buffer in WASM memory
    wasmtime_val_t allocate_args[] = {
        { .kind = WASMTIME_I32, .of = { .i32 = (int32_t)file_data.size() } }
    };
    wasmtime_val_t allocate_result;
    error = wasmtime_func_call(store, &allocate_func, allocate_args, 1, &allocate_result, 1, nullptr);

    if (error || allocate_result.of.i32 == 0) {
        return Error::from_string_literal("WASM OOM");
    }

    int32_t file_ptr = allocate_result.of.i32;

    // 9. Copy file data to WASM memory
    uint8_t* wasm_mem = wasmtime_memory_data(store, &memory);
    memcpy(wasm_mem + file_ptr, file_data.data(), file_data.size());

    // 10. Call analyze_file
    wasmtime_val_t analyze_args[] = {
        { .kind = WASMTIME_I32, .of = { .i32 = file_ptr } },
        { .kind = WASMTIME_I32, .of = { .i32 = (int32_t)file_data.size() } },
    };
    wasmtime_val_t analyze_result;
    error = wasmtime_func_call(store, &analyze_func, analyze_args, 2, &analyze_result, 1, nullptr);

    if (error) {
        return Error::from_string_literal("Analysis failed");
    }

    int32_t result_ptr = analyze_result.of.i32;
    if (result_ptr == 0) {
        return Error::from_string_literal("Analysis returned null");
    }

    // 11. Read result from WASM memory
    struct WASMAnalysisResult {
        float yara_score;
        float ml_score;
        uint32_t detected_patterns;
        uint64_t execution_time_us;
        uint32_t error_code;
        uint32_t padding;
    } __attribute__((packed));

    auto* wasm_result = reinterpret_cast<WASMAnalysisResult*>(wasm_mem + result_ptr);

    // 12. Convert to WasmExecutionResult
    WasmExecutionResult result;
    result.yara_score = wasm_result->yara_score;
    result.ml_score = wasm_result->ml_score;
    result.timed_out = false;

    // 13. Cleanup
    wasmtime_val_t dealloc_args[] = {
        { .kind = WASMTIME_I32, .of = { .i32 = file_ptr } },
        { .kind = WASMTIME_I32, .of = { .i32 = (int32_t)file_data.size() } },
    };
    wasmtime_func_call(store, &deallocate_func, dealloc_args, 2, nullptr, 0, nullptr);

    wasmtime_store_delete(store);
    wasm_engine_delete(engine);

    return result;
}
#endif
```

### Phase 3: Testing & Optimization (Week 3-4)

**Unit Tests**:
```rust
// tests/integration_test.rs

#[test]
fn test_entropy_calculation() {
    let uniform_data = vec![0xAAu8; 1024];
    let entropy = calculate_entropy(&uniform_data);
    assert!(entropy < 1.0); // Low entropy

    let random_data: Vec<u8> = (0..1024).map(|_| rand::random()).collect();
    let entropy = calculate_entropy(&random_data);
    assert!(entropy > 7.0); // High entropy
}

#[test]
fn test_pattern_matching() {
    let malicious_data = b"VirtualAllocEx call detected http://evil.com";
    let matches = match_patterns(malicious_data);
    assert!(matches > 0);
}

#[test]
fn test_pe_analysis() {
    let pe_file = include_bytes!("../testdata/calc.exe");
    let analysis = analyze_pe_header(pe_file);
    assert!(analysis.is_valid_pe);
}
```

**Benchmarks**:
```rust
// benches/perf.rs

#[bench]
fn bench_entropy_1mb(b: &mut Bencher) {
    let data = vec![0u8; 1024 * 1024];
    b.iter(|| calculate_entropy(&data));
}

#[bench]
fn bench_pattern_matching_1mb(b: &mut Bencher) {
    let data = vec![0u8; 1024 * 1024];
    b.iter(|| match_patterns(&data));
}

#[bench]
fn bench_full_analysis_1mb(b: &mut Bencher) {
    let data = vec![0u8; 1024 * 1024];
    b.iter(|| analyze_file(data.as_ptr() as i32, data.len() as u32));
}
```

---

## Performance Requirements

### Target Metrics

| Operation | Target | Actual (Measured) |
|-----------|--------|-------------------|
| Entropy calculation (1MB) | <10ms | TBD |
| Pattern matching (1MB) | <20ms | TBD |
| PE header analysis | <5ms | TBD |
| String detection (1MB) | <15ms | TBD |
| **Total analysis (1MB)** | **<100ms** | **TBD** |
| **Total analysis (10MB)** | **<500ms** | **TBD** |

### Memory Budget

| Component | Budget | Notes |
|-----------|--------|-------|
| WASM module code | <100KB | Optimized with -Oz |
| Pattern database | <4KB | 24 patterns × ~170 bytes |
| ML weights (stub) | <4KB | 6→16→2 network |
| Working buffers | <4KB | Frequency tables, etc. |
| File data buffer | <10MB | Maximum file size |
| **Total** | **<10.2MB** | Well under 128MB limit |

### Optimization Strategies

1. **Code Size**:
   - Use `opt-level = "z"` (size optimization)
   - Enable LTO (link-time optimization)
   - Strip debug symbols
   - Inline small functions

2. **Execution Speed**:
   - Boyer-Moore-Horspool for pattern matching (O(n/m) average)
   - Single-pass entropy calculation
   - Early exit on high-confidence matches
   - Avoid allocations in hot paths

3. **Memory Efficiency**:
   - Bump allocator (no fragmentation)
   - Reuse buffers across analyses
   - Fixed-size pattern database
   - No heap allocations for scoring

---

## Testing Strategy

### Test Levels

1. **Unit Tests** (Rust):
   - Individual function correctness
   - Edge cases (empty files, huge files)
   - Pattern matching accuracy

2. **Integration Tests** (C++ + WASM):
   - Wasmtime loading and execution
   - Host ↔ WASM communication
   - Timeout enforcement

3. **Performance Tests** (Benchmarks):
   - Analysis latency by file size
   - Memory usage profiling
   - Fuel consumption measurement

4. **Accuracy Tests** (Real malware):
   - Test against known malware samples
   - Measure false positive/negative rates
   - Compare to stub implementation

### Test Data

```
tests/testdata/
├─ benign/
│  ├─ calc.exe              # Clean Windows calculator
│  ├─ document.pdf          # Clean PDF
│  └─ photo.jpg             # Clean image
├─ malicious/
│  ├─ eicar.com             # EICAR test file
│  ├─ packed_malware.exe    # High entropy
│  └─ ransomware_stub.exe   # Suspicious strings
└─ edge_cases/
   ├─ empty.txt             # Empty file
   ├─ huge_file.bin         # 10MB random data
   └─ truncated.exe         # Corrupted PE
```

### Success Criteria

- ✅ Analysis completes in <100ms for 1MB files
- ✅ No false negatives on EICAR test file
- ✅ <5% false positive rate on benign files
- ✅ Memory usage stays under 128MB limit
- ✅ No WASM crashes or timeouts
- ✅ Matches stub implementation accuracy (±5%)

---

## Integration Guide

### Host Integration (C++)

```cpp
// Services/Sentinel/Sandbox/WasmExecutor.h

class WasmExecutor {
public:
    // ...existing methods...

private:
    // WASM module state
    #ifdef ENABLE_WASMTIME
    wasmtime_engine_t* m_engine { nullptr };
    wasmtime_store_t* m_store { nullptr };
    wasmtime_module_t* m_module { nullptr };
    wasmtime_instance_t m_instance;
    wasmtime_memory_t m_memory;

    // Exported functions
    wasmtime_func_t m_allocate_func;
    wasmtime_func_t m_deallocate_func;
    wasmtime_func_t m_analyze_func;

    // Imported function implementations
    static wasm_trap_t* log_callback(
        void* env,
        wasmtime_caller_t* caller,
        const wasmtime_val_t* args,
        size_t nargs,
        wasmtime_val_t* results,
        size_t nresults
    );

    static wasm_trap_t* time_callback(
        void* env,
        wasmtime_caller_t* caller,
        const wasmtime_val_t* args,
        size_t nargs,
        wasmtime_val_t* results,
        size_t nresults
    );
    #endif
};
```

### CMake Configuration

```cmake
# Services/Sentinel/CMakeLists.txt

# Optional Wasmtime support
option(ENABLE_WASMTIME "Enable Wasmtime WASM sandbox" OFF)

if(ENABLE_WASMTIME)
    find_package(Wasmtime REQUIRED)
    target_compile_definitions(sentinel PRIVATE ENABLE_WASMTIME)
    target_link_libraries(sentinel PRIVATE Wasmtime::wasmtime)

    # Install WASM module
    install(FILES
        ${CMAKE_SOURCE_DIR}/Services/Sentinel/assets/malware_analyzer.wasm
        DESTINATION ${CMAKE_INSTALL_DATADIR}/ladybird/
    )
endif()
```

### Usage Example

```cpp
// Example: Analyze downloaded file

auto executor = TRY(WasmExecutor::create(config));

ByteBuffer file_data = /* ... downloaded file ... */;
String filename = "suspicious.exe"_string;

// Execute WASM analysis
auto result = TRY(executor->execute(file_data, filename));

dbgln("YARA score: {:.2f}", result.yara_score);
dbgln("ML score: {:.2f}", result.ml_score);
dbgln("Detected patterns: {}", result.detected_patterns);
dbgln("Execution time: {}ms", result.execution_time.to_milliseconds());

if (result.yara_score > 0.5f || result.ml_score > 0.5f) {
    // Suspicious - escalate to Tier 2 sandbox
}
```

---

## Appendices

### Appendix A: WASM Module Size Breakdown

```
malware_analyzer.wasm (optimized):
├─ Code section: 45KB
│  ├─ Entropy calculation: 5KB
│  ├─ Pattern matching: 15KB
│  ├─ PE analysis: 12KB
│  ├─ String detection: 8KB
│  └─ Scoring: 5KB
├─ Data section: 8KB
│  ├─ Pattern database: 4KB
│  └─ ML weights: 4KB
└─ Total: ~53KB
```

### Appendix B: Alternative Languages Considered

| Language | Pros | Cons | Verdict |
|----------|------|------|---------|
| **Rust** | Memory safe, fast, great WASM support | Learning curve | ✅ **Selected** |
| AssemblyScript | TypeScript-like, easy | Slower, GC overhead | ❌ Rejected |
| C | Maximum performance | Unsafe, manual memory | ❌ Rejected |
| Go | Easy to write | Large WASM binary (2MB+) | ❌ Rejected |
| Zig | Fast, small binaries | Immature WASM support | ⚠️ Future option |

### Appendix C: Future Enhancements

**Phase 2 (Milestone 0.6)**:
- Add YARA bytecode interpreter (real YARA rules)
- Implement full ML neural network (replace heuristic)
- Add ELF and Mach-O binary analysis
- Support JavaScript deobfuscation

**Phase 3 (Milestone 0.7)**:
- Multi-threaded analysis (Web Workers in WASM)
- Streaming analysis for large files
- Incremental pattern matching
- ML model hot-reloading

---

## References

### WASM Specifications
- WebAssembly Core Specification: https://webassembly.github.io/spec/core/
- Wasmtime Documentation: https://docs.wasmtime.dev/
- WASI (WebAssembly System Interface): https://wasi.dev/

### Related Ladybird Docs
- [SANDBOX_ARCHITECTURE.md](./SANDBOX_ARCHITECTURE.md) - Overall sandbox design
- [MILESTONE_0.5_PLAN.md](./MILESTONE_0.5_PLAN.md) - Phase 1 plan
- [TENSORFLOW_LITE_INTEGRATION.md](./TENSORFLOW_LITE_INTEGRATION.md) - ML detector

### Malware Analysis References
- PE Format Specification: https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Shannon Entropy: https://en.wikipedia.org/wiki/Entropy_(information_theory)
- Boyer-Moore-Horspool Algorithm: https://en.wikipedia.org/wiki/Boyer%E2%80%93Moore%E2%80%93Horspool_algorithm

---

**Document Status**: ✅ Ready for Implementation
**Next Steps**:
1. Create Rust project (`malware_analyzer_wasm`)
2. Implement core detection functions
3. Build and optimize WASM module
4. Integrate with Wasmtime in WasmExecutor
5. Test and benchmark

*Document Version: 1.0*
*Created: 2025-11-01*
*Ladybird Sentinel - WASM Module Specification*
