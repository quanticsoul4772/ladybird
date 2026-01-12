---
name: ci-cd-patterns
description: Comprehensive CI/CD patterns for Ladybird browser development, covering GitHub Actions workflows, matrix builds, sanitizers, release automation, and artifact publishing
use-when: Setting up CI workflows, configuring matrix builds, adding sanitizer tests, troubleshooting CI failures, automating releases, or optimizing build caching
allowed-tools:
  - Bash
  - Read
  - Write
  - Grep
scripts:
  - setup_ci_environment.sh
  - run_ci_tests.sh
  - publish_artifacts.sh
  - check_performance_regression.sh
---

# CI/CD Patterns for Ladybird Browser

## Overview
```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  PR Trigger  │ → │ Matrix Build │ → │  Run Tests   │ → │   Artifacts  │
│  (push/PR)   │    │ Multi-Config │    │  + Lint      │    │  + Reports   │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
        ↓                   ↓                   ↓                   ↓
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│    Cache     │    │  Sanitizers  │    │   Coverage   │    │   Release    │
│ ccache/vcpkg │    │  ASAN/UBSAN  │    │   Reports    │    │  Automation  │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
```

## 1. Ladybird CI Architecture

Ladybird uses a **reusable workflow pattern** for multi-platform CI:

### Main CI Entry Point
```yaml
# .github/workflows/ci.yml
name: CI
on: [push, pull_request]

concurrency:
  group: ${{ github.workflow }}-${{ github.head_ref || format('{0}-{1}', github.ref, github.run_number) }}
  cancel-in-progress: true  # Cancel previous runs on new push

jobs:
  Lagom:
    strategy:
      fail-fast: false
      matrix:
        os_name: ['Linux', 'macOS']
        arch: ['x86_64', 'arm64']
        build_preset: ['Release', 'Debug', 'Sanitizer', 'Fuzzers']
        toolchain: ['GNU', 'Clang']
        # Exclude invalid combinations
        exclude:
          - os_name: 'macOS'
            toolchain: 'GNU'

    uses: ./.github/workflows/lagom-template.yml
    with:
      toolchain: ${{ matrix.toolchain }}
      os_name: ${{ matrix.os_name }}
      arch: ${{ matrix.arch }}
      build_preset: ${{ matrix.build_preset }}
```

### Reusable Workflow Template
```yaml
# .github/workflows/lagom-template.yml
name: Lagom Template
on:
  workflow_call:
    inputs:
      toolchain: { required: true, type: string }
      os_name: { required: true, type: string }
      build_preset: { required: true, type: string }

env:
  CCACHE_DIR: ${{ github.workspace }}/.ccache
  VCPKG_ROOT: ${{ github.workspace }}/Build/vcpkg
  CCACHE_COMPILERCHECK: "%compiler% -v"

jobs:
  CI:
    runs-on: ${{ inputs.os_name == 'Linux' && 'ubuntu-latest' || 'macos-latest' }}
    steps:
      - uses: actions/checkout@v5
      - name: Build and Test
        run: |
          cmake --preset ${{ inputs.build_preset }}
          cmake --build Build
          ctest --preset ${{ inputs.build_preset }}
```

### Key Patterns

**1. Concurrency Control** - Automatically cancel outdated workflow runs
**2. Matrix Builds** - Test multiple OS/compiler/configuration combinations
**3. Reusable Workflows** - DRY principle for CI configurations
**4. Environment Variables** - Consistent paths and settings
**5. Cache Strategy** - ccache for compilation, vcpkg for dependencies

## 2. Matrix Build Strategies

### Platform Matrix (Linux/macOS/Windows)
```yaml
strategy:
  fail-fast: false  # Continue other jobs if one fails
  matrix:
    os: [ubuntu-24.04, macos-15, windows-latest]
    include:
      # Linux configurations
      - os: ubuntu-24.04
        toolchain: gcc-14
        preset: Release
      - os: ubuntu-24.04
        toolchain: clang-20
        preset: Sanitizer

      # macOS configurations
      - os: macos-15
        toolchain: clang  # Uses Xcode clang
        preset: Release
        arch: arm64

      # Windows configurations
      - os: windows-latest
        toolchain: clang-cl
        preset: Windows_CI
```

### Compiler Matrix (GCC/Clang Versions)
```yaml
strategy:
  matrix:
    compiler:
      - { name: gcc-14, cc: gcc-14, cxx: g++-14 }
      - { name: gcc-13, cc: gcc-13, cxx: g++-13 }
      - { name: clang-20, cc: clang-20, cxx: clang++-20 }
      - { name: clang-19, cc: clang-19, cxx: clang++-19 }

steps:
  - name: Build
    env:
      CC: ${{ matrix.compiler.cc }}
      CXX: ${{ matrix.compiler.cxx }}
    run: cmake --preset Release && cmake --build Build
```

### Build Preset Matrix
```yaml
strategy:
  matrix:
    preset:
      - name: Release
        tests: full
      - name: Debug
        tests: full
      - name: Sanitizer
        tests: sanitizer
        timeout: 60  # Sanitizers are slower
      - name: Fuzzers
        tests: none  # Fuzzers tested separately
      - name: Distribution
        tests: smoke  # Quick validation only
```

### Architecture Matrix (x86_64/arm64)
```yaml
strategy:
  matrix:
    include:
      - arch: x86_64
        runner: ubuntu-24.04
      - arch: arm64
        runner: ['self-hosted', 'linux', 'arm64']
      - arch: arm64
        runner: ['macos-15', 'self-hosted']

runs-on: ${{ matrix.runner }}
```

## 3. Build Presets in CI

Ladybird uses **CMake Presets** (CMakePresets.json) for reproducible builds:

### Available Presets

| Preset | Purpose | Sanitizers | Optimization | Use Case |
|--------|---------|------------|--------------|----------|
| `Release` | Production build | No | -O2 | Main CI, performance tests |
| `Debug` | Development build | No | -O0 -g | Debug symbols, slower |
| `Sanitizer` | Memory safety | ASAN+UBSAN | -O1 | Primary CI safety check |
| `Fuzzers` | Fuzzing targets | ASAN | -O1 | Continuous fuzzing |
| `Distribution` | Static build | No | -O3 | Release artifacts |
| `All_Debug` | Max debugging | No | -O0 | Debug all macros |

### Using Presets in CI
```yaml
- name: Configure Build
  run: cmake --preset ${{ matrix.preset }}

- name: Build
  run: cmake --build Build --config ${{ matrix.preset }}

- name: Test
  run: ctest --preset ${{ matrix.preset }} --output-on-failure
```

### Custom Preset for CI
```json
{
  "name": "CI_Fast",
  "inherits": "Release",
  "cacheVariables": {
    "ENABLE_CI_BASELINE_CPU": "ON",  // Baseline x86-64 for portability
    "BUILD_SHARED_LIBS": "OFF",       // Static for distribution
    "ENABLE_QT": "ON",
    "INCLUDE_WASM_SPEC_TESTS": "ON"
  }
}
```

## 4. Sanitizer Integration

### ASAN (AddressSanitizer)
Detects memory errors: use-after-free, buffer overflows, leaks

```yaml
- name: Build with ASAN
  run: |
    cmake --preset Sanitizer
    cmake --build Build

- name: Test with ASAN
  env:
    ASAN_OPTIONS: "strict_string_checks=1:detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:allocator_may_return_null=1"
  run: ctest --preset Sanitizer --output-on-failure
```

### UBSAN (UndefinedBehaviorSanitizer)
Detects undefined behavior: integer overflow, null pointer deref

```yaml
- name: Test with UBSAN
  env:
    UBSAN_OPTIONS: "print_stacktrace=1:print_summary=1:halt_on_error=1"
  run: ctest --preset Sanitizer --output-on-failure
```

### MSAN (MemorySanitizer)
Detects uninitialized memory reads (requires instrumented dependencies)

```yaml
- name: Build with MSAN
  run: |
    export CC=clang-20
    export CXX=clang++-20
    cmake -B Build -DENABLE_MEMORY_SANITIZER=ON
    cmake --build Build
```

### TSAN (ThreadSanitizer)
Detects data races in multi-threaded code

```yaml
- name: Build with TSAN
  run: |
    cmake -B Build -DENABLE_THREAD_SANITIZER=ON
    cmake --build Build
```

### Sanitizer CI Strategy

**Primary CI**: ASAN + UBSAN (catches 90% of memory issues)
**Weekly CI**: MSAN (slower, requires special build)
**On-Demand**: TSAN (for concurrency changes)

## 5. Automated Testing Strategy

### Test Hierarchy
```yaml
jobs:
  # Stage 1: Fast smoke tests (2-5 minutes)
  smoke-tests:
    runs-on: ubuntu-latest
    steps:
      - run: ./Meta/ladybird.py test AK  # Core data structures
      - run: ./Meta/ladybird.py test LibCore  # OS abstraction

  # Stage 2: Full test suite (15-30 minutes)
  full-tests:
    needs: smoke-tests
    strategy:
      matrix:
        suite: [LibWeb, LibJS, LibGfx, LibHTTP, Services]
    runs-on: ubuntu-latest
    steps:
      - run: ./Meta/ladybird.py test ${{ matrix.suite }}

  # Stage 3: Integration tests (30-60 minutes)
  integration-tests:
    needs: full-tests
    runs-on: ubuntu-latest
    steps:
      - run: ./Meta/ladybird.py run test-web
      - run: ./Meta/WPT.sh run --log results.log
```

### Test Categories

**Unit Tests**: C++ unit tests using LibTest framework
```bash
./Build/release/bin/TestAK
./Build/release/bin/TestLibWeb
```

**LibWeb Tests**: Text, Layout, Ref, Screenshot tests
```bash
./Meta/ladybird.py run test-web -- -f Text/input/
./Meta/ladybird.py run test-web -- -f Layout/
```

**Web Platform Tests**: Standards compliance
```bash
./Meta/WPT.sh run --log results.log
./Meta/WPT.sh compare --log results.log expectations.log
```

**JavaScript Tests**: Test262 conformance
```bash
./Meta/ladybird.py test LibJS
./Build/release/bin/test262-runner
```

### Test Parallelization
```yaml
- name: Run Tests in Parallel
  run: |
    ctest --preset Release \
      --parallel $(nproc) \
      --output-on-failure \
      --timeout 300
```

## 6. Cache Optimization

### ccache (Compiler Cache)
Dramatically speeds up recompilation by caching object files

```yaml
- name: Restore ccache
  uses: actions/cache@v4
  with:
    path: ${{ github.workspace }}/.ccache
    key: ${{ runner.os }}-ccache-${{ matrix.preset }}-${{ github.sha }}
    restore-keys: |
      ${{ runner.os }}-ccache-${{ matrix.preset }}-
      ${{ runner.os }}-ccache-

- name: Configure ccache
  run: |
    ccache --set-config=max_size=2G
    ccache --set-config=compression=true
    ccache --set-config=compression_level=6
    ccache --zero-stats

- name: Build
  env:
    CCACHE_DIR: ${{ github.workspace }}/.ccache
  run: cmake --build Build

- name: ccache Statistics
  run: ccache --show-stats
```

### vcpkg Binary Cache
Cache compiled dependencies (Qt, SQLite, etc.)

```yaml
- name: Restore vcpkg Cache
  uses: actions/cache@v4
  with:
    path: ${{ github.workspace }}/Build/caches/vcpkg-binary-cache
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json') }}

- name: Configure Build
  env:
    VCPKG_BINARY_SOURCES: "clear;files,${{ github.workspace }}/Build/caches/vcpkg-binary-cache,readwrite"
  run: cmake --preset Release
```

### Build Artifact Cache
```yaml
- name: Cache Build Directory
  uses: actions/cache@v4
  with:
    path: Build/
    key: build-${{ runner.os }}-${{ github.sha }}
```

### Cache Strategy Best Practices

1. **Cache Key Hierarchy**: Use cascading restore-keys for partial cache hits
2. **Size Limits**: Keep caches under 2GB (GitHub limit is 10GB per repo)
3. **Compression**: Enable compression for ccache to maximize storage
4. **Invalidation**: Include dependency hash in cache key for automatic invalidation

## 7. Artifact Publishing

### Build Artifacts
```yaml
- name: Upload Build Artifacts
  uses: actions/upload-artifact@v4
  with:
    name: ladybird-${{ matrix.os }}-${{ matrix.preset }}
    path: |
      Build/release/bin/Ladybird
      Build/release/bin/WebContent
      Build/release/bin/RequestServer
    retention-days: 7
```

### Test Reports
```yaml
- name: Generate Test Report
  if: always()  # Run even if tests fail
  run: |
    ctest --preset Release --output-junit test-results.xml

- name: Upload Test Results
  if: always()
  uses: actions/upload-artifact@v4
  with:
    name: test-results-${{ matrix.preset }}
    path: test-results.xml
```

### Coverage Reports
```yaml
- name: Generate Coverage
  run: |
    cmake -B Build -DENABLE_COVERAGE=ON
    cmake --build Build
    ctest --preset Release
    lcov --capture --directory Build --output-file coverage.info

- name: Upload to Codecov
  uses: codecov/codecov-action@v4
  with:
    files: coverage.info
    flags: ${{ matrix.preset }}
```

### Benchmark Results
```yaml
- name: Run Benchmarks
  run: ./Build/release/bin/js-benchmark-suite > benchmark-results.json

- name: Upload Benchmark Results
  uses: actions/upload-artifact@v4
  with:
    name: benchmark-results
    path: benchmark-results.json
```

## 8. Performance Regression Detection

### Benchmark Comparison Workflow
```yaml
name: Performance Regression Check

on:
  pull_request:
    types: [opened, synchronize]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5
        with:
          fetch-depth: 0  # Need history for comparison

      # Build and benchmark PR branch
      - name: Build PR
        run: cmake --preset Release && cmake --build Build

      - name: Run PR Benchmarks
        run: ./Build/release/bin/js-benchmark-suite > pr-benchmarks.json

      # Build and benchmark base branch
      - name: Checkout Base
        run: git checkout ${{ github.event.pull_request.base.sha }}

      - name: Build Base
        run: cmake --build Build --clean-first

      - name: Run Base Benchmarks
        run: ./Build/release/bin/js-benchmark-suite > base-benchmarks.json

      # Compare results
      - name: Compare Performance
        run: |
          python3 scripts/compare_benchmarks.py \
            base-benchmarks.json pr-benchmarks.json \
            --threshold 5  # Fail if >5% regression

      - name: Comment on PR
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('benchmark-report.md', 'utf8');
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: report
            });
```

### Memory Usage Tracking
```yaml
- name: Measure Memory Usage
  run: |
    /usr/bin/time -v ./Build/release/bin/Ladybird --run-tests \
      2>&1 | tee memory-usage.txt

    # Extract peak RSS
    grep "Maximum resident set size" memory-usage.txt > memory-report.txt
```

### Build Time Tracking
```yaml
- name: Track Build Time
  run: |
    START=$(date +%s)
    cmake --build Build
    END=$(date +%s)
    echo "Build time: $((END - START))s" | tee build-time.txt
```

## 9. Continuous Fuzzing

### OSS-Fuzz Integration
```yaml
name: Continuous Fuzzing

on:
  schedule:
    - cron: '0 */6 * * *'  # Every 6 hours

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5

      - name: Build Fuzzers
        run: |
          cmake --preset Fuzzers
          cmake --build Build

      - name: Run Fuzzing Campaign
        timeout-minutes: 60
        run: |
          for fuzzer in Build/fuzzers/bin/Fuzz*; do
            timeout 600 "$fuzzer" \
              -max_total_time=600 \
              -artifact_prefix=artifacts/ \
              corpus/$(basename "$fuzzer")/
          done

      - name: Upload Crashes
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: fuzzing-crashes
          path: artifacts/crash-*
```

### Fuzzer Corpus Management
```yaml
- name: Download Corpus
  uses: actions/download-artifact@v4
  with:
    name: fuzzing-corpus

- name: Minimize Corpus
  run: |
    ./Build/fuzzers/bin/FuzzIPCMessages -merge=1 \
      corpus-minimized/ corpus/ artifacts/
```

## 10. Release Automation

### Tagged Release Workflow
```yaml
name: Release

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5

      - name: Build Distribution
        run: |
          cmake --preset Distribution
          cmake --build Build --target package

      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: |
            Build/distribution/Ladybird-*.tar.gz
            Build/distribution/Ladybird-*.deb
            Build/distribution/Ladybird-*.AppImage
          body_path: RELEASE_NOTES.md
          draft: false
          prerelease: false
```

### Nightly Builds
```yaml
name: Nightly Build

on:
  schedule:
    - cron: '0 2 * * *'  # 2 AM UTC daily

jobs:
  nightly:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest, windows-latest]

    steps:
      - uses: actions/checkout@v5

      - name: Build Release
        run: |
          cmake --preset Release
          cmake --build Build

      - name: Package
        run: cpack -B Build/release

      - name: Upload to Nightly
        uses: pyTooling/Actions/releaser@r0
        with:
          token: ${{ secrets.GITHUB_TOKEN }}
          tag: nightly
          files: Build/release/Ladybird-*
```

### Deployment to Package Registries
```yaml
- name: Publish to APT Repository
  run: |
    dpkg-buildpackage -us -uc
    dput ppa:ladybird/nightly ../ladybird_*.changes

- name: Publish to Homebrew
  run: |
    brew bump-formula-pr ladybird \
      --url=https://github.com/ladybird/releases/latest.tar.gz
```

## 11. Security Scanning in CI

### Static Analysis (CodeQL)
```yaml
name: CodeQL Analysis

on:
  push:
    branches: [master]
  pull_request:
    branches: [master]
  schedule:
    - cron: '0 6 * * 1'  # Weekly Monday 6 AM

jobs:
  analyze:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5

      - name: Initialize CodeQL
        uses: github/codeql-action/init@v3
        with:
          languages: cpp, javascript

      - name: Build
        run: cmake --preset Release && cmake --build Build

      - name: Perform CodeQL Analysis
        uses: github/codeql-action/analyze@v3
```

### Dependency Scanning
```yaml
- name: Audit Dependencies
  run: |
    vcpkg install --dry-run | \
      xargs -I {} osv-scanner scan --lockfile=vcpkg.json
```

### Secrets Scanning
```yaml
- name: Scan for Secrets
  uses: trufflesecurity/trufflehog@main
  with:
    path: ./
    base: ${{ github.event.pull_request.base.sha }}
    head: ${{ github.event.pull_request.head.sha }}
```

## 12. Common CI Issues and Solutions

### Issue: Flaky Tests
**Symptom**: Tests pass locally but fail randomly in CI

**Solution**:
```yaml
- name: Run Tests with Retry
  uses: nick-fields/retry@v2
  with:
    timeout_minutes: 10
    max_attempts: 3
    command: ctest --preset Release --output-on-failure
```

### Issue: Out of Disk Space
**Symptom**: Build fails with "No space left on device"

**Solution**:
```yaml
- name: Free Disk Space
  run: |
    sudo rm -rf /usr/share/dotnet
    sudo rm -rf /opt/ghc
    sudo rm -rf /usr/local/share/boost
    sudo apt-get clean
    df -h  # Show remaining space
```

### Issue: Timeout on Large Builds
**Symptom**: Build times out after 6 hours (GitHub limit)

**Solution**:
```yaml
jobs:
  build:
    timeout-minutes: 360  # 6 hours max
    steps:
      - name: Build with Ninja
        run: cmake --build Build --parallel $(nproc)
```

### Issue: Cache Corruption
**Symptom**: Random build failures, works after cache clear

**Solution**:
```yaml
- name: Restore Cache
  uses: actions/cache@v4
  with:
    path: .ccache
    key: ${{ runner.os }}-ccache-${{ github.sha }}
    restore-keys: ${{ runner.os }}-ccache-

- name: Validate Cache
  run: ccache --cleanup  # Remove corrupted entries
```

### Issue: Matrix Job Explosion
**Symptom**: Too many CI jobs, long queue times

**Solution**:
```yaml
strategy:
  matrix:
    # Only test critical combinations
    include:
      - { os: ubuntu-24.04, compiler: gcc-14, preset: Sanitizer }
      - { os: ubuntu-24.04, compiler: clang-20, preset: Release }
      - { os: macos-15, compiler: clang, preset: Release }
    # Exclude unnecessary combinations
    exclude:
      - { os: macos, preset: Sanitizer }  # Sanitizers slow on macOS
```

## 13. CI Dashboard and Monitoring

### Status Badges
```markdown
[![CI](https://github.com/user/ladybird/workflows/CI/badge.svg)](https://github.com/user/ladybird/actions)
[![Coverage](https://codecov.io/gh/user/ladybird/branch/master/graph/badge.svg)](https://codecov.io/gh/user/ladybird)
```

### Workflow Status Tracking
```yaml
- name: Report Status
  if: always()
  uses: ravsamhq/notify-slack-action@v2
  with:
    status: ${{ job.status }}
    notification_title: 'CI Build ${{ job.status }}'
    message_format: '{emoji} *{workflow}* {status_message}'
```

## Checklist

- [ ] CI workflow configured with matrix builds
- [ ] Sanitizers enabled (ASAN + UBSAN minimum)
- [ ] ccache and vcpkg caching configured
- [ ] Test suite runs on every PR
- [ ] Artifacts uploaded for releases
- [ ] Performance regression detection enabled
- [ ] Security scanning (CodeQL) configured
- [ ] Flaky tests have retry logic
- [ ] Build time optimized (<30 minutes)
- [ ] Cache invalidation strategy defined
- [ ] Nightly builds configured
- [ ] Status badges added to README

## Best Practices

1. **Fast Feedback**: Prioritize fast smoke tests before slow integration tests
2. **Fail Fast**: Use `fail-fast: false` in matrix to see all failures
3. **Cache Aggressively**: Cache ccache, vcpkg, and intermediate build artifacts
4. **Test What Matters**: Focus on critical paths, use sampling for comprehensive tests
5. **Monitor CI Health**: Track build times, cache hit rates, and flaky tests
6. **Incremental Builds**: Use ccache and Ninja for fast rebuilds
7. **Parallel Everything**: Parallelize tests, builds, and matrix jobs
8. **Clean Caches Periodically**: Prevent cache bloat with scheduled cleanup

## References

- [GitHub Actions Documentation](https://docs.github.com/en/actions)
- [CMake Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [ccache Manual](https://ccache.dev/manual/latest.html)
- [AddressSanitizer](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [vcpkg Binary Caching](https://github.com/microsoft/vcpkg/blob/master/docs/users/binarycaching.md)

## Related Skills

### Build Integration
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Configure CI builds with CMake presets. Use ccache and vcpkg binary caching for faster builds.

### Testing in CI
- **[web-standards-testing](../web-standards-testing/SKILL.md)**: Run WPT and LibWeb tests in CI. Configure parallel test execution and report generation.
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Set up continuous fuzzing in CI. Run fuzzer corpus regression tests and integrate OSS-Fuzz.
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Run sanitizer builds in CI. Configure ASAN/UBSAN/MSAN options for all test suites.

### Performance Monitoring
- **[browser-performance](../browser-performance/SKILL.md)**: Run performance regression tests in CI. Track benchmark results and build times across commits.

### Quality Gates
- **[ipc-security](../ipc-security/SKILL.md)**: Run IPC fuzzing and security tests in CI. Validate IPC handler robustness.
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Enforce code style checks in CI. Run clang-format and linters.
- **[ladybird-git-workflow](../ladybird-git-workflow/SKILL.md)**: Validate commit message format in CI. Check for proper category prefixes.

### Documentation
- **[documentation-generation](../documentation-generation/SKILL.md)**: Generate and publish documentation in CI. Render Mermaid diagrams and API docs.

### Multi-Platform Testing
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Test multi-process architecture on all platforms. Verify process isolation and IPC.
