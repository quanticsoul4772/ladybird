# Sentinel Phase 5 Day 31: Implementation Checklist

**Purpose**: Step-by-step checklist for completing Day 31 deliverables
**Date**: 2025-10-30

---

## Pre-Integration Checklist

### Files Created ✓
- [x] `/home/rbsmith4/ladybird/Tools/benchmark_sentinel_full.cpp` (269 lines)
- [x] `/home/rbsmith4/ladybird/Tools/benchmark_policydb.cpp` (448 lines)
- [x] `/home/rbsmith4/ladybird/Tools/CMakeLists.txt` (build configuration)
- [x] `/home/rbsmith4/ladybird/Tools/README.md` (usage documentation)
- [x] `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraphCacheMetrics.cpp` (reference impl)
- [x] `/home/rbsmith4/ladybird/docs/SENTINEL_PHASE5_DAY31_PERFORMANCE.md` (919 lines)
- [x] `/home/rbsmith4/ladybird/docs/SENTINEL_PHASE5_DAY31_COMPLETION.md` (360 lines)
- [x] `/home/rbsmith4/ladybird/docs/SENTINEL_PHASE5_DAY31_CHECKLIST.md` (this file)

### Files Modified ✓
- [x] `/home/rbsmith4/ladybird/Services/Sentinel/PolicyGraph.h`
  - [x] Added `CacheMetrics` struct with hit/miss/eviction counters
  - [x] Added metric tracking members to `PolicyGraphCache`
  - [x] Added `get_metrics()` and `reset_metrics()` methods
  - [x] Added public API in `PolicyGraph` class

---

## Integration Steps (To Be Completed)

### 1. PolicyGraph Cache Metrics Implementation

**File**: `Services/Sentinel/PolicyGraph.cpp`

**Status**: ⧗ Pending manual integration

**Steps**:

#### 1.1 Update `get_cached()` method

Location: Lines 17-27

**Current code**:
```cpp
Optional<Optional<int>> PolicyGraphCache::get_cached(String const& key)
{
    auto it = m_cache.find(key);
    if (it == m_cache.end())
        return {};

    // Update LRU order
    update_lru(key);

    return it->value;
}
```

**Replace with**:
```cpp
Optional<Optional<int>> PolicyGraphCache::get_cached(String const& key)
{
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        m_cache_misses++;
        return {};
    }

    // Cache hit
    m_cache_hits++;

    // Update LRU order
    update_lru(key);

    return it->value;
}
```

**Changes**:
- [ ] Add `m_cache_misses++` on cache miss
- [ ] Add `m_cache_hits++` on cache hit

#### 1.2 Update `cache_policy()` method

Location: Lines 29-41

**Current code**:
```cpp
void PolicyGraphCache::cache_policy(String const& key, Optional<int> policy_id)
{
    // If cache is full, evict least recently used entry
    if (m_cache.size() >= m_max_size && !m_cache.contains(key)) {
        if (!m_lru_order.is_empty()) {
            auto lru_key = m_lru_order.take_first();
            m_cache.remove(lru_key);
        }
    }

    m_cache.set(key, policy_id);
    update_lru(key);
}
```

**Replace with**:
```cpp
void PolicyGraphCache::cache_policy(String const& key, Optional<int> policy_id)
{
    // If cache is full, evict least recently used entry
    if (m_cache.size() >= m_max_size && !m_cache.contains(key)) {
        if (!m_lru_order.is_empty()) {
            auto lru_key = m_lru_order.take_first();
            m_cache.remove(lru_key);
            m_cache_evictions++;
        }
    }

    m_cache.set(key, policy_id);
    update_lru(key);
}
```

**Changes**:
- [ ] Add `m_cache_evictions++` when LRU eviction occurs

#### 1.3 Update `invalidate()` method

Location: Lines 43-47

**Current code**:
```cpp
void PolicyGraphCache::invalidate()
{
    m_cache.clear();
    m_lru_order.clear();
}
```

**Replace with**:
```cpp
void PolicyGraphCache::invalidate()
{
    m_cache.clear();
    m_lru_order.clear();
    m_cache_invalidations++;
}
```

**Changes**:
- [ ] Add `m_cache_invalidations++`

#### 1.4 Add new methods

Location: After `update_lru()` (after line 56)

**Add these methods**:
```cpp
PolicyGraphCache::CacheMetrics PolicyGraphCache::get_metrics() const
{
    return CacheMetrics {
        .hits = m_cache_hits,
        .misses = m_cache_misses,
        .evictions = m_cache_evictions,
        .invalidations = m_cache_invalidations,
        .current_size = m_cache.size(),
        .max_size = m_max_size
    };
}

void PolicyGraphCache::reset_metrics()
{
    m_cache_hits = 0;
    m_cache_misses = 0;
    m_cache_evictions = 0;
    m_cache_invalidations = 0;
}
```

**Changes**:
- [ ] Add `get_metrics()` implementation
- [ ] Add `reset_metrics()` implementation

**Reference**: See `Services/Sentinel/PolicyGraphCacheMetrics.cpp` for complete implementation.

### 2. Build System Integration

**File**: Main project `CMakeLists.txt` or `Services/CMakeLists.txt`

**Status**: ⧗ Pending

**Steps**:

- [ ] Add `Tools/CMakeLists.txt` to project
- [ ] OR copy benchmark build rules to appropriate CMakeLists.txt
- [ ] Verify RequestServer linkage for benchmark_sentinel_full
- [ ] Verify Sentinel linkage for benchmark_policydb
- [ ] Build in Release mode for accurate benchmarks

**Build commands**:
```bash
cd Build
cmake .. -DCMAKE_BUILD_TYPE=Release
make benchmark_sentinel_full
make benchmark_policydb
```

### 3. Benchmark Execution

#### 3.1 Setup Test Environment

**Prerequisites**:
- [ ] Sentinel daemon compiled
- [ ] YARA rules available
- [ ] Database directory writable

**Setup commands**:
```bash
# Create necessary directories
mkdir -p ~/.config/ladybird/sentinel/rules
mkdir -p ~/.local/share/Ladybird/PolicyGraph

# Start Sentinel daemon
./Build/bin/Sentinel &

# Verify socket
ls -l /tmp/sentinel.sock
```

#### 3.2 Run Download Path Benchmark

**Steps**:
- [ ] Run baseline (hash only): `./benchmark_sentinel_full --skip-sentinel`
- [ ] Save baseline results
- [ ] Start Sentinel daemon
- [ ] Run full benchmark: `./benchmark_sentinel_full`
- [ ] Save full results
- [ ] Compare overhead vs. 5% target

**Output location**: `results_sentinel_baseline.txt`, `results_sentinel_full.txt`

#### 3.3 Run Database Benchmark

**Steps**:
- [ ] Run with default data: `./benchmark_policydb --cleanup`
- [ ] Save results
- [ ] Run with large dataset: `./benchmark_policydb --policies 5000 --threats 10000 --cleanup`
- [ ] Save large dataset results
- [ ] Verify all targets met

**Output location**: `results_db_default.txt`, `results_db_large.txt`

#### 3.4 Collect Cache Metrics

**Implementation needed**:
```cpp
// Example: Add to about:security page or sentinel-cli tool
auto metrics = policy_graph.get_cache_metrics();
outln("Cache Statistics:");
outln("  Hit rate: {:.1f}%", metrics.hit_rate());
outln("  Hits: {}", metrics.hits);
outln("  Misses: {}", metrics.misses);
outln("  Evictions: {}", metrics.evictions);
outln("  Invalidations: {}", metrics.invalidations);
outln("  Size: {}/{}", metrics.current_size, metrics.max_size);
```

**Steps**:
- [ ] Add cache metrics display to sentinel-cli or debug interface
- [ ] Run realistic workload
- [ ] Collect metrics after 1000 operations
- [ ] Verify hit rate > 80%

### 4. Results Analysis

**Steps**:
- [ ] Update `docs/SENTINEL_PHASE5_DAY31_PERFORMANCE.md` section 7.3 with actual results
- [ ] Compare actual vs. expected performance
- [ ] Identify discrepancies from predictions
- [ ] Prioritize optimizations based on actual bottlenecks
- [ ] Update optimization roadmap if needed

### 5. Documentation Updates

**Steps**:
- [ ] Fill in actual benchmark results in performance report
- [ ] Update bottleneck analysis if predictions were wrong
- [ ] Revise optimization priorities based on real data
- [ ] Add any new findings or insights
- [ ] Update completion summary with final status

---

## Verification Checklist

### Code Quality
- [ ] Cache metrics code compiles without warnings
- [ ] Benchmark code compiles without warnings
- [ ] No memory leaks (run with ASAN)
- [ ] Follows Ladybird code style
- [ ] Proper error handling in benchmarks

### Functionality
- [ ] Cache hits tracked correctly
- [ ] Cache misses tracked correctly
- [ ] Evictions tracked correctly
- [ ] Invalidations tracked correctly
- [ ] get_metrics() returns accurate data
- [ ] reset_metrics() clears counters

### Benchmarks
- [ ] benchmark_sentinel_full produces valid output
- [ ] benchmark_policydb produces valid output
- [ ] Warmup runs execute correctly
- [ ] Statistical measurements accurate
- [ ] Pass/fail criteria work correctly

### Documentation
- [ ] All files have proper copyright headers
- [ ] Usage instructions are clear
- [ ] Performance targets documented
- [ ] Optimization recommendations actionable
- [ ] Examples work as shown

---

## Optimization Implementation Checklist (Future)

### P1: High Priority

#### YARA Rule Optimization
- [ ] Profile individual YARA rules
- [ ] Remove low-value rules
- [ ] Optimize rule patterns
- [ ] Consider rule compilation caching
- [ ] Benchmark improvement
- [ ] Target: 30-50% reduction in scan time

#### Binary IPC Protocol
- [ ] Design binary message format
- [ ] Implement serialization/deserialization
- [ ] Replace JSON encoding
- [ ] Test with various file sizes
- [ ] Benchmark improvement
- [ ] Target: 25-35% reduction in IPC time

### P2: Medium Priority

#### Granular Cache Invalidation
- [ ] Design policy-to-cache-key mapping
- [ ] Implement selective invalidation
- [ ] Update all policy modification paths
- [ ] Test cache consistency
- [ ] Benchmark hit rate improvement
- [ ] Target: Maintain 80% hit rate after updates

#### Cache Warming
- [ ] Implement warm_cache() method
- [ ] Call on startup
- [ ] Load recent threats
- [ ] Pre-populate cache
- [ ] Benchmark cold start improvement
- [ ] Target: 70-80% initial hit rate

### P3: Low Priority

#### Async IPC Integration
- [ ] Update download flow to use async_inspect_download
- [ ] Implement callback handling
- [ ] Test concurrent downloads
- [ ] Benchmark concurrency improvement
- [ ] Target: Better multi-download performance

#### Shared Memory IPC
- [ ] Design shared memory protocol
- [ ] Implement for large files (> 1MB)
- [ ] Update Sentinel to use shm
- [ ] Test and benchmark
- [ ] Target: 50-70% IPC time reduction for large files

---

## Testing Checklist

### Unit Tests (Future)
- [ ] Test CacheMetrics struct
- [ ] Test get_metrics() accuracy
- [ ] Test reset_metrics()
- [ ] Test cache behavior under load
- [ ] Test eviction tracking
- [ ] Test invalidation tracking

### Integration Tests (Future)
- [ ] Test full download path with metrics
- [ ] Test PolicyGraph with metrics enabled
- [ ] Test benchmark tools end-to-end
- [ ] Test with various workloads
- [ ] Test error handling

### Performance Regression Tests (Future)
- [ ] Add benchmarks to CI/CD
- [ ] Set performance thresholds
- [ ] Alert on regressions
- [ ] Track metrics over time
- [ ] Compare across versions

---

## Sign-Off Checklist

### Development
- [ ] All code implemented
- [ ] Cache metrics integrated into PolicyGraph.cpp
- [ ] Benchmarks build successfully
- [ ] All compiler warnings resolved
- [ ] Code reviewed

### Testing
- [ ] Benchmarks executed
- [ ] Results collected
- [ ] Performance targets evaluated
- [ ] Cache metrics validated
- [ ] No critical bugs found

### Documentation
- [ ] Performance report complete with actual data
- [ ] Completion summary updated
- [ ] Usage documentation clear
- [ ] Optimization roadmap finalized
- [ ] All examples tested

### Deployment
- [ ] Build system integrated
- [ ] Benchmarks accessible
- [ ] Documentation published
- [ ] Team notified
- [ ] Ready for Day 32

---

## Quick Reference: Manual Integration Commands

```bash
# 1. Edit PolicyGraph.cpp to add cache metrics
vim Services/Sentinel/PolicyGraph.cpp
# Use PolicyGraphCacheMetrics.cpp as reference

# 2. Build benchmarks
cd Build
cmake .. -DCMAKE_BUILD_TYPE=Release
make benchmark_sentinel_full benchmark_policydb

# 3. Run benchmarks
./bin/benchmark_sentinel_full --skip-sentinel > baseline.txt
./bin/Sentinel &
./bin/benchmark_sentinel_full > fullpath.txt
./bin/benchmark_policydb --cleanup > database.txt

# 4. Analyze results
diff baseline.txt fullpath.txt
grep "PASS\|FAIL" database.txt

# 5. Update documentation
vim docs/SENTINEL_PHASE5_DAY31_PERFORMANCE.md
# Fill in section 7.3 with actual results
```

---

## Success Criteria

Day 31 is complete when:

1. **Code Integration**: ✓ (Pending PolicyGraph.cpp)
   - [x] Cache metrics implemented in header
   - [ ] Cache metrics implemented in .cpp
   - [x] Benchmark tools created
   - [ ] Build system integrated

2. **Benchmark Execution**: ⧗ Pending
   - [ ] Download path benchmark run
   - [ ] Database benchmark run
   - [ ] Cache metrics collected
   - [ ] Results documented

3. **Analysis**: ⧗ Pending
   - [ ] Bottlenecks identified
   - [ ] Performance targets evaluated
   - [ ] Optimization priorities set
   - [ ] Recommendations actionable

4. **Documentation**: ✓ Complete
   - [x] Performance report created
   - [x] Completion summary created
   - [x] Usage documentation created
   - [ ] Updated with actual results

**Current Status**: 75% Complete
- Code creation: 100%
- Code integration: 75% (header done, .cpp pending)
- Benchmark execution: 0% (pending build)
- Documentation: 90% (pending results)

**Estimated Time to Complete**: 2-4 hours
- Integration: 30 minutes
- Building: 15 minutes
- Execution: 30 minutes
- Analysis: 1-2 hours
- Documentation: 30 minutes

---

**Document Version**: 1.0
**Last Updated**: 2025-10-30
**Status**: Active Checklist
**Next Review**: After benchmark execution
