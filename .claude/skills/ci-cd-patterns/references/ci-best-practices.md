# CI/CD Best Practices for Browser Development

## General Principles

### 1. Fast Feedback Loop
- **Goal**: Developers should know within 15-30 minutes if their change breaks something
- **Strategy**: Tiered testing (fast smoke tests → full suite → integration tests)
- **Implementation**: Run critical unit tests first, long-running tests in parallel

### 2. Fail Fast
- **Principle**: Surface failures immediately, don't waste CI time on known-broken builds
- **Matrix Builds**: Use `fail-fast: false` to see all failures, not just the first
- **Pre-commit Checks**: Catch issues locally before CI (formatting, linting, basic tests)

### 3. Reproducible Builds
- **Pinned Dependencies**: Lock vcpkg versions, use specific compiler versions
- **Cache Strategy**: Cache compiled dependencies (vcpkg), not source files
- **Baseline CPU**: Use `-march=x86-64` for portable binaries, not `-march=native`

### 4. Scale Testing Intelligently
- **Full CI**: Every push/PR (core functionality)
- **Extended CI**: Nightly or weekly (WPT, fuzzing, sanitizers)
- **Pre-release CI**: Before merges (integration tests, performance benchmarks)

## Browser-Specific Patterns

### Multi-Process Testing
Browsers use multi-process architecture (UI, WebContent, RequestServer, etc.)

```yaml
- name: Test Multi-Process Communication
  run: |
    # Test IPC message passing
    ./Build/release/bin/TestIPC

    # Test process isolation
    ./Build/release/bin/TestSandbox

    # Test process lifecycle
    ./Build/release/bin/TestProcessManager
```

### Memory Safety is Critical
Use sanitizers aggressively - browsers are prime targets for exploits

```yaml
strategy:
  matrix:
    sanitizer:
      - name: ASAN+UBSAN
        flags: "-fsanitize=address,undefined"
        env:
          ASAN_OPTIONS: "detect_leaks=1:strict_init_order=1"
          UBSAN_OPTIONS: "halt_on_error=1:print_stacktrace=1"

      - name: MSAN (weekly)
        flags: "-fsanitize=memory"
        schedule: weekly

      - name: TSAN (on-demand)
        flags: "-fsanitize=thread"
        trigger: manual
```

### Rendering Tests
Visual regression testing for layout engine

```yaml
- name: Screenshot Tests
  run: |
    # Generate screenshots
    ./Meta/ladybird.py run test-web -- -f Screenshot/

    # Compare with baselines
    ./scripts/compare_screenshots.sh \
      Tests/LibWeb/Screenshot/expected/ \
      Tests/LibWeb/Screenshot/actual/

- name: Upload Visual Diffs
  if: failure()
  uses: actions/upload-artifact@v4
  with:
    name: screenshot-diffs
    path: Tests/LibWeb/Screenshot/diffs/
```

### Web Standards Compliance
Regular testing against official test suites

```yaml
- name: Web Platform Tests
  run: |
    ./Meta/WPT.sh run --log wpt-results.log

    # Compare pass rate
    CURRENT_PASS_RATE=$(grep "^PASS" wpt-results.log | wc -l)
    BASELINE_PASS_RATE=$(grep "^PASS" wpt-baseline.log | wc -l)

    if [ $CURRENT_PASS_RATE -lt $BASELINE_PASS_RATE ]; then
      echo "❌ WPT pass rate decreased!"
      exit 1
    fi
```

## Cache Optimization

### ccache Strategy
```yaml
- uses: actions/cache@v4
  with:
    path: ~/.ccache
    # Hierarchical keys for fallback
    key: ccache-${{ runner.os }}-${{ matrix.compiler }}-${{ github.sha }}
    restore-keys: |
      ccache-${{ runner.os }}-${{ matrix.compiler }}-
      ccache-${{ runner.os }}-

- name: Configure ccache
  run: |
    # Optimize for CI
    ccache --set-config=max_size=2G
    ccache --set-config=compression=true
    ccache --set-config=compression_level=6

    # Ignore certain things for cache key
    ccache --set-config=sloppiness=time_macros,include_file_mtime

    # Use compiler version only (ignore plugin .so changes)
    ccache --set-config=compiler_check="%compiler% -v"
```

### vcpkg Binary Cache
```yaml
- uses: actions/cache@v4
  with:
    path: Build/caches/vcpkg-binary-cache
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json', 'vcpkg-configuration.json') }}

- name: Configure vcpkg Cache
  env:
    VCPKG_BINARY_SOURCES: "clear;files,${{ github.workspace }}/Build/caches/vcpkg-binary-cache,readwrite"
  run: cmake --preset Release
```

### Cache Invalidation
- **On Dependency Change**: Hash `vcpkg.json` in cache key
- **On Compiler Update**: Include compiler version in key
- **On Build System Change**: Include CMakePresets.json hash
- **Periodic Cleanup**: Clear cache weekly to prevent bloat

## Parallelization

### Test Parallelization
```yaml
- name: Run Tests in Parallel
  run: |
    # CTest built-in parallelization
    ctest --parallel $(nproc) --output-on-failure

    # Custom parallelization for long tests
    ./scripts/run_tests_parallel.sh \
      --jobs $(nproc) \
      --timeout 300
```

### Matrix Parallelization
```yaml
strategy:
  matrix:
    # Split tests by category
    test_suite:
      - AK
      - LibCore
      - LibWeb
      - LibJS
      - LibGfx
      - Services

jobs:
  test:
    name: Test ${{ matrix.test_suite }}
    steps:
      - run: ctest -R "^${{ matrix.test_suite }}"
```

### Build Parallelization
```yaml
- name: Build
  run: |
    # Use all available cores
    cmake --build Build --parallel $(nproc)

    # Or with explicit job count
    cmake --build Build -j$(nproc)
```

## Continuous Fuzzing

### Integration with OSS-Fuzz
```yaml
# Fuzzers run continuously on Google's infrastructure
# Local CI runs short campaigns

- name: Quick Fuzzing Check
  timeout-minutes: 10
  run: |
    for fuzzer in Build/fuzzers/bin/Fuzz*; do
      # 1 minute per fuzzer
      timeout 60 "$fuzzer" -max_total_time=60 corpus/
    done
```

### Crash Triage Automation
```yaml
- name: Triage Fuzzing Crashes
  if: always()
  run: |
    if ls artifacts/crash-* 1> /dev/null 2>&1; then
      ./scripts/triage_crashes.sh artifacts/

      # Create issues for new crashes
      for crash in artifacts/crash-*; do
        ./scripts/create_crash_issue.sh "$crash"
      done
    fi
```

## Performance Regression Detection

### Automated Benchmarking
```yaml
- name: Run Benchmarks
  run: |
    # Warm up
    ./Build/release/bin/js-benchmark-suite --iterations 1

    # Set CPU governor for consistency
    echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

    # Run benchmarks
    ./Build/release/bin/js-benchmark-suite \
      --iterations 5 \
      --output benchmarks.json

- name: Compare with Baseline
  run: |
    python3 scripts/compare_benchmarks.py \
      baseline-benchmarks.json \
      benchmarks.json \
      --threshold 5.0  # Fail on >5% regression
```

### Build Size Tracking
```yaml
- name: Track Binary Size
  run: |
    # Measure stripped binary sizes
    strip Build/release/bin/Ladybird -o Ladybird-stripped
    strip Build/release/bin/WebContent -o WebContent-stripped

    LADYBIRD_SIZE=$(stat -f%z Ladybird-stripped)
    WEBCONTENT_SIZE=$(stat -f%z WebContent-stripped)

    # Compare with baseline
    if [ $LADYBIRD_SIZE -gt $((BASELINE_SIZE * 11 / 10)) ]; then
      echo "❌ Binary size increased by >10%"
      exit 1
    fi
```

## Security Practices

### Dependency Scanning
```yaml
- name: Audit Dependencies
  run: |
    # Scan vcpkg dependencies
    vcpkg list | while read pkg; do
      osv-scanner scan --lockfile=vcpkg.json "$pkg"
    done

    # Check for known vulnerabilities
    ./scripts/check_cves.sh
```

### Static Analysis
```yaml
- name: CodeQL Analysis
  uses: github/codeql-action/init@v3
  with:
    languages: cpp
    queries: security-and-quality

- name: Build for Analysis
  run: cmake --build Build

- uses: github/codeql-action/analyze@v3
```

### Secrets Scanning
```yaml
- name: Scan for Leaked Secrets
  uses: trufflesecurity/trufflehog@main
  with:
    path: ./
    base: ${{ github.event.pull_request.base.sha }}
    head: ${{ github.event.pull_request.head.sha }}
```

## Artifact Management

### Retention Policies
```yaml
- uses: actions/upload-artifact@v4
  with:
    name: build-artifacts
    path: Build/release/bin/
    # Tiered retention
    retention-days: ${{ github.event_name == 'release' && 90 || 7 }}
```

### Artifact Compression
```yaml
- name: Compress Artifacts
  run: |
    # Strip binaries
    strip Build/release/bin/*

    # Create compressed archive
    tar -czf ladybird-${{ runner.os }}.tar.gz \
      -C Build/release/bin .

    # Compute checksum
    sha256sum ladybird-${{ runner.os }}.tar.gz > checksums.txt
```

## Monitoring and Observability

### CI Health Metrics
Track:
- **Build Success Rate**: Should be >95%
- **Average Build Time**: Track trends, optimize if increasing
- **Cache Hit Rate**: Should be >80%
- **Flaky Test Rate**: Quarantine tests with <95% pass rate
- **Queue Time**: Monitor for capacity issues

### Status Notifications
```yaml
- name: Notify on Failure
  if: failure()
  uses: ravsamhq/notify-slack-action@v2
  with:
    status: ${{ job.status }}
    notification_title: 'CI Failed: ${{ github.workflow }}'
    message_format: '{emoji} *{workflow}* {status_message} in <{repo_url}|{repo}>'
  env:
    SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK }}
```

## Flaky Test Management

### Automatic Retry
```yaml
- uses: nick-fields/retry@v2
  with:
    timeout_minutes: 10
    max_attempts: 3
    retry_on: error
    command: ctest --preset Release --output-on-failure
```

### Quarantine Flaky Tests
```yaml
- name: Run Tests
  run: |
    # Skip known flaky tests
    ctest --exclude-regex "FlakyTest|UnstableTest"

- name: Run Flaky Tests (allow failure)
  continue-on-error: true
  run: ctest --tests-regex "FlakyTest|UnstableTest"
```

## Cost Optimization

### Self-Hosted Runners
For expensive CI:
- **Long builds**: Browsers take 30+ minutes to compile
- **High frequency**: Many PRs per day
- **Resource intensive**: Sanitizer builds use lots of memory

```yaml
jobs:
  build:
    runs-on: [self-hosted, linux, x64, high-memory]
```

### Conditional CI
```yaml
on:
  pull_request:
    paths:
      # Only run on code changes
      - 'Libraries/**'
      - 'Services/**'
      - 'CMakeLists.txt'
      # Skip on doc changes
      - '!**.md'
      - '!Documentation/**'
```

## Anti-Patterns to Avoid

❌ **Building from Scratch Every Time**
- Use ccache and vcpkg binary cache

❌ **Running All Tests Sequentially**
- Parallelize with CTest or custom orchestration

❌ **No Test Categorization**
- Split into smoke/unit/integration tiers

❌ **Ignoring Flaky Tests**
- Track and fix or quarantine

❌ **No Performance Tracking**
- Regression detection is critical for browsers

❌ **Storing Large Artifacts**
- Compress, use short retention periods

❌ **No Matrix Pruning**
- Don't test every OS × compiler × config combination

❌ **Hardcoded Paths**
- Use ${{ github.workspace }} and environment variables

❌ **No Cache Strategy**
- Browser builds are too slow without caching

❌ **Mixing Concerns**
- Separate build, test, deploy into different jobs

## Recommended Reading

- [Google's Testing Blog](https://testing.googleblog.com/)
- [Mozilla's CI Documentation](https://firefox-source-docs.mozilla.org/testing/index.html)
- [Chromium's Testing Guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/)
- [GitHub Actions Best Practices](https://docs.github.com/en/actions/learn-github-actions/best-practices-for-using-github-actions)
