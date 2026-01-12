# Fuzzing Workflow Scripts

This directory contains production-ready helper scripts for fuzzing Ladybird browser components with AFL++ and LibFuzzer.

## Scripts

### 1. run_fuzzer.sh
Launch fuzzing campaigns with comprehensive configuration options.

**Features:**
- Support for both LibFuzzer and AFL++
- Automatic corpus directory creation
- Seed corpus generation
- Parallel fuzzing with multiple jobs/workers
- Dictionary file support
- Crash detection and organization
- Configurable timeouts and memory limits

**Usage:**
```bash
# Basic LibFuzzer campaign
./run_fuzzer.sh FuzzIPCMessages

# Parallel fuzzing with 8 jobs
./run_fuzzer.sh -j 8 -w 8 FuzzIPCMessages

# AFL++ fuzzing
./run_fuzzer.sh -t afl FuzzIPCMessages

# Custom corpus and dictionary
./run_fuzzer.sh -c my-corpus/ -d ipc.dict FuzzIPCMessages

# Run for specific duration (1 hour)
./run_fuzzer.sh -R 3600 FuzzIPCMessages
```

### 2. triage_crashes.sh
Analyze crash findings with automatic deduplication and prioritization.

**Features:**
- Automatic crash type detection (UAF, buffer overflow, null deref, etc.)
- Stack trace hash-based deduplication
- Severity classification (critical/high/medium/low)
- Parallel analysis with GNU parallel support
- Exploitability indicators
- Crash minimization
- Summary report generation

**Usage:**
```bash
# Analyze crashes from LibFuzzer
./triage_crashes.sh Fuzzing/Corpus/FuzzIPCMessages-crashes/ Build/fuzzers/bin/FuzzIPCMessages

# Analyze and minimize crashes
./triage_crashes.sh -m crashes/ Build/fuzzers/bin/FuzzIPCMessages

# Parallel analysis with 4 jobs
./triage_crashes.sh -p 4 -v crashes/ Build/fuzzers/bin/FuzzIPCMessages

# Analyze AFL++ crashes
./triage_crashes.sh Fuzzing/Corpus/FuzzIPCMessages-afl-findings/default/crashes/ Build/fuzzers/bin/FuzzIPCMessages
```

### 3. minimize_corpus.sh
Optimize corpus size by removing redundant test cases.

**Features:**
- Coverage-guided corpus minimization
- Support for LibFuzzer merge and AFL++ cmin
- Before/after statistics
- File size distribution analysis
- Safe backup of original corpus
- Significant size reduction (typically 50-90%)

**Usage:**
```bash
# Minimize corpus with LibFuzzer
./minimize_corpus.sh Fuzzing/Corpus/FuzzIPCMessages/ Build/fuzzers/bin/FuzzIPCMessages

# Minimize and replace original
./minimize_corpus.sh -d Fuzzing/Corpus/FuzzIPCMessages/ Build/fuzzers/bin/FuzzIPCMessages

# Use AFL++ minimization
./minimize_corpus.sh -m afl Fuzzing/Corpus/queue/ Build/fuzzers/bin/FuzzIPCMessages

# Custom output directory
./minimize_corpus.sh -o minimized/ Fuzzing/Corpus/FuzzIPCMessages/ Build/fuzzers/bin/FuzzIPCMessages
```

### 4. monitor_fuzzing.sh
Real-time monitoring of fuzzing campaigns.

**Features:**
- Auto-detection of fuzzer type
- Live statistics display (exec/s, coverage, crashes)
- AFL++ stats parsing
- Corpus growth tracking for LibFuzzer
- Alert on new crashes (beep or custom command)
- Configurable refresh interval
- One-shot or continuous monitoring

**Usage:**
```bash
# Monitor AFL++ campaign
./monitor_fuzzing.sh Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

# Monitor LibFuzzer corpus
./monitor_fuzzing.sh -t libfuzzer Fuzzing/Corpus/FuzzIPCMessages/

# Display stats once and exit
./monitor_fuzzing.sh -1 Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

# Alert on crashes with custom command
./monitor_fuzzing.sh -c "notify-send 'Crash found!'" Fuzzing/Corpus/FuzzIPCMessages-afl-findings/

# Custom refresh interval
./monitor_fuzzing.sh -r 10 Fuzzing/Corpus/FuzzIPCMessages-afl-findings/
```

### 5. reproduce_crash.sh
Reproduce crashes with debugging support and minimal reproducer generation.

**Features:**
- Crash reproducibility testing
- Detailed stack trace extraction
- Crash type classification
- GDB/LLDB debugger integration
- Automatic crash minimization
- ASAN/UBSAN report parsing
- Unit test template generation
- Reproducibility statistics

**Usage:**
```bash
# Basic reproduction
./reproduce_crash.sh crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

# Reproduce with GDB
./reproduce_crash.sh -d crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

# Reproduce and minimize
./reproduce_crash.sh -m crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

# Test reproducibility (10 runs)
./reproduce_crash.sh -n 10 crash-abc123 Build/fuzzers/bin/FuzzIPCMessages

# Use LLDB instead of GDB
./reproduce_crash.sh -d lldb crash-abc123 Build/fuzzers/bin/FuzzIPCMessages
```

## Typical Workflow

### 1. Initial Setup
```bash
# Build fuzzers
cmake -B Build/fuzzers -DENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build Build/fuzzers

# Create corpus directories
mkdir -p Fuzzing/Corpus/FuzzIPCMessages/
```

### 2. Launch Fuzzing Campaign
```bash
# Start fuzzing with monitoring in separate terminal
./run_fuzzer.sh -j 8 -w 8 FuzzIPCMessages &
./monitor_fuzzing.sh Fuzzing/Corpus/FuzzIPCMessages/
```

### 3. Crash Triage
```bash
# After crashes are found
./triage_crashes.sh -m -p 4 \
    Fuzzing/Corpus/FuzzIPCMessages-crashes/ \
    Build/fuzzers/bin/FuzzIPCMessages

# Review summary
cat crash_reports_*/SUMMARY.txt
```

### 4. Crash Reproduction
```bash
# Reproduce and debug priority crashes
./reproduce_crash.sh -d -m \
    crash_reports_*/unique/critical_crash-* \
    Build/fuzzers/bin/FuzzIPCMessages
```

### 5. Corpus Maintenance
```bash
# Periodically minimize corpus
./minimize_corpus.sh \
    Fuzzing/Corpus/FuzzIPCMessages/ \
    Build/fuzzers/bin/FuzzIPCMessages
```

## Requirements

### Essential
- **CMake 3.25+** - Build system
- **Clang/GCC with sanitizers** - For instrumentation
- **LibFuzzer** - Built into Clang
- **Bash 4.0+** - Shell scripts

### Optional
- **AFL++** - For AFL-based fuzzing (`sudo apt install afl++`)
- **GDB/LLDB** - For crash debugging
- **GNU Parallel** - For parallel crash analysis (`sudo apt install parallel`)
- **llvm-symbolizer** - For stack trace symbolication

## Environment Variables

Scripts respect these environment variables:

- `BUILD_DIR` - Override build directory (default: Build/fuzzers)
- `ASAN_OPTIONS` - AddressSanitizer configuration
- `UBSAN_OPTIONS` - UndefinedBehaviorSanitizer configuration
- `ASAN_SYMBOLIZER_PATH` - Path to llvm-symbolizer

## Integration with Ladybird

These scripts are designed to work with Ladybird's build system:

```bash
# Build with fuzzing enabled
cmake --preset Release
cmake -B Build/fuzzers -DENABLE_FUZZING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build Build/fuzzers

# Expected fuzz targets
# - FuzzIPCMessages
# - FuzzHTMLParser
# - FuzzImageDecoder
# - FuzzCSSParser
# - FuzzURL
# etc.
```

## Error Handling

All scripts implement:
- Input validation
- Graceful error messages
- Non-zero exit codes on failure
- Safe handling of missing dependencies
- Colored output for clarity

## Performance Tips

1. **Parallel Fuzzing**: Use `-j` and `-w` flags to utilize all CPU cores
2. **Corpus Minimization**: Run regularly to keep corpus size manageable
3. **Dictionary Files**: Provide domain-specific dictionaries for better coverage
4. **Memory Limits**: Adjust `-r` flag based on available RAM
5. **Distributed Fuzzing**: Run on multiple machines and merge corpora

## Troubleshooting

### "Binary not found"
Ensure you've built with `-DENABLE_FUZZING=ON` and the binary exists in Build/fuzzers/

### "afl-fuzz not found"
Install AFL++: `sudo apt install afl++`

### "No crashes detected"
This is good! Your code may be robust, or you need:
- Longer fuzzing time
- Better seed corpus
- Domain-specific dictionary

### "Crash doesn't reproduce"
Some crashes are non-deterministic. Try:
- Running multiple times with `-n` flag
- Using ASAN with `detect_invalid_pointer_pairs=2`
- Checking for race conditions

## Contributing

When adding new scripts:
1. Follow existing naming conventions (verb_noun.sh)
2. Include comprehensive help text
3. Add error handling and validation
4. Use colors for output clarity
5. Make executable with `chmod +x`
6. Update this README

## Related Documentation

- **SKILL.md** - Parent fuzzing workflow skill
- **CLAUDE.md** - Ladybird development guide
- **Documentation/Testing.md** - Ladybird testing documentation
- **Milestone 0.4 Technical Specs** - Advanced detection features
