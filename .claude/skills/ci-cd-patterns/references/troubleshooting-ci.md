# Troubleshooting CI Failures

## Common CI Issues

### 1. Build Timeouts

**Symptom**: Workflow times out after 6 hours (GitHub limit)

**Causes**:
- No ccache (building from scratch every time)
- vcpkg rebuilding dependencies
- Inefficient parallelization

**Solutions**:

```yaml
# Set job timeout
jobs:
  build:
    timeout-minutes: 120  # 2 hours

# Enable ccache
- uses: actions/cache@v4
  with:
    path: ~/.ccache
    key: ccache-${{ github.sha }}
    restore-keys: ccache-

# Parallelize build
- run: cmake --build Build --parallel $(nproc)

# Use vcpkg binary cache
- env:
    VCPKG_BINARY_SOURCES: "clear;files,cache/vcpkg,readwrite"
  run: cmake --preset Release
```

### 2. Out of Disk Space

**Symptom**: `No space left on device`

**Causes**:
- Large build directories
- Too many artifacts
- Cache bloat

**Solutions**:

```yaml
- name: Free Disk Space
  run: |
    # Remove unnecessary packages
    sudo apt-get clean
    sudo rm -rf /usr/share/dotnet
    sudo rm -rf /opt/ghc
    sudo rm -rf /usr/local/share/boost

    # Show available space
    df -h

- name: Clean Build Directory
  run: |
    # Remove unnecessary build artifacts
    find Build -name "*.o" -delete
    find Build -name "*.a" -delete

    # Keep only final binaries
    du -sh Build/
```

### 3. Out of Memory

**Symptom**: `Killed` or `segmentation fault` during compilation

**Causes**:
- Too many parallel jobs
- Memory-intensive compilation (templates, LTO)
- Sanitizer builds

**Solutions**:

```yaml
# Limit parallel jobs based on available RAM
- name: Build with Memory Limit
  run: |
    # Calculate safe parallelism (2GB per job)
    AVAILABLE_RAM=$(free -g | awk '/^Mem:/{print $2}')
    MAX_JOBS=$((AVAILABLE_RAM / 2))
    JOBS=$(( MAX_JOBS < $(nproc) ? MAX_JOBS : $(nproc) ))

    echo "Using $JOBS parallel jobs"
    cmake --build Build --parallel $JOBS

# Disable LTO for CI
- run: |
    cmake --preset Release \
      -DENABLE_LTO=OFF
```

### 4. Flaky Tests

**Symptom**: Tests pass locally but fail randomly in CI

**Causes**:
- Timing issues (race conditions)
- Uninitialized memory
- Environment dependencies
- Network issues

**Solutions**:

```yaml
# Retry flaky tests
- uses: nick-fields/retry@v2
  with:
    timeout_minutes: 10
    max_attempts: 3
    command: ctest --output-on-failure

# Run tests in random order (detect order dependencies)
- run: ctest --schedule-random

# Set deterministic environment
- env:
    TZ: UTC
    LANG: C.UTF-8
    LC_ALL: C.UTF-8
  run: ctest

# Increase timeouts for slow CI runners
- run: ctest --timeout 300  # 5 minutes per test
```

### 5. Sanitizer False Positives

**Symptom**: ASAN/UBSAN reports errors that don't reproduce locally

**Causes**:
- Third-party library issues
- Known issues in dependencies
- Different compiler versions

**Solutions**:

```yaml
# Use suppression files
- name: Run Tests with Sanitizer Suppressions
  env:
    ASAN_OPTIONS: "suppressions=${{ github.workspace }}/Meta/sanitizers/asan.supp"
    LSAN_OPTIONS: "suppressions=${{ github.workspace }}/Meta/sanitizers/lsan.supp"
    UBSAN_OPTIONS: "suppressions=${{ github.workspace }}/Meta/sanitizers/ubsan.supp"
  run: ctest --preset Sanitizer
```

Example suppression file (`Meta/sanitizers/lsan.supp`):
```
# Ignore leaks in third-party libraries
leak:libQt5Core.so
leak:libfontconfig.so

# Known issues
leak:vcpkg_installed
```

### 6. Cache Corruption

**Symptom**: Random build failures, works after clearing cache

**Causes**:
- Incomplete cache writes
- Cache key collision
- ccache corruption

**Solutions**:

```yaml
# Add cache validation
- name: Restore Cache
  uses: actions/cache@v4
  id: cache
  with:
    path: ~/.ccache
    key: ccache-${{ github.sha }}

- name: Validate Cache
  if: steps.cache.outputs.cache-hit == 'true'
  run: |
    # Check ccache integrity
    ccache --show-stats || {
      echo "Cache corrupted, clearing..."
      rm -rf ~/.ccache
    }

# Periodic cache refresh
- name: Clear Old Cache
  if: github.run_number % 100 == 0
  run: |
    ccache --clear
    echo "Cache cleared (periodic refresh)"
```

### 7. vcpkg Build Failures

**Symptom**: `error: building <package> failed`

**Causes**:
- Network issues downloading packages
- Dependency version conflicts
- Platform-specific build issues

**Solutions**:

```yaml
# Retry vcpkg installation
- name: Install vcpkg Dependencies
  uses: nick-fields/retry@v2
  with:
    timeout_minutes: 30
    max_attempts: 3
    command: cmake --preset Release

# Use manifest mode (reproducible)
# vcpkg.json
{
  "dependencies": [
    { "name": "qt5", "version>=": "5.15.10" }
  ],
  "builtin-baseline": "abc123",  # Pin baseline
  "overrides": [
    { "name": "zlib", "version": "1.2.13" }  # Pin specific versions
  ]
}

# Increase vcpkg verbosity
- env:
    VCPKG_KEEP_ENV_VARS: "PATH"
  run: cmake --preset Release -- --trace-expand
```

### 8. Permission Errors

**Symptom**: `Permission denied` when accessing files

**Causes**:
- Scripts not executable
- Wrong file ownership
- SELinux/AppArmor restrictions

**Solutions**:

```yaml
# Make scripts executable
- name: Fix Permissions
  run: |
    chmod +x scripts/*.sh
    chmod +x Meta/*.sh

# Fix ownership (for self-hosted runners)
- name: Fix Ownership
  run: |
    sudo chown -R $(whoami):$(whoami) .
```

### 9. Network Timeouts

**Symptom**: `Failed to connect` or `Connection timed out`

**Causes**:
- Slow/unreliable network
- Rate limiting (GitHub API, registries)
- Blocked IPs

**Solutions**:

```yaml
# Retry network operations
- name: Clone vcpkg
  uses: nick-fields/retry@v2
  with:
    timeout_minutes: 10
    max_attempts: 3
    command: |
      git clone https://github.com/microsoft/vcpkg.git Build/vcpkg

# Use GitHub token for API calls
- env:
    GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
  run: gh api repos/${{ github.repository }}
```

### 10. Matrix Job Failures

**Symptom**: Some matrix combinations fail, others pass

**Causes**:
- Platform-specific bugs
- Compiler differences
- Missing dependencies

**Solutions**:

```yaml
strategy:
  fail-fast: false  # Continue other jobs
  matrix:
    include:
      - os: ubuntu-24.04
        compiler: gcc-14
        continue-on-error: false  # Must pass

      - os: macos-15
        compiler: clang
        continue-on-error: true   # Allow failure

# Conditional steps per OS
- name: Install Dependencies (Linux)
  if: runner.os == 'Linux'
  run: sudo apt-get install -y gcc-14

- name: Install Dependencies (macOS)
  if: runner.os == 'macOS'
  run: brew install llvm
```

## Debugging Techniques

### Enable Debug Logging

```yaml
# GitHub Actions debug logging
# Set repository secret: ACTIONS_STEP_DEBUG = true

# Or in workflow:
- run: echo "::debug::Debug message"

# Verbose output
- run: cmake --preset Release --verbose
- run: ctest --output-on-failure --verbose
```

### SSH Into Runner (for debugging)

```yaml
- name: Debug via SSH
  if: failure()  # Only on failure
  uses: mxschmitt/action-tmate@v3
  with:
    limit-access-to-actor: true  # Only PR author
    timeout-minutes: 30
```

### Dump Environment

```yaml
- name: Debug Environment
  run: |
    echo "=== Environment Variables ==="
    env | sort

    echo "=== GitHub Context ==="
    echo '${{ toJSON(github) }}'

    echo "=== Runner Context ==="
    echo '${{ toJSON(runner) }}'

    echo "=== Job Context ==="
    echo '${{ toJSON(job) }}'

    echo "=== System Info ==="
    uname -a
    cat /proc/cpuinfo | grep "model name" | head -1
    free -h
    df -h
```

### Reproduce Locally

```bash
# Use act to run GitHub Actions locally
brew install act

# Run workflow
act -j build

# With secrets
act -j build --secret-file .secrets

# With specific event
act pull_request

# Debug mode
act -j build --verbose
```

### Bisect Failure

```yaml
# Find which commit broke CI
- name: Bisect Failure
  run: |
    git bisect start
    git bisect bad HEAD
    git bisect good ${{ github.event.pull_request.base.sha }}

    # Automated bisect
    git bisect run ./scripts/test.sh
```

## Performance Issues

### Slow Tests

```yaml
# Identify slow tests
- name: Test with Timing
  run: |
    ctest --output-on-failure --verbose | tee test-output.txt

    # Extract timing
    grep "test [0-9]" test-output.txt | \
      sed 's/.*test \([0-9]*\).*\([0-9.]*\) sec.*/\2 \1/' | \
      sort -rn | head -20
```

### Slow Builds

```yaml
# Profile build time
- name: Build with Timing
  run: |
    time cmake --build Build --verbose 2>&1 | tee build-log.txt

    # Find slowest files
    grep "Building CXX object" build-log.txt | \
      awk '{print $NF}' | sort | uniq -c | sort -rn | head -20
```

## Common Error Messages

### "Cache already exists"

**Message**: `Error: Cache already exists for key: <key>`

**Cause**: Trying to save a cache with the same key

**Solution**:
```yaml
# Use unique keys per commit
key: cache-${{ github.sha }}

# Or use cache/save action
- uses: actions/cache/save@v4
  with:
    path: ~/.ccache
    key: cache-${{ github.sha }}
```

### "Resource not accessible by integration"

**Message**: `Resource not accessible by integration`

**Cause**: Missing permissions

**Solution**:
```yaml
permissions:
  contents: read
  pull-requests: write
  issues: write
```

### "Container action is not supported on macOS"

**Message**: Container actions are only supported on Linux runners

**Cause**: Using Docker action on macOS

**Solution**:
```yaml
# Use conditional action
- uses: docker-action
  if: runner.os == 'Linux'

# Or use composite action
- uses: composite-action
  # Works on all platforms
```

## Getting Help

### Check Workflow Runs
```
https://github.com/<owner>/<repo>/actions/runs/<run-id>
```

### Review Logs
- Download raw logs
- Search for error keywords
- Check timing information

### Community Resources
- [GitHub Community Forum](https://github.com/orgs/community/discussions)
- [GitHub Actions Issues](https://github.com/actions/runner/issues)
- [Stack Overflow](https://stackoverflow.com/questions/tagged/github-actions)

### Create Reproducible Issue

```yaml
# Minimal reproducible example
name: Minimal Repro

on: [push]

jobs:
  repro:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v5
      - run: |
          echo "Reproducing issue..."
          # Minimal steps to reproduce
```
