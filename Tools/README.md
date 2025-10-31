# Ladybird Tools

This directory contains utility tools and benchmarks for Ladybird development and testing.

## Performance Benchmarks

### benchmark_sentinel_full

Comprehensive benchmark for the Sentinel security system's download vetting performance.

**Purpose**: Measure the performance impact of Sentinel on download processing.

**Usage**:
```bash
# Full benchmark (requires Sentinel daemon running at /tmp/sentinel.sock)
./benchmark_sentinel_full

# Hash-only baseline (no Sentinel needed)
./benchmark_sentinel_full --skip-sentinel

# Verbose output
./benchmark_sentinel_full --verbose
```

**What it measures**:
- SHA256 hash computation time (baseline)
- Full download path time (Sentinel + YARA scanning)
- Overhead calculation and target compliance
- Performance across multiple file sizes (100KB to 100MB)

**Requirements**:
- Sentinel daemon must be running (unless `--skip-sentinel`)
- YARA rules must be loaded
- Unix socket at `/tmp/sentinel.sock`

**Output**: Detailed performance metrics with pass/fail against < 5% overhead target

**See also**: `docs/SENTINEL_PHASE5_DAY31_PERFORMANCE.md` (Appendix A)

---

### benchmark_policydb

Database performance benchmark for PolicyGraph operations.

**Purpose**: Measure database query and operation performance.

**Usage**:
```bash
# Standard benchmark with 1000 policies, 5000 threats
./benchmark_policydb

# Custom data size
./benchmark_policydb --policies 5000 --threats 10000

# Clean up test database after benchmark
./benchmark_policydb --cleanup

# Full example
./benchmark_policydb --policies 2000 --threats 8000 --cleanup
```

**What it measures**:
- Policy match query time (target: < 5ms)
- Policy creation time (target: < 10ms)
- Threat history query time (target: < 50ms)
- Statistics aggregation time (target: < 100ms)
- Cache hit/miss rates

**Requirements**:
- Write access to `/tmp/sentinel_benchmark_db`
- LibDatabase available
- Sufficient disk space for test data

**Output**: Performance metrics with pass/fail against defined targets

**See also**: `docs/SENTINEL_PHASE5_DAY31_PERFORMANCE.md` (Appendix A)

---

## Building the Benchmarks

### Option 1: Add to Main Build System

Add the contents of `Tools/CMakeLists.txt` to the appropriate CMakeLists.txt file in the project.

### Option 2: Manual Compilation

```bash
# From Ladybird root directory
cd Build

# Build benchmark_sentinel_full
c++ -std=c++23 -O3 \
    -I../Libraries -I../AK -I../Services \
    ../Tools/benchmark_sentinel_full.cpp \
    -lLibCore -lLibCrypto \
    -o bin/benchmark_sentinel_full

# Build benchmark_policydb
c++ -std=c++23 -O3 \
    -I../Libraries -I../AK -I../Services \
    ../Tools/benchmark_policydb.cpp \
    -lLibCore -lLibFileSystem -lLibDatabase \
    -o bin/benchmark_policydb
```

### Option 3: Use CMake Directly

```bash
cd Build
cmake .. -DCMAKE_BUILD_TYPE=Release
make benchmark_sentinel_full
make benchmark_policydb
```

---

## Running the Benchmarks

### Prerequisites

1. **Start Sentinel Daemon** (for benchmark_sentinel_full):
   ```bash
   ./Sentinel &
   # Wait for socket to appear
   ls -l /tmp/sentinel.sock
   ```

2. **Load YARA Rules** (if not auto-loaded):
   ```bash
   # Ensure YARA rules are in correct location
   ls ~/.config/ladybird/sentinel/rules/
   ```

### Execution

```bash
# From Build directory
cd bin

# Run download path benchmark
./benchmark_sentinel_full

# Run database benchmark
./benchmark_policydb --cleanup

# Save results
./benchmark_sentinel_full > results_sentinel.txt 2>&1
./benchmark_policydb > results_db.txt 2>&1
```

### Interpreting Results

**Download Path Benchmark**:
- Look for "Sentinel Overhead" percentage
- Target: < 5% overhead compared to baseline
- If overhead > 5%: Consider YARA rule optimization or IPC improvements

**Database Benchmark**:
- Check "Status" column for PASS/FAIL
- Look for cache hit rate (target: > 80%)
- If tests fail: Consider database tuning or cache optimization

---

## Performance Targets

| Benchmark | Operation | Target | Priority |
|-----------|-----------|--------|----------|
| Sentinel Full | Download Overhead | < 5% | High |
| PolicyDB | Policy Match | < 5ms | High |
| PolicyDB | Policy Creation | < 10ms | Medium |
| PolicyDB | Threat History | < 50ms | Medium |
| PolicyDB | Statistics | < 100ms | Low |
| Cache | Hit Rate | > 80% | High |

---

## Troubleshooting

### benchmark_sentinel_full: "Could not connect to Sentinel daemon"

**Problem**: Sentinel daemon not running or socket not accessible.

**Solutions**:
1. Start Sentinel daemon: `./Sentinel &`
2. Check socket exists: `ls -l /tmp/sentinel.sock`
3. Run baseline only: `./benchmark_sentinel_full --skip-sentinel`

### benchmark_policydb: Permission denied on /tmp/sentinel_benchmark_db

**Problem**: No write access to test database directory.

**Solutions**:
1. Create directory: `mkdir -p /tmp/sentinel_benchmark_db`
2. Check permissions: `chmod 755 /tmp/sentinel_benchmark_db`
3. Use different location (requires code modification)

### Benchmark results show high variance

**Problem**: System load affecting measurements.

**Solutions**:
1. Close other applications
2. Run with higher iteration counts
3. Disable CPU frequency scaling
4. Run on dedicated test machine

---

## Documentation

- **Performance Report**: `docs/SENTINEL_PHASE5_DAY31_PERFORMANCE.md`
- **Completion Summary**: `docs/SENTINEL_PHASE5_DAY31_COMPLETION.md`
- **Phase 5 Plan**: `docs/SENTINEL_PHASE5_PLAN.md`

---

## Contributing

When adding new benchmarks:

1. Follow existing patterns (setup, warmup, measure, report)
2. Define clear performance targets
3. Support configurable iteration counts
4. Include statistical measurements (min, max, avg)
5. Provide pass/fail criteria
6. Document usage in this README
7. Update performance report documentation

---

## License

All tools are licensed under BSD-2-Clause, consistent with the Ladybird project.

---

**Last Updated**: 2025-10-30
**Maintainer**: Development Team
