# Memory Safety Debugging Skill

Comprehensive guide for memory safety debugging in Ladybird using sanitizers and memory profiling tools.

## Quick Start

```bash
# 1. Build with sanitizers
cmake --preset Sanitizers && cmake --build Build/sanitizers

# 2. Run with ASAN
./scripts/run_with_asan.sh

# 3. Analyze errors
./scripts/analyze_sanitizer_report.sh asan_*.log
```

## Skill Contents

### Main Documentation (SKILL.md)

Complete reference covering:
- Sanitizer types (ASAN, UBSAN, MSAN, TSAN)
- Building with sanitizers
- Interpreting sanitizer reports
- Common memory safety patterns in Ladybird
- Valgrind workflow
- Debugging techniques
- Prevention patterns

### Examples Directory

Annotated real-world examples:
- **asan-report-example.txt** - Detailed ASAN output with annotations
- **ubsan-report-example.txt** - UBSAN error types with fixes
- **valgrind-report-example.txt** - Valgrind memcheck output
- **memory-leak-fix-example.cpp** - 10 leak patterns with before/after

### Scripts Directory

Ready-to-use helper scripts:
- **run_with_asan.sh** - Launch Ladybird with ASAN configured
- **run_with_valgrind.sh** - Launch with Valgrind memcheck/callgrind/massif
- **analyze_sanitizer_report.sh** - Parse and summarize sanitizer logs
- **symbolicate_stack.sh** - Symbolicate addresses in stack traces

### References Directory

Deep-dive documentation:
- **sanitizer-options.md** - Complete ASAN/UBSAN/LSAN/TSAN option reference
- **common-errors.md** - Catalog of frequent bugs with detection/prevention
- **debugging-guide.md** - Step-by-step debugging workflow
- **ci-sanitizer-integration.md** - CI/CD sanitizer integration guide

## Usage Scenarios

### Scenario 1: Debug a Crash

```bash
# Run with ASAN to get detailed error
./scripts/run_with_asan.sh -s Ladybird

# Analyze the error
./scripts/analyze_sanitizer_report.sh asan_*.log

# See common-errors.md for fix patterns
```

### Scenario 2: Find Memory Leaks

```bash
# Run with leak detection
ASAN_OPTIONS=detect_leaks=1 ./Build/sanitizers/bin/Ladybird

# Or use Valgrind for more detail
./scripts/run_with_valgrind.sh Ladybird
```

### Scenario 3: Debug Race Condition

```bash
# Build with TSAN
cmake -B Build/tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread"
cmake --build Build/tsan

# Run application
./Build/tsan/bin/Ladybird
```

### Scenario 4: Profile Memory Usage

```bash
# Run with Valgrind massif
./scripts/run_with_valgrind.sh -t massif Ladybird

# Visualize heap growth
ms_print massif.out.* | less
```

### Scenario 5: CI Debugging

```bash
# Reproduce CI failure locally
export ASAN_OPTIONS="halt_on_error=1:detect_leaks=1"
ctest --preset Sanitizers

# See ci-sanitizer-integration.md for more
```

## Key Features

### Comprehensive Coverage

- All 4 major sanitizers (ASAN, UBSAN, MSAN, TSAN)
- Valgrind integration
- Heap profiling
- Performance profiling
- CI/CD integration

### Practical Examples

- Annotated sanitizer reports
- Before/after code examples
- Real-world bug patterns
- Browser-specific issues

### Production-Ready Scripts

- Properly handle edge cases
- Colorized output
- Detailed help messages
- Error checking

### Browser-Specific Guidance

- DOM node lifetime issues
- IPC integer overflows
- Parser buffer overflows
- Reference cycles in DOM
- Async callback safety

## Common Workflows

### Development Workflow

1. Write code
2. Build with sanitizers: `cmake --preset Sanitizers`
3. Run tests: `ctest --preset Sanitizers`
4. Fix any errors
5. Push to CI

### Bug Investigation Workflow

1. Reproduce with sanitizers
2. Analyze stack trace
3. Identify error type (UAF, leak, overflow, etc.)
4. Apply fix pattern from common-errors.md
5. Verify fix
6. Add regression test

### CI Integration Workflow

1. Configure sanitizer preset in CI
2. Run tests with halt_on_error=1
3. Upload logs as artifacts
4. Fix issues before merge

## File Organization

```
memory-safety-debugging/
├── SKILL.md                          # Main documentation
├── README.md                         # This file
├── examples/                         # Real-world examples
│   ├── asan-report-example.txt
│   ├── ubsan-report-example.txt
│   ├── valgrind-report-example.txt
│   └── memory-leak-fix-example.cpp
├── scripts/                          # Helper scripts
│   ├── run_with_asan.sh
│   ├── run_with_valgrind.sh
│   ├── analyze_sanitizer_report.sh
│   └── symbolicate_stack.sh
└── references/                       # Deep-dive docs
    ├── sanitizer-options.md
    ├── common-errors.md
    ├── debugging-guide.md
    └── ci-sanitizer-integration.md
```

## Integration with Other Skills

### Complements fuzzing-workflow

- Fuzzing finds bugs → Sanitizers detect them
- Use both: `./Build/sanitizers/bin/FuzzIPCMessages`
- See fuzzing-workflow skill for fuzz target writing

### Complements ipc-security

- IPC messages are untrusted input
- Integer overflows in IPC are security critical
- See ipc-security skill for validation patterns

### Complements ladybird-cpp-patterns

- Memory safety requires proper C++ patterns
- RefPtr/OwnPtr usage documented in both
- See ladybird-cpp-patterns for RAII, smart pointers

## Tips and Tricks

### Fast Iteration

```bash
# Build only changed files
cmake --build Build/sanitizers --target TestIPC

# Run specific test
./Build/sanitizers/bin/TestIPC --test=specific_test
```

### Better Diagnostics

```bash
# More detailed stack traces
export ASAN_OPTIONS="malloc_context_size=50:fast_unwind_on_malloc=0"

# Track uninitialized values to allocation
valgrind --track-origins=yes ./Build/release/bin/Ladybird
```

### Debugging with GDB

```bash
# Break on sanitizer error
gdb --args ./Build/sanitizers/bin/Ladybird
(gdb) break __asan_report_error
(gdb) run
```

## Resources

- **SKILL.md** - Start here for comprehensive guide
- **debugging-guide.md** - Step-by-step debugging workflow
- **common-errors.md** - Quick reference for error types
- **sanitizer-options.md** - Configuration reference

## Contributing

When adding new content:

1. **Examples** - Include before/after code, explain the bug
2. **Scripts** - Add usage documentation, error handling
3. **References** - Keep organized by topic, use clear headings
4. **Updates** - Keep SKILL.md in sync with other files

## Version History

- **v1.0** (2025-11-01) - Initial creation
  - Complete sanitizer coverage
  - Example reports and fixes
  - Production scripts
  - Reference documentation
  - CI integration guide
