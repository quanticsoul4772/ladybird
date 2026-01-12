# Caching Strategies for Ladybird CI

## Overview

Effective caching is **critical** for browser CI. A full Ladybird build can take 30+ minutes from scratch, but with proper caching, incremental builds complete in 5-10 minutes.

## Cache Layers

```
┌─────────────────────────────────────────┐
│  GitHub Actions Cache (10GB limit)     │
├─────────────────────────────────────────┤
│  ccache         │ Compiled objects     │ ← Fastest (90%+ hit rate)
│  (C++ compiler) │ (~1-2GB)             │
├─────────────────┼──────────────────────┤
│  vcpkg          │ Binary packages      │ ← Medium (reused across PRs)
│  (dependencies) │ (~3-5GB)             │
├─────────────────┼──────────────────────┤
│  Intermediate   │ Build artifacts      │ ← Slower (branch-specific)
│  Build Files    │ (~2-3GB)             │
└─────────────────┴──────────────────────┘
```

## 1. ccache (Compiler Cache)

### Purpose
Caches compiled object files to avoid recompiling unchanged source files.

### Configuration

```yaml
- name: Restore ccache
  uses: actions/cache@v4
  with:
    path: ~/.ccache
    # Hierarchical keys for fallback
    key: ccache-${{ runner.os }}-${{ matrix.compiler }}-${{ github.sha }}
    restore-keys: |
      ccache-${{ runner.os }}-${{ matrix.compiler }}-
      ccache-${{ runner.os }}-

- name: Configure ccache
  run: |
    # Set cache directory
    ccache --set-config=cache_dir=$HOME/.ccache

    # Size limit (adjust based on repo size)
    ccache --set-config=max_size=2G

    # Compression (saves GitHub Actions cache space)
    ccache --set-config=compression=true
    ccache --set-config=compression_level=6

    # Sloppiness settings (improve cache hit rate)
    ccache --set-config=sloppiness=time_macros,include_file_mtime,include_file_ctime

    # Compiler check (ignore plugin .so changes)
    ccache --set-config=compiler_check="%compiler% -v"

    # Hash PCH (precompiled headers)
    ccache --set-config=pch_external_checksum=true

    # Zero stats for this run
    ccache --zero-stats

- name: Build with ccache
  env:
    CCACHE_DIR: $HOME/.ccache
  run: cmake --build Build --parallel $(nproc)

- name: Print ccache Statistics
  run: |
    ccache --show-stats
    ccache --show-stats | grep -E "cache hit|cache miss"
```

### Best Practices

1. **Key Strategy**: Use hierarchical keys for fallback
   - Primary: `ccache-linux-gcc14-abc123` (unique per commit)
   - Fallback 1: `ccache-linux-gcc14-` (same OS + compiler)
   - Fallback 2: `ccache-linux-` (same OS)

2. **Size Management**: Monitor cache size
   ```bash
   # Check cache size
   ccache --show-stats | grep "cache size"

   # Clean if needed
   ccache --cleanup
   ccache --max-size 2G
   ```

3. **Compression**: Always enable for CI (saves GitHub cache space)

4. **Compiler Check**: Use `%compiler% -v` to ignore plugin changes

### Expected Performance

| Scenario | Cache Hit Rate | Build Time |
|----------|----------------|------------|
| Fresh PR (from master) | 80-90% | 8-12 min |
| Incremental change | 95-99% | 2-5 min |
| Full rebuild (cache miss) | 0% | 25-35 min |

## 2. vcpkg Binary Cache

### Purpose
Caches compiled dependencies (Qt, SQLite, etc.) to avoid rebuilding them.

### Configuration

```yaml
- name: Restore vcpkg Cache
  uses: actions/cache@v4
  with:
    path: ${{ github.workspace }}/Build/caches/vcpkg-binary-cache
    # Invalidate on dependency changes
    key: vcpkg-${{ runner.os }}-${{ hashFiles('vcpkg.json', 'vcpkg-configuration.json') }}
    restore-keys: |
      vcpkg-${{ runner.os }}-

- name: Configure vcpkg Binary Sources
  env:
    VCPKG_BINARY_SOURCES: "clear;files,${{ github.workspace }}/Build/caches/vcpkg-binary-cache,readwrite"
  run: |
    cmake --preset Release

- name: Build
  run: cmake --build Build
```

### Advanced: Azure Blob Storage (for large teams)

```yaml
- name: Configure vcpkg with Azure Cache
  env:
    # Read from Azure for all builds
    VCPKG_BINARY_SOURCES: "clear;files,${{ github.workspace }}/Build/caches/vcpkg-binary-cache,readwrite;x-azblob,https://vcpkg-cache.blob.core.windows.net/cache,${{ secrets.VCPKG_CACHE_SAS }},read"

    # Write to Azure only from master
    VCPKG_CACHE_MODE: ${{ github.ref == 'refs/heads/master' && 'write' || 'read' }}
  run: cmake --preset Release
```

### Best Practices

1. **Dependency Pinning**: Lock vcpkg versions in `vcpkg.json`
   ```json
   {
     "dependencies": [
       { "name": "qt5", "version>=": "5.15.10" }
     ],
     "builtin-baseline": "abc123def456"
   }
   ```

2. **Manifest Mode**: Use `vcpkg.json` (not `vcpkg install` commands)

3. **Binary Cache Strategy**:
   - **Read**: All PRs read from cache
   - **Write**: Only master/main branch writes to cache

### Expected Performance

| Scenario | Time to Install Dependencies |
|----------|------------------------------|
| Cache hit (all deps cached) | 30-60 seconds |
| Partial cache hit | 5-10 minutes |
| Cache miss (rebuild all) | 20-30 minutes |

## 3. Build Artifact Cache

### Purpose
Cache intermediate build files for faster incremental builds.

### Configuration

```yaml
- name: Restore Build Cache
  uses: actions/cache@v4
  with:
    path: |
      Build/
      !Build/**/*.o
      !Build/**/*.a
    key: build-${{ runner.os }}-${{ matrix.preset }}-${{ github.sha }}
    restore-keys: |
      build-${{ runner.os }}-${{ matrix.preset }}-
```

### Caution
- **Size**: Build directories can be large (5-10GB)
- **Usefulness**: Often superseded by ccache
- **When to Use**: For generated files (Moc, IDL bindings, etc.)

## 4. Dependency Source Cache

### Purpose
Cache downloaded source archives (vcpkg ports, git submodules).

### Configuration

```yaml
- name: Restore Download Cache
  uses: actions/cache@v4
  with:
    path: |
      Build/vcpkg/downloads/
      Build/caches/download-cache/
    key: downloads-${{ hashFiles('vcpkg.json') }}
```

### Benefits
- Faster vcpkg bootstrapping (no re-downloading source archives)
- Resilient to upstream source changes

## Cache Invalidation Strategies

### When to Invalidate

1. **Dependency Changes**
   ```yaml
   key: vcpkg-${{ hashFiles('vcpkg.json') }}
   ```

2. **Compiler Updates**
   ```yaml
   key: ccache-${{ matrix.compiler }}-${{ env.COMPILER_VERSION }}
   ```

3. **Build System Changes**
   ```yaml
   key: build-${{ hashFiles('CMakeLists.txt', 'CMakePresets.json') }}
   ```

4. **Scheduled Invalidation** (weekly cleanup)
   ```yaml
   key: ccache-${{ github.run_number / 1000 }}-  # Changes every ~1000 runs
   ```

### Automatic Cleanup

```yaml
- name: Clean Old Caches
  if: github.ref == 'refs/heads/master'
  uses: actions/github-script@v7
  with:
    script: |
      const caches = await github.rest.actions.getActionsCacheList({
        owner: context.repo.owner,
        repo: context.repo.repo,
      });

      // Delete caches older than 7 days
      const oneWeekAgo = Date.now() - (7 * 24 * 60 * 60 * 1000);
      for (const cache of caches.data.actions_caches) {
        if (new Date(cache.created_at) < oneWeekAgo) {
          await github.rest.actions.deleteActionsCacheById({
            owner: context.repo.owner,
            repo: context.repo.repo,
            cache_id: cache.id,
          });
        }
      }
```

## Cache Hit Rate Monitoring

### Tracking ccache Performance

```yaml
- name: ccache Statistics
  if: always()
  run: |
    echo "## ccache Statistics" >> $GITHUB_STEP_SUMMARY
    ccache --show-stats >> $GITHUB_STEP_SUMMARY

    # Extract hit rate
    HIT_RATE=$(ccache --show-stats | grep "cache hit rate" | awk '{print $4}')
    echo "Cache hit rate: $HIT_RATE" >> $GITHUB_STEP_SUMMARY

    # Alert if hit rate is low
    if (( $(echo "$HIT_RATE < 80" | bc -l) )); then
      echo "⚠️ Low cache hit rate: $HIT_RATE%" >> $GITHUB_STEP_SUMMARY
    fi
```

### Tracking GitHub Actions Cache Usage

```yaml
- name: Cache Usage Report
  uses: actions/github-script@v7
  with:
    script: |
      const caches = await github.rest.actions.getActionsCacheList({
        owner: context.repo.owner,
        repo: context.repo.repo,
      });

      let totalSize = 0;
      for (const cache of caches.data.actions_caches) {
        totalSize += cache.size_in_bytes;
      }

      const totalGB = (totalSize / (1024 ** 3)).toFixed(2);
      console.log(`Total cache size: ${totalGB} GB / 10 GB`);

      core.summary.addHeading('Cache Usage');
      core.summary.addTable([
        [{data: 'Metric', header: true}, {data: 'Value', header: true}],
        ['Total caches', caches.data.actions_caches.length],
        ['Total size', `${totalGB} GB`],
        ['Limit', '10 GB'],
        ['Usage', `${(totalGB / 10 * 100).toFixed(1)}%`]
      ]);
      await core.summary.write();
```

## Optimization Techniques

### 1. Compression Tuning

```bash
# Fastest compression (less CPU, larger cache)
ccache --set-config=compression_level=1

# Balanced (default)
ccache --set-config=compression_level=6

# Best compression (more CPU, smaller cache)
ccache --set-config=compression_level=9
```

### 2. Cache Warming

```yaml
# Pre-populate cache on master
- name: Warm Cache
  if: github.ref == 'refs/heads/master'
  run: |
    # Build all targets to populate cache
    cmake --build Build --target all

    # Force ccache to cache everything
    ccache --max-size=3G
```

### 3. Selective Caching

```yaml
# Don't cache test binaries (they change often)
- uses: actions/cache@v4
  with:
    path: Build/
    exclude: |
      Build/**/Test*
      Build/**/*.test
```

### 4. Multi-Level Fallback

```yaml
restore-keys: |
  ccache-${{ runner.os }}-${{ matrix.compiler }}-${{ github.base_ref }}-
  ccache-${{ runner.os }}-${{ matrix.compiler }}-master-
  ccache-${{ runner.os }}-${{ matrix.compiler }}-
  ccache-${{ runner.os }}-
```

## Troubleshooting

### Cache Not Restoring

```yaml
- name: Debug Cache
  run: |
    echo "Cache key: ccache-${{ runner.os }}-${{ matrix.compiler }}-${{ github.sha }}"
    echo "Restore keys:"
    echo "  - ccache-${{ runner.os }}-${{ matrix.compiler }}-"
    echo "  - ccache-${{ runner.os }}-"

    # Check if ccache directory exists
    if [ -d ~/.ccache ]; then
      echo "ccache directory found"
      du -sh ~/.ccache
    else
      echo "ccache directory not found"
    fi
```

### Low Cache Hit Rate

Possible causes:
1. **PCH not hashed**: Enable `pch_external_checksum`
2. **Timestamp changes**: Set `sloppiness=time_macros,include_file_mtime`
3. **Plugin changes**: Use `compiler_check="%compiler% -v"`
4. **Cache too small**: Increase `max_size`

### GitHub Cache Full (10GB limit)

```yaml
- name: Check Cache Size
  run: |
    # Show cache sizes
    du -sh ~/.ccache
    du -sh Build/caches/vcpkg-binary-cache

    # Clean if needed
    ccache --evict-older-than 7d
```

## References

- [ccache Manual](https://ccache.dev/manual/latest.html)
- [vcpkg Binary Caching](https://github.com/microsoft/vcpkg/blob/master/docs/users/binarycaching.md)
- [GitHub Actions Cache](https://docs.github.com/en/actions/using-workflows/caching-dependencies-to-speed-up-workflows)
