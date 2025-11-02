# BehavioralAnalyzer Test Infrastructure - Index

**Created:** 2025-11-02
**Status:** Infrastructure Complete ✅
**Location:** `/home/rbsmith4/ladybird/Services/Sentinel/Test/behavioral/`

---

## Quick Navigation

| Document | Purpose | Lines |
|----------|---------|-------|
| [QUICK_START.md](QUICK_START.md) | Quick reference for running tests | 200+ |
| [README.md](README.md) | Test dataset documentation | 65 |
| [TEST_HARNESS_SUMMARY.md](TEST_HARNESS_SUMMARY.md) | Comprehensive summary | 320+ |
| [INDEX.md](INDEX.md) | This file | 100+ |

**Parent Directory:**
- [PHASE2_TEST_HARNESS_DELIVERABLES.md](../PHASE2_TEST_HARNESS_DELIVERABLES.md) - Full deliverables (460+ lines)

---

## Test Files

### Unit Tests
**File:** `/home/rbsmith4/ladybird/Services/Sentinel/TestBehavioralAnalyzer.cpp`
**Size:** 11 KB (331 lines)
**Test Count:** 30 test cases

**Categories:**
- Week 1: Infrastructure (8 tests)
- Week 2: Analysis (4 tests)
- Week 3: Syscalls (4 tests)
- Integration (4 tests)
- Error Handling (4 tests)
- Performance (3 tests)
- YARA (3 tests)

**Build:**
```bash
cmake --build Build/release --target TestBehavioralAnalyzer
```

**Run:**
```bash
ctest -R TestBehavioralAnalyzer -V
```

---

### Benchmarks
**File:** `/home/rbsmith4/ladybird/Services/Sentinel/BenchmarkBehavioralAnalyzer.cpp`
**Size:** 11 KB (286 lines)
**Benchmark Count:** 4 benchmarks

**Benchmarks:**
1. Small file (1 KB) - 10 iterations
2. Medium file (100 KB) - 10 iterations
3. Large file (1 MB) - 5 iterations
4. Real file (hello.sh) - 10 iterations

**Build:**
```bash
cmake --build Build/release --target BenchmarkBehavioralAnalyzer
```

**Run:**
```bash
./Build/release/bin/BenchmarkBehavioralAnalyzer
```

**Performance Target:** 95th percentile < 5000 ms

---

## Test Dataset

### Directory Structure
```
behavioral/
├── benign/
│   ├── hello.sh          (256 B)
│   └── calculator.py     (512 B)
└── malicious/
    ├── eicar.txt         (68 B)
    └── ransomware-sim.sh (2 KB)
```

### Benign Files

#### hello.sh
- **Type:** Bash script
- **Size:** 256 bytes
- **Expected Score:** < 0.1
- **Purpose:** Baseline benign behavior
- **Characteristics:** Minimal syscalls, no file I/O, basic arithmetic

**Run:**
```bash
./Services/Sentinel/Test/behavioral/benign/hello.sh
```

#### calculator.py
- **Type:** Python script
- **Size:** 512 bytes
- **Expected Score:** < 0.2
- **Purpose:** Higher-complexity benign script
- **Characteristics:** Basic operations, no I/O, no network

**Run:**
```bash
./Services/Sentinel/Test/behavioral/benign/calculator.py
```

---

### Malicious Files

#### eicar.txt
- **Type:** EICAR anti-malware test file
- **Size:** 68 bytes
- **Expected Score:** > 0.9
- **Purpose:** YARA rule validation
- **Standard:** Industry-standard safe test file
- **Safety:** ✅ Harmless test signature

**Content:**
```
X5O!P%@AP[4\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*
```

#### ransomware-sim.sh
- **Type:** Bash script (ransomware simulator)
- **Size:** 2 KB
- **Expected Score:** > 0.7
- **Purpose:** Behavioral pattern detection
- **Patterns:**
  - High-frequency file operations
  - File renaming (.encrypted extension)
  - Simulated encryption (base64)
  - Ransom note creation
- **Safety:** ✅ Confined to temp directory, auto-cleanup

**Run (safe):**
```bash
# Runs in temp directory only, auto-cleanup
./Services/Sentinel/Test/behavioral/malicious/ransomware-sim.sh
```

---

## Verification

### Verification Script
**File:** `/home/rbsmith4/ladybird/Services/Sentinel/verify-test-harness.sh`
**Purpose:** Validate test infrastructure completeness

**Run:**
```bash
./Services/Sentinel/verify-test-harness.sh
```

**Checks:**
1. Directory structure
2. Test files
3. Documentation
4. Test dataset
5. CMake integration
6. CTest registration
7. Test file contents
8. Benchmark contents

**Expected Output:**
```
✓ All checks passed!
Test harness is ready for implementation.
```

---

## Build Integration

### CMakeLists.txt
**File:** `/home/rbsmith4/ladybird/Services/Sentinel/CMakeLists.txt`

**Test Configuration:**
```cmake
ladybird_test(TestBehavioralAnalyzer.cpp Sentinel
    LIBS sentinelservice LibCore LibFileSystem)
```

**Benchmark Configuration:**
```cmake
add_executable(BenchmarkBehavioralAnalyzer BenchmarkBehavioralAnalyzer.cpp)
target_link_libraries(BenchmarkBehavioralAnalyzer PRIVATE
    sentinelservice LibCore LibMain LibFileSystem)
```

**CTest Registration:**
- Test ID: #171
- Name: TestBehavioralAnalyzer
- Working Directory: Services/Sentinel/

---

## Implementation Timeline

### Week 1: Core Infrastructure (Days 1-5)
**Focus:** Temp directories, nsjail command generation

**Tests to Implement:**
- `behavioral_analyzer_creation` ✓
- `behavioral_analyzer_creation_with_custom_config` ✓
- `temp_directory_creation` → Week 1 Day 3
- `temp_directory_cleanup_on_error` → Week 1 Day 3
- `nsjail_command_generation_basic` → Week 1 Day 4
- `nsjail_command_generation_with_network_disabled` → Week 1 Day 4
- `nsjail_command_generation_with_resource_limits` → Week 1 Day 4

**Target:** 7/30 tests passing (23%)

---

### Week 2: Behavioral Analysis (Days 6-12)
**Focus:** File analysis, YARA integration, threat scoring

**Tests to Implement:**
- `benign_file_analysis_hello_script` → Week 2
- `benign_file_analysis_calculator` → Week 2
- `malicious_pattern_detection_eicar` → Week 2
- `malicious_pattern_detection_ransomware_sim` → Week 2
- `yara_rule_loading` → Week 2
- `yara_rule_matching` → Week 2
- `yara_rule_no_false_positives` → Week 2
- `resource_limit_enforcement` → Week 2
- `analysis_timeout_handling` → Week 2

**Target:** 18/30 tests passing (60%)

---

### Week 3: Syscall Monitoring (Days 13-15)
**Focus:** Syscall capture, frequency analysis, integration

**Tests to Implement:**
- `syscall_monitoring_file_operations` → Week 3
- `syscall_monitoring_network_operations` → Week 3
- `syscall_monitoring_process_operations` → Week 3
- `syscall_frequency_analysis` → Week 3
- `sandbox_escape_prevention` → Week 3
- `concurrent_analysis` → Week 3
- All remaining error handling tests
- All performance tests

**Target:** 30/30 tests passing (100%)

---

## Performance Targets

| Metric | Target | Measured By |
|--------|--------|-------------|
| Small file (1 KB) | < 1 sec | BenchmarkBehavioralAnalyzer |
| Medium file (100 KB) | < 3 sec | BenchmarkBehavioralAnalyzer |
| Large file (1 MB) | < 5 sec | BenchmarkBehavioralAnalyzer |
| 95th percentile | < 5 sec | BenchmarkBehavioralAnalyzer |

---

## Threat Score Thresholds

| Score Range | Threat Level | Action | Test Files |
|-------------|--------------|--------|------------|
| 0.0 - 0.3 | Low | Allow | hello.sh, calculator.py |
| 0.3 - 0.7 | Medium | Warn | - |
| 0.7 - 1.0 | High | Block | ransomware-sim.sh, eicar.txt |

---

## Common Commands

### Build Everything
```bash
cmake --preset Release
cmake --build Build/release --target TestBehavioralAnalyzer BenchmarkBehavioralAnalyzer
```

### Run All Tests
```bash
./Meta/ladybird.py test ".*BehavioralAnalyzer.*"
```

### Run Specific Test
```bash
cd Build/release
ctest -R TestBehavioralAnalyzer -V
```

### Run Benchmarks
```bash
./Build/release/bin/BenchmarkBehavioralAnalyzer > results.txt
```

### Verify Infrastructure
```bash
./Services/Sentinel/verify-test-harness.sh
```

### List Test Files
```bash
ls -lah Services/Sentinel/Test/behavioral/*/*
```

### Count Tests
```bash
grep -c "^TEST_CASE" Services/Sentinel/TestBehavioralAnalyzer.cpp
# Expected: 29-30
```

---

## Dependencies

### Required
- LibTest (unit testing framework)
- LibCore (core functionality)
- LibFileSystem (file operations)
- sentinelservice (Sentinel library)
- LibMain (entry points for benchmarks)

### Optional
- nsjail (sandboxing - tests skip if missing)
- YARA (malware detection - graceful degradation)

---

## Safety Notes

1. **All test files are safe to execute**
   - Benign files have minimal impact
   - Malicious files are simulations only
   - No real malware or exploits

2. **EICAR is industry standard**
   - Safe test signature
   - Recognized by anti-malware
   - No harmful behavior

3. **Ransomware simulator is confined**
   - Operates only in temp directory
   - Auto-cleanup after execution
   - No system-wide impact

4. **Sandbox isolation**
   - All tests run in isolated environment
   - nsjail prevents escape
   - Resource limits enforced

---

## Documentation References

### Local Documentation
- [README.md](README.md) - Test dataset details
- [QUICK_START.md](QUICK_START.md) - Quick reference
- [TEST_HARNESS_SUMMARY.md](TEST_HARNESS_SUMMARY.md) - Comprehensive summary
- [../PHASE2_TEST_HARNESS_DELIVERABLES.md](../PHASE2_TEST_HARNESS_DELIVERABLES.md) - Full deliverables

### Main Documentation
- `docs/BEHAVIORAL_ANALYSIS_SPEC.md` - Full specification
- `docs/BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md` - Implementation guide
- `docs/BEHAVIORAL_ANALYSIS_INDEX.md` - Documentation index
- `docs/BEHAVIORAL_ANALYSIS_QUICK_REFERENCE.md` - Quick reference

---

## Troubleshooting

### Test executable not found
```bash
# Rebuild test
cmake --build Build/release --target TestBehavioralAnalyzer
```

### Test files not found
```bash
# Check working directory
cd Services/Sentinel
ctest -R TestBehavioralAnalyzer -V
```

### Permission errors
```bash
# Make test files executable
chmod +x Services/Sentinel/Test/behavioral/benign/*.sh
chmod +x Services/Sentinel/Test/behavioral/benign/*.py
chmod +x Services/Sentinel/Test/behavioral/malicious/*.sh
```

### CMake not finding test
```bash
# Reconfigure
cmake --preset Release
```

---

## Next Steps

1. ✅ **Infrastructure Complete**
2. → **Begin Implementation** (Week 1)
   - Create BehavioralAnalyzer.h
   - Implement temp directory creation
   - Update first tests
3. → **Progressive Development** (Weeks 1-3)
   - Remove EXPECT_TODO() as features implemented
   - Verify tests pass
   - Monitor performance benchmarks

---

**Infrastructure Status: READY ✅**

**Last Updated:** 2025-11-02
