# CI Sanitizer Integration

Guide for integrating sanitizers into Continuous Integration workflows.

## Overview

Ladybird CI uses sanitizers to catch memory safety issues before merge. This document explains:
- How CI runs sanitizers
- Interpreting CI failures
- Adding sanitizer checks to new code
- Suppressing false positives

## Current CI Setup

### Sanitizer Preset

Ladybird's `Sanitizers` CMake preset enables:
- **AddressSanitizer (ASAN)** - Memory errors
- **UndefinedBehaviorSanitizer (UBSAN)** - Undefined behavior
- **LeakSanitizer (LSAN)** - Memory leaks (part of ASAN)

Build configuration:
```cmake
# From CMakePresets.json
{
  "name": "Sanitizers",
  "binaryDir": "Build/sanitizers",
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "RelWithDebInfo",
    "CMAKE_CXX_FLAGS": "-fsanitize=address,undefined -fno-omit-frame-pointer -g",
    "CMAKE_EXE_LINKER_FLAGS": "-fsanitize=address,undefined"
  }
}
```

### Test Execution

CI runs tests with:
```bash
# Configure
cmake --preset Sanitizers

# Build
cmake --build Build/sanitizers

# Run tests
ctest --preset Sanitizers
```

## CI Workflow Example

```yaml
# .github/workflows/sanitizers.yml
name: Memory Safety (Sanitizers)

on:
  pull_request:
    branches: [master]
  push:
    branches: [master]

jobs:
  sanitizers:
    name: ASAN + UBSAN Tests
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            cmake ninja-build \
            clang-18 llvm-18 \
            libfontconfig1-dev libgl1-mesa-dev

      - name: Configure with Sanitizers
        run: cmake --preset Sanitizers

      - name: Build
        run: cmake --build Build/sanitizers -j$(nproc)

      - name: Run tests with sanitizers
        env:
          ASAN_OPTIONS: |
            detect_leaks=1:
            strict_string_checks=1:
            print_stacktrace=1:
            halt_on_error=1:
            suppressions=${{ github.workspace }}/asan_suppressions.txt
          UBSAN_OPTIONS: |
            print_stacktrace=1:
            halt_on_error=1
          LSAN_OPTIONS: |
            suppressions=${{ github.workspace }}/lsan_suppressions.txt
        run: ctest --preset Sanitizers --output-on-failure

      - name: Upload sanitizer logs
        if: failure()
        uses: actions/upload-artifact@v3
        with:
          name: sanitizer-logs
          path: |
            Build/sanitizers/Testing/Temporary/LastTest.log
            asan_*.log
            ubsan_*.log
```

## Handling CI Failures

### 1. Sanitizer Error in CI

**CI Output:**
```
Test #42: TestIPC ...........................***Failed    0.12 sec
=================================================================
==98765==ERROR: AddressSanitizer: heap-use-after-free on address 0x60700000eff8
    #0 IPC::Decoder::decode<int>() LibIPC/Decoder.cpp:45
    ...
```

**Steps:**
1. **Download logs** - Click "Download artifact" on CI page
2. **Reproduce locally:**
   ```bash
   cmake --preset Sanitizers
   cmake --build Build/sanitizers
   ASAN_OPTIONS=halt_on_error=1 ./Build/sanitizers/bin/TestIPC
   ```
3. **Fix the bug** - See debugging-guide.md
4. **Verify fix:**
   ```bash
   ./Build/sanitizers/bin/TestIPC  # Should pass
   ctest --preset Sanitizers        # All tests pass
   ```
5. **Push fix** - CI will re-run

### 2. Memory Leak in CI

**CI Output:**
```
=================================================================
==98765==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 128 bytes in 2 object(s) allocated from:
    #0 operator new
    #1 Web::ResourceLoader::create() LibWeb/Loader/ResourceLoader.cpp:45
    ...
```

**Steps:**
1. **Identify leak source** - Stack trace shows allocation site
2. **Check if real leak or false positive:**
   - Real leak: Object allocated but never deleted
   - False positive: Object intentionally kept alive (cache, singleton)
3. **Fix or suppress:**
   - If real: Use `OwnPtr`/`RefPtr`
   - If false positive: Add to suppression file

### 3. Timeout in CI

**CI Output:**
```
Test #42: TestIPC ...........................***Timeout  120.00 sec
```

**Possible causes:**
- Deadlock (use TSAN)
- Infinite loop
- Slow sanitizer overhead

**Steps:**
1. **Check if test is slow:**
   ```bash
   time ./Build/sanitizers/bin/TestIPC
   ```
2. **Compare with Release:**
   ```bash
   time ./Build/release/bin/TestIPC
   ```
3. **If much slower:** Consider splitting test or increasing timeout

## Suppression Files

### When to Suppress

**DO suppress:**
- Third-party library issues (Qt, OpenGL, fontconfig)
- Known issues with workarounds
- Platform-specific issues (WSL, specific Linux distro)

**DON'T suppress:**
- Ladybird code issues (fix instead)
- IPC security issues (always fix)
- Parser vulnerabilities (always fix)

### ASAN Suppressions

Create `asan_suppressions.txt` in repo root:

```
# Qt framework leaks (not our code)
leak:QApplication::exec
leak:QEventLoop::exec

# OpenGL driver leaks
leak:libGL.so
leak:libEGL.so

# Fontconfig cache (intentional global)
leak:FcConfigSubstitute
leak:FcPatternCreate

# X11 display (global resource)
leak:XOpenDisplay
```

Usage:
```bash
export ASAN_OPTIONS="suppressions=$PWD/asan_suppressions.txt"
./Build/sanitizers/bin/Ladybird
```

### UBSAN Suppressions

Create `ubsan_suppressions.txt`:

```
# Qt hash function (unsigned overflow is intentional)
unsigned-integer-overflow:qhash.cpp

# Third-party library
signed-integer-overflow:libfontconfig.so
```

### LSAN Suppressions

Create `lsan_suppressions.txt`:

```
# Qt global resources
leak:QCoreApplication::
leak:QGuiApplication::

# Thread-local storage
leak:__tls_get_addr
```

### Suppression Format

```
# Comment explaining why suppressed
<error_type>:<pattern>
```

**Error types:**
- `leak` - Memory leak (LSAN)
- `interceptor_via_fun` - Function call (ASAN)
- `signed-integer-overflow` - Overflow (UBSAN)
- `race` - Data race (TSAN)

**Patterns:**
- Function name: `leak:FunctionName`
- Source file: `leak:filename.cpp`
- Shared library: `leak:library.so`

## Adding Sanitizer Checks to New Code

### 1. Write Tests First

```cpp
// Tests/LibWeb/TestNewFeature.cpp
TEST_CASE(new_feature_memory_safe)
{
    // This will run with ASAN in CI
    auto object = create_new_feature();
    object->use();
    // ASAN checks for leaks/UAF when test ends
}
```

### 2. Run Locally Before Push

```bash
# Build with sanitizers
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Run new test
./Build/sanitizers/bin/TestNewFeature

# Run all tests
ctest --preset Sanitizers

# Manual testing
./Build/sanitizers/bin/Ladybird
```

### 3. Check CI Results

- CI runs sanitizers automatically
- Green checkmark = no errors
- Red X = sanitizer found issue
- Check "Details" for logs

### 4. Fix Issues Before Merge

Don't merge if:
- Sanitizer errors in CI
- Tests timeout
- New suppressions without justification

## Best Practices

### 1. Fast Feedback

```bash
# Run sanitizers locally before push
make test-sanitizers  # If Makefile configured

# Or manually
cmake --build Build/sanitizers && ctest --preset Sanitizers
```

### 2. Incremental Testing

```bash
# Test only changed components
ctest --preset Sanitizers -R LibWeb

# Test specific test
./Build/sanitizers/bin/TestIPC
```

### 3. Clean Test Environment

```bash
# Clean build to avoid false positives
rm -rf Build/sanitizers
cmake --preset Sanitizers
cmake --build Build/sanitizers
```

### 4. Document Suppressions

Always add comment explaining why:

```
# Qt intentionally uses global QApplication instance
# Memory is reachable but not freed at exit
# This is by design - see Qt documentation
leak:QApplication::exec
```

## Troubleshooting CI Issues

### Issue: "Sanitizers not enabled"

**Symptom:** CI passes but no sanitizer output

**Fix:** Check CMake configuration:
```bash
# Verify ASAN is linked
ldd Build/sanitizers/bin/Ladybird | grep asan
# Should show: libasan.so
```

### Issue: "Different results locally vs CI"

**Symptom:** Test passes locally, fails in CI

**Possible causes:**
- Different sanitizer version
- Different suppression file
- Different environment variables

**Fix:** Match CI environment:
```bash
# Use same options as CI
export ASAN_OPTIONS="detect_leaks=1:halt_on_error=1"
export UBSAN_OPTIONS="halt_on_error=1"

# Run tests
ctest --preset Sanitizers
```

### Issue: "Too many logs in CI"

**Symptom:** Logs truncated, hard to read

**Fix:** Halt on first error:
```yaml
env:
  ASAN_OPTIONS: halt_on_error=1
```

### Issue: "Slow CI builds"

**Symptom:** Sanitizer builds take too long

**Optimizations:**
- Cache CMake build directory
- Use ccache
- Build only changed targets
- Parallel builds with `-j$(nproc)`

## Advanced CI Configurations

### Matrix Testing

Test with different sanitizers:

```yaml
strategy:
  matrix:
    sanitizer:
      - name: ASAN+UBSAN
        cmake_flags: -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined"
      - name: TSAN
        cmake_flags: -DCMAKE_CXX_FLAGS="-fsanitize=thread"
      - name: MSAN
        cmake_flags: -DCMAKE_CXX_FLAGS="-fsanitize=memory"

steps:
  - name: Configure
    run: cmake -B Build ${{ matrix.sanitizer.cmake_flags }}
```

### Nightly Full Sanitizer Runs

```yaml
# .github/workflows/nightly-sanitizers.yml
name: Nightly Sanitizer Tests

on:
  schedule:
    - cron: '0 2 * * *'  # Run at 2 AM daily

jobs:
  full-sanitizers:
    runs-on: ubuntu-latest
    steps:
      # Run all tests with strict options
      - name: Run with strict ASAN
        env:
          ASAN_OPTIONS: |
            detect_leaks=1:
            strict_string_checks=1:
            detect_stack_use_after_return=1:
            check_initialization_order=1
        run: ctest --preset Sanitizers
```

### Fuzzing + Sanitizers

Combine fuzzing with sanitizers in CI:

```yaml
- name: Fuzz with sanitizers
  run: |
    ./Build/sanitizers/bin/FuzzIPCMessages \
      -max_total_time=600 \
      -jobs=4 \
      corpus/

- name: Upload crashes
  if: failure()
  uses: actions/upload-artifact@v3
  with:
    name: fuzz-crashes
    path: crash-*
```

## Monitoring and Metrics

### Track Sanitizer Failures

```bash
# Count failures over time
grep "ERROR: AddressSanitizer" ci_logs/*.log | wc -l

# Track by type
grep "heap-use-after-free" ci_logs/*.log | wc -l
grep "leak:" ci_logs/*.log | wc -l
```

### Dashboard (Example)

Track metrics:
- Sanitizer errors per commit
- Time to fix sanitizer issues
- New suppressions added
- Test coverage with sanitizers

## Migration Guide

### Adding Sanitizers to Existing Project

1. **Start with ASAN:**
   ```cmake
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
   ```

2. **Fix initial errors:**
   - Run tests locally
   - Fix real bugs
   - Suppress third-party issues

3. **Add to CI:**
   - Create sanitizer workflow
   - Start with warnings only (don't fail CI)
   - Gradually make stricter

4. **Add UBSAN:**
   ```cmake
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
   ```

5. **Enable leak detection:**
   ```bash
   export ASAN_OPTIONS="detect_leaks=1"
   ```

6. **Make CI mandatory:**
   - Require sanitizer checks to pass
   - Block merge on failures

## Resources

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [ASAN Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- See `debugging-guide.md` for debugging workflow
- See `sanitizer-options.md` for configuration reference
