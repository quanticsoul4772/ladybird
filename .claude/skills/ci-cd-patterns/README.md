# CI/CD Patterns Skill for Ladybird

Comprehensive CI/CD patterns for Ladybird browser development, covering GitHub Actions workflows, matrix builds, sanitizers, release automation, and artifact publishing.

## Skill Overview

This skill provides:
- **Example Workflows**: Production-ready GitHub Actions workflows
- **Shell Scripts**: Automated setup, testing, artifact publishing, and performance checking
- **Reference Documentation**: Best practices, caching strategies, troubleshooting guides
- **Browser-Specific Patterns**: Multi-process testing, rendering tests, web standards compliance

## Quick Start

### View Main Documentation
```bash
cat .claude/skills/ci-cd-patterns/SKILL.md
```

### Example Workflows

Located in `examples/`:
- `basic-ci-workflow.yml` - Simple build and test
- `matrix-build-workflow.yml` - Multi-platform, multi-compiler testing
- `sanitizer-workflow.yml` - Memory safety testing (ASAN/UBSAN/MSAN/TSAN)
- `fuzzing-workflow.yml` - Continuous fuzzing campaigns
- `release-workflow.yml` - Automated release packaging and publishing
- `performance-workflow.yml` - Performance regression detection

### Helper Scripts

Located in `scripts/`:
- `setup_ci_environment.sh` - Install dependencies, setup vcpkg and ccache
- `run_ci_tests.sh` - Execute all test suites with proper error handling
- `publish_artifacts.sh` - Package and publish release artifacts
- `check_performance_regression.sh` - Compare benchmark results

Make scripts executable:
```bash
chmod +x scripts/*.sh
```

### Reference Documentation

Located in `references/`:
- `github-actions-reference.md` - Complete GitHub Actions syntax reference
- `ci-best-practices.md` - CI/CD patterns for browser development
- `caching-strategies.md` - Optimize build times with ccache and vcpkg
- `troubleshooting-ci.md` - Debug common CI failures

## Usage Examples

### Setup CI Environment Locally

```bash
./scripts/setup_ci_environment.sh

# Source environment variables
source ci-env.sh

# Verify setup
cmake --version
ccache --version
```

### Run Tests Like CI Does

```bash
./scripts/run_ci_tests.sh \
  --build-dir Build/release \
  --preset Release \
  --parallel 4 \
  --timeout 300
```

### Check Performance Regression

```bash
# Run benchmarks on base branch
./Build/release/bin/js-benchmark-suite --output base-benchmarks.json

# Switch to PR branch and rebuild
git checkout pr-branch
cmake --build Build

# Run benchmarks on PR
./Build/release/bin/js-benchmark-suite --output pr-benchmarks.json

# Compare results
./scripts/check_performance_regression.sh \
  --base base-benchmarks.json \
  --pr pr-benchmarks.json \
  --threshold 5.0
```

### Publish Release Artifacts

```bash
# Build release version
cmake --preset Distribution
cmake --build Build

# Package and publish
./scripts/publish_artifacts.sh \
  --build-dir Build/distribution \
  --version 1.0.0 \
  --sign
```

## Key Patterns

### 1. Matrix Builds
Test multiple OS, compiler, and configuration combinations:
```yaml
strategy:
  fail-fast: false
  matrix:
    os: [ubuntu-24.04, macos-15, windows-latest]
    compiler: [gcc-14, clang-20]
    preset: [Release, Debug, Sanitizer]
```

### 2. Reusable Workflows
DRY principle - define once, use everywhere:
```yaml
jobs:
  build:
    uses: ./.github/workflows/reusable-build.yml
    with:
      preset: Release
      compiler: gcc-14
```

### 3. Aggressive Caching
Browser builds are slow - cache everything:
```yaml
- uses: actions/cache@v4
  with:
    path: ~/.ccache
    key: ccache-${{ runner.os }}-${{ github.sha }}
    restore-keys: ccache-${{ runner.os }}-
```

### 4. Sanitizer Testing
Memory safety is critical:
```yaml
- name: Test with ASAN+UBSAN
  env:
    ASAN_OPTIONS: "detect_leaks=1:strict_init_order=1"
    UBSAN_OPTIONS: "halt_on_error=1:print_stacktrace=1"
  run: ctest --preset Sanitizer
```

### 5. Performance Regression Detection
Prevent performance regressions:
```yaml
- run: ./scripts/check_performance_regression.sh \
    --base base-benchmarks.json \
    --pr pr-benchmarks.json \
    --threshold 5.0
```

## Ladybird-Specific Configuration

### Build Presets
```yaml
strategy:
  matrix:
    preset:
      - Release          # Production build
      - Debug            # Development build
      - Sanitizer        # ASAN + UBSAN
      - Fuzzers          # Fuzzing targets
      - Distribution     # Static release
      - All_Debug        # All debug macros
```

### Test Categories
```yaml
jobs:
  unit-tests:
    run: ctest -R "^Test(AK|LibCore|LibWeb)"

  libweb-tests:
    run: ./Meta/ladybird.py run test-web

  js-tests:
    run: ./Build/release/bin/test262-runner

  wpt-tests:
    run: ./Meta/WPT.sh run --log results.log
```

### Multi-Process Testing
```yaml
- name: Test IPC
  run: ./Build/release/bin/TestIPC

- name: Test Process Isolation
  run: ./Build/release/bin/TestSandbox
```

## Common Workflows

### Basic CI (Every PR)
1. Build with Release preset
2. Run unit tests
3. Run LibWeb text tests
4. Upload test results

### Extended CI (Nightly)
1. Build with all presets (Release, Debug, Sanitizer, Fuzzers)
2. Run full test suite (including WPT)
3. Run fuzzing campaign (6 hours)
4. Performance benchmarks
5. Publish nightly builds

### Release CI (On Tag)
1. Build distribution binaries (Linux, macOS, Windows)
2. Package artifacts (DEB, RPM, DMG, EXE)
3. Sign binaries
4. Generate checksums
5. Create GitHub release
6. Publish to package registries

## Performance Optimization

### Expected Build Times

| Configuration | First Build | Cached Build |
|---------------|-------------|--------------|
| Release | 25-35 min | 5-10 min |
| Debug | 30-40 min | 8-12 min |
| Sanitizer | 35-45 min | 10-15 min |
| Fuzzers | 20-30 min | 5-8 min |

### Cache Hit Rates

| Cache Type | Expected Hit Rate |
|------------|-------------------|
| ccache | 80-95% |
| vcpkg | 90-99% |
| Build artifacts | 70-85% |

### Cost Optimization
- Use ccache and vcpkg binary cache
- Parallelize tests with CTest
- Self-hosted runners for high-frequency CI
- Conditional CI (skip on doc-only changes)
- Tiered testing (smoke → full → integration)

## Troubleshooting

### Build Timeout
```yaml
# Increase timeout and enable caching
jobs:
  build:
    timeout-minutes: 120
    steps:
      - uses: actions/cache@v4  # Enable ccache
```

### Out of Disk Space
```yaml
- run: |
    sudo rm -rf /usr/share/dotnet
    sudo apt-get clean
    df -h
```

### Flaky Tests
```yaml
- uses: nick-fields/retry@v2
  with:
    max_attempts: 3
    command: ctest
```

### Cache Corruption
```yaml
- run: |
    ccache --show-stats || {
      echo "Cache corrupted"
      rm -rf ~/.ccache
    }
```

See `references/troubleshooting-ci.md` for comprehensive debugging guide.

## Best Practices

1. **Fast Feedback**: Smoke tests in <5 minutes, full suite in <30 minutes
2. **Fail Fast**: Use `fail-fast: false` in matrix to see all failures
3. **Cache Aggressively**: ccache + vcpkg = 80% faster builds
4. **Test What Matters**: Focus on critical paths, sample comprehensive tests
5. **Monitor CI Health**: Track build times, cache hit rates, flaky tests
6. **Security First**: Sanitizers on every PR, fuzzing continuously
7. **Performance**: Detect regressions automatically
8. **Documentation**: Comment complex workflows, maintain this README

## Integration with Ladybird

This skill integrates with:
- `CMakePresets.json` - Build preset definitions
- `Meta/ladybird.py` - Build and test orchestration
- `Meta/WPT.sh` - Web Platform Tests
- `vcpkg.json` - Dependency manifest
- `.github/workflows/` - Actual CI workflows

## Resources

- [Main Documentation](SKILL.md)
- [GitHub Actions Reference](references/github-actions-reference.md)
- [CI Best Practices](references/ci-best-practices.md)
- [Caching Strategies](references/caching-strategies.md)
- [Troubleshooting Guide](references/troubleshooting-ci.md)

## Contributing

When adding new CI patterns:
1. Test locally with `act` (GitHub Actions local runner)
2. Document in `SKILL.md`
3. Add example workflow to `examples/`
4. Update this README

## License

This skill follows the Ladybird project license.
