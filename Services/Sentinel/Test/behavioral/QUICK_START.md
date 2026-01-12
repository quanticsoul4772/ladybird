# BehavioralAnalyzer Test Harness - Quick Start

**Status:** Infrastructure Complete ✅
**Ready for:** Implementation (Weeks 1-3)

---

## Quick Verification

```bash
# From repository root
./Services/Sentinel/verify-test-harness.sh
```

**Expected:** All checks pass ✓

---

## Directory Layout

```
Services/Sentinel/Test/behavioral/
├── README.md              # Dataset documentation
├── QUICK_START.md         # This file
├── benign/
│   ├── hello.sh          # Simple bash (score < 0.1)
│   └── calculator.py     # Python calc (score < 0.2)
└── malicious/
    ├── eicar.txt         # EICAR test (score > 0.9)
    └── ransomware-sim.sh # Ransomware (score > 0.7)
```

---

## Running Tests

### Build
```bash
# Reconfigure (if needed)
cmake --preset Release

# Build tests
cmake --build Build/release --target TestBehavioralAnalyzer

# Build benchmarks
cmake --build Build/release --target BenchmarkBehavioralAnalyzer
```

### Run Unit Tests
```bash
# All BehavioralAnalyzer tests
./Meta/ladybird.py test ".*BehavioralAnalyzer.*"

# Using CTest (verbose)
cd Build/release
ctest -R TestBehavioralAnalyzer -V

# Show failures only
CTEST_OUTPUT_ON_FAILURE=1 ctest -R TestBehavioralAnalyzer
```

### Run Benchmarks
```bash
# Run benchmarks
./Build/release/bin/BenchmarkBehavioralAnalyzer

# Save results
./Build/release/bin/BenchmarkBehavioralAnalyzer > results.txt
```

---

## Test Dataset

### Benign Files
- **hello.sh** - Minimal syscalls, baseline benign
- **calculator.py** - Basic operations, no I/O

### Malicious Files
- **eicar.txt** - YARA validation (industry standard)
- **ransomware-sim.sh** - Behavioral patterns (safe simulation)

**All files safe to execute in sandbox**

---

## Test Categories (30 tests)

### Week 1: Infrastructure (8 tests)
- Analyzer creation
- Temp directory management
- nsjail command generation
- Configuration validation

### Week 2: Analysis (4 tests)
- Benign file detection
- Malicious pattern detection
- YARA integration
- Threat scoring

### Week 3: Syscalls (4 tests)
- Syscall monitoring
- Frequency analysis
- Pattern detection

### Integration (4 tests)
- Sandbox escape prevention
- Resource limits
- Concurrency
- Timeout handling

### Error Handling (4 tests)
- Missing files
- Permission errors
- Invalid file types
- Corrupted files

### Performance (3 tests)
- Small file (< 1 sec)
- Medium file (< 3 sec)
- Large file (< 5 sec)

### YARA (3 tests)
- Rule loading
- Pattern matching
- False positive prevention

---

## Implementation Status

Current: **0/30 tests passing** (infrastructure ready)

### Week 1 Target: 7/30 (23%)
- Core infrastructure
- Temp directory creation
- Command generation

### Week 2 Target: 18/30 (60%)
- File analysis
- YARA integration
- Threat scoring

### Week 3 Target: 30/30 (100%)
- Syscall monitoring
- Full integration
- Performance validation

---

## Performance Targets

| File Size | Target | 95th % | Benchmark |
|-----------|--------|--------|-----------|
| 1 KB | < 1s | < 1.1s | Small |
| 100 KB | < 3s | < 3.3s | Medium |
| 1 MB | < 5s | < 5.5s | Large |

**Overall:** 95th percentile < 5s

---

## Quick Commands

```bash
# Verify infrastructure
./Services/Sentinel/verify-test-harness.sh

# Build everything
cmake --build Build/release --target TestBehavioralAnalyzer BenchmarkBehavioralAnalyzer

# Run all tests
./Meta/ladybird.py test ".*BehavioralAnalyzer.*"

# Run benchmarks
./Build/release/bin/BenchmarkBehavioralAnalyzer

# List test files
ls -lh Services/Sentinel/Test/behavioral/*/*

# Check test count
grep -c "^TEST_CASE" Services/Sentinel/TestBehavioralAnalyzer.cpp
```

---

## Adding New Tests

1. Create test file in `benign/` or `malicious/`
2. Document in `README.md`
3. Add test case in `TestBehavioralAnalyzer.cpp`
4. Update expected score thresholds
5. Run verification script

---

## Debugging

### Test not found
```bash
# Check working directory
cd Services/Sentinel
ctest -R TestBehavioralAnalyzer -V
```

### File paths
```bash
# Verify test files exist
find Services/Sentinel/Test -type f
```

### Build errors
```bash
# Reconfigure from scratch
rm -rf Build/release
cmake --preset Release
cmake --build Build/release
```

---

## Documentation

- **README.md** - Test dataset details
- **TEST_HARNESS_SUMMARY.md** - Comprehensive summary
- **PHASE2_TEST_HARNESS_DELIVERABLES.md** - Full deliverables
- **verify-test-harness.sh** - Verification script

Main docs:
- `docs/BEHAVIORAL_ANALYSIS_SPEC.md`
- `docs/BEHAVIORAL_ANALYSIS_IMPLEMENTATION_GUIDE.md`
- `docs/BEHAVIORAL_ANALYSIS_INDEX.md`

---

## Next Steps

1. ✅ Infrastructure complete
2. → Begin BehavioralAnalyzer.h implementation
3. → Implement temp directory creation (Week 1)
4. → Run first tests
5. → Progressive implementation (Weeks 1-3)

---

**Ready to begin implementation!**
