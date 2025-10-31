# Sentinel Phase 5 Day 33 - Error Resilience

**Date**: Ready for Implementation
**Duration**: 6 hours (parallel agents: 4-5 hours)
**Focus**: Graceful degradation and operational resilience
**Status**: 📋 PLANNED

---

## Overview

Day 33 focuses on making the Sentinel system resilient to failures and capable of graceful degradation. When external dependencies fail (database, YARA scanner, network), the system should continue operating in a degraded but functional state rather than crashing.

---

## Issues to Resolve

### ISSUE-020: No Graceful Degradation (MEDIUM)

**Priority**: MEDIUM
**Effort**: 2 hours
**Impact**: Service availability during partial failures

**Current Behavior**:
- PolicyGraph database failure → entire system unusable
- YARA scanner crash → all downloads blocked
- IPC connection loss → browser hangs
- Network failure → no fallback

**Desired Behavior**:
- Database failure → use in-memory cache, warn user
- YARA failure → allow downloads with warning
- IPC failure → graceful reconnection
- Network failure → queue operations for retry

**Implementation Plan**:
```cpp
// Services/Sentinel/FallbackManager.h
class FallbackManager {
public:
    enum class ServiceState {
        Healthy,      // Service operating normally
        Degraded,     // Service operating with reduced functionality
        Failed,       // Service unavailable, using fallback
        Critical      // Fallback also failed, system unstable
    };

    // Register service health status
    void set_service_state(StringView service_name, ServiceState state, StringView reason);

    // Query current system state
    ServiceState get_system_state() const;
    Vector<String> get_degraded_services() const;

    // Fallback behavior
    bool should_use_fallback(StringView service_name) const;
    Optional<String> get_fallback_reason(StringView service_name) const;

private:
    HashMap<String, ServiceState> m_service_states;
    HashMap<String, String> m_failure_reasons;
    UnixDateTime m_last_state_change;
};
```

**Integration Points**:
- PolicyGraph: Fallback to in-memory policy cache
- YARAScanWorkerPool: Fallback to allow downloads with warning
- SecurityTap: Fallback to bypass mode (log but don't block)
- IPC: Fallback to reconnection attempts

**User Experience**:
```
┌─────────────────────────────────────────────────┐
│ ⚠️  Sentinel Operating in Degraded Mode         │
│                                                 │
│ Database connection lost. Using cached         │
│ security policies. Some features unavailable.  │
│                                                 │
│ [Retry Connection] [Continue with Cache]       │
└─────────────────────────────────────────────────┘
```

**Test Cases** (10 tests):
1. Database connection failure
2. YARA scanner crash during scan
3. IPC connection loss
4. Network timeout
5. Policy cache miss during degraded mode
6. Recovery from degraded mode
7. Multiple simultaneous failures
8. Cascade failure prevention
9. Fallback notification to UI
10. State persistence across restarts

---

### ISSUE-021: No Circuit Breakers (MEDIUM)

**Priority**: MEDIUM
**Effort**: 1.5 hours
**Impact**: Prevent cascade failures

**Current Behavior**:
- Failed service continuously retried → resource exhaustion
- Slow service continuously called → latency accumulation
- External API failures → timeout storm
- Database overload → query pileup

**Desired Behavior**:
- Circuit opens after N failures → stop calls temporarily
- Half-open state → test with single request
- Circuit closes after successful test → resume normal operation
- Metrics tracked → dashboards show circuit states

**Implementation Plan**:
```cpp
// Libraries/LibCore/CircuitBreaker.h
class CircuitBreaker {
public:
    enum class State {
        Closed,     // Normal operation
        Open,       // Failing, reject requests
        HalfOpen    // Testing recovery
    };

    CircuitBreaker(
        size_t failure_threshold,           // Open after N failures
        Duration timeout,                   // Time before trying again
        size_t success_threshold_to_close   // Successes needed to close
    );

    // Execute with circuit breaker protection
    template<typename Func>
    ErrorOr<decltype(declval<Func>()())> execute(Func&& func);

    // Manual state control
    void trip();              // Force open
    void reset();             // Force closed

    // Metrics
    State state() const { return m_state; }
    size_t failure_count() const { return m_failure_count; }
    size_t success_count() const { return m_success_count; }
    UnixDateTime last_failure_time() const { return m_last_failure_time; }

private:
    State m_state { State::Closed };
    size_t m_failure_threshold;
    size_t m_success_threshold;
    Duration m_timeout;

    size_t m_failure_count { 0 };
    size_t m_success_count { 0 };
    UnixDateTime m_last_failure_time;
    UnixDateTime m_state_changed_time;

    mutable Mutex m_mutex;

    void on_success();
    void on_failure();
    bool should_attempt() const;
};
```

**Circuit Breaker Locations**:
1. **Database Queries**: PolicyGraph operations
   - Threshold: 5 failures
   - Timeout: 30 seconds
   - Success needed: 2 consecutive

2. **YARA Scanning**: Worker pool operations
   - Threshold: 3 failures
   - Timeout: 60 seconds
   - Success needed: 3 consecutive

3. **IPC Communications**: Client connections
   - Threshold: 10 failures
   - Timeout: 10 seconds
   - Success needed: 1 success

4. **External APIs**: Future integrations
   - Threshold: 3 failures
   - Timeout: 60 seconds
   - Success needed: 2 consecutive

**Metrics Dashboard**:
```
Circuit Breaker Status:
┌─────────────────────────────────────────────┐
│ Service          State    Failures  Uptime  │
├─────────────────────────────────────────────┤
│ Database         CLOSED   0         100%    │
│ YARA Scanner     CLOSED   0         100%    │
│ IPC Client       HALF_OPEN 8        95%     │
│ External API     OPEN     15        ERROR   │
└─────────────────────────────────────────────┘
```

**Test Cases** (8 tests):
1. Circuit trips after threshold failures
2. Circuit stays open during timeout
3. Circuit transitions to half-open after timeout
4. Half-open success closes circuit
5. Half-open failure reopens circuit
6. Concurrent requests during half-open
7. Circuit metrics accuracy
8. Manual circuit control

---

### ISSUE-022: No Health Checks (LOW)

**Priority**: LOW
**Effort**: 1 hour
**Impact**: System observability and monitoring

**Current Behavior**:
- No health status endpoint
- No service dependency checks
- No readiness/liveness separation
- No metrics exposure

**Desired Behavior**:
- `/health` endpoint → overall system health
- `/health/live` → liveness probe (is process alive?)
- `/health/ready` → readiness probe (can handle requests?)
- `/metrics` → Prometheus-compatible metrics

**Implementation Plan**:
```cpp
// Services/Sentinel/HealthCheck.h
class HealthCheck {
public:
    enum class Status {
        Healthy,     // All systems operational
        Degraded,    // Some services degraded
        Unhealthy    // Critical services failed
    };

    struct ComponentHealth {
        String component_name;
        Status status;
        Optional<String> message;
        UnixDateTime last_check;
        Duration response_time;
    };

    // Register health check callback
    using HealthCheckFunc = Function<ErrorOr<ComponentHealth>()>;
    void register_check(String component_name, HealthCheckFunc callback);

    // Perform health checks
    ErrorOr<Status> check_health();
    ErrorOr<Vector<ComponentHealth>> check_all_components();

    // Liveness vs Readiness
    bool is_alive() const;     // Process is running
    bool is_ready() const;     // Can handle requests

    // Metrics
    HashMap<String, i64> get_metrics() const;

private:
    HashMap<String, HealthCheckFunc> m_health_checks;
    Vector<ComponentHealth> m_last_results;
    UnixDateTime m_last_full_check;
};
```

**Health Check Components**:
1. **Database Connectivity**
   - Check: `SELECT 1` query
   - Timeout: 1 second
   - Failure: Mark degraded

2. **YARA Scanner**
   - Check: Test scan small buffer
   - Timeout: 2 seconds
   - Failure: Mark degraded

3. **IPC Server**
   - Check: Count active connections
   - Threshold: <1000 connections
   - Failure: Mark degraded (overload)

4. **Disk Space**
   - Check: /tmp and /home available
   - Threshold: >1GB free
   - Failure: Mark unhealthy

5. **Memory Usage**
   - Check: Process RSS
   - Threshold: <2GB
   - Failure: Mark degraded

**HTTP Health Endpoints** (via IPC):
```
GET /health
{
  "status": "healthy",
  "timestamp": "2025-10-30T12:34:56Z",
  "uptime_seconds": 86400,
  "components": {
    "database": { "status": "healthy", "response_ms": 5 },
    "yara": { "status": "healthy", "response_ms": 10 },
    "ipc": { "status": "healthy", "connections": 42 },
    "disk": { "status": "healthy", "free_gb": 50 },
    "memory": { "status": "healthy", "rss_mb": 512 }
  }
}

GET /health/live
{ "alive": true }

GET /health/ready
{ "ready": true, "reason": null }

GET /metrics
sentinel_uptime_seconds 86400
sentinel_database_query_duration_seconds 0.005
sentinel_yara_scan_duration_seconds 0.010
sentinel_ipc_connections_active 42
sentinel_disk_free_bytes{mount="/tmp"} 53687091200
sentinel_memory_rss_bytes 536870912
```

**Test Cases** (7 tests):
1. All components healthy
2. Database unhealthy
3. Multiple components degraded
4. Liveness check always returns true
5. Readiness check fails when database down
6. Metrics endpoint format
7. Health check timeout handling

---

### ISSUE-023: No Retry Logic (LOW)

**Priority**: LOW
**Effort**: 1.5 hours
**Impact**: Resilience to transient failures

**Current Behavior**:
- Database query fails → immediate error
- IPC send fails → message lost
- File operation fails → operation abandoned
- Network request fails → no retry

**Desired Behavior**:
- Exponential backoff retry
- Jittered delays (avoid thundering herd)
- Maximum retry limit
- Configurable per operation type

**Implementation Plan**:
```cpp
// Libraries/LibCore/RetryPolicy.h
class RetryPolicy {
public:
    RetryPolicy(
        size_t max_attempts = 3,
        Duration initial_delay = Duration::from_milliseconds(100),
        Duration max_delay = Duration::from_seconds(10),
        double backoff_multiplier = 2.0,
        double jitter_factor = 0.1
    );

    // Execute with retry
    template<typename Func>
    ErrorOr<decltype(declval<Func>()())> execute(Func&& func);

    // Check if should retry this error
    bool should_retry(Error const& error) const;

    // Calculate next delay (exponential backoff + jitter)
    Duration calculate_next_delay(size_t attempt) const;

    // Reset retry state
    void reset();

private:
    size_t m_max_attempts;
    Duration m_initial_delay;
    Duration m_max_delay;
    double m_backoff_multiplier;
    double m_jitter_factor;

    size_t m_current_attempt { 0 };
    UnixDateTime m_last_attempt;
};
```

**Retry Strategies by Operation**:

1. **Database Queries** (Transient errors only)
   - Max attempts: 3
   - Initial delay: 100ms
   - Max delay: 1 second
   - Backoff: 2x
   - Retryable: Connection errors, lock timeouts
   - Non-retryable: Constraint violations, syntax errors

2. **IPC Messages** (Connection errors)
   - Max attempts: 5
   - Initial delay: 50ms
   - Max delay: 2 seconds
   - Backoff: 1.5x
   - Retryable: Connection refused, timeout
   - Non-retryable: Protocol errors

3. **File Operations** (I/O errors)
   - Max attempts: 3
   - Initial delay: 200ms
   - Max delay: 5 seconds
   - Backoff: 2x
   - Retryable: EAGAIN, EBUSY
   - Non-retryable: ENOENT, EACCES, ENOSPC

4. **YARA Scanning** (Resource exhaustion)
   - Max attempts: 2
   - Initial delay: 1 second
   - Max delay: 5 seconds
   - Backoff: 3x
   - Retryable: Memory allocation failures
   - Non-retryable: Invalid rules

**Example Usage**:
```cpp
// Database query with retry
RetryPolicy db_retry_policy(
    /* max_attempts */ 3,
    /* initial_delay */ Duration::from_milliseconds(100),
    /* max_delay */ Duration::from_seconds(1)
);

auto result = TRY(db_retry_policy.execute([&] {
    return m_database->query("SELECT * FROM policies WHERE hash = ?", hash);
}));

// IPC message with retry
RetryPolicy ipc_retry_policy(
    /* max_attempts */ 5,
    /* initial_delay */ Duration::from_milliseconds(50),
    /* max_delay */ Duration::from_seconds(2),
    /* backoff_multiplier */ 1.5
);

TRY(ipc_retry_policy.execute([&] {
    return client.send_message(message);
}));
```

**Exponential Backoff Calculation**:
```
Attempt 1: delay = 100ms * (1 + random(0.1)) = 100-110ms
Attempt 2: delay = 200ms * (1 + random(0.1)) = 200-220ms
Attempt 3: delay = 400ms * (1 + random(0.1)) = 400-440ms
Attempt 4: delay = 800ms * (1 + random(0.1)) = 800-880ms
Attempt 5: delay = min(1600ms, max_delay) * (1 + random(0.1))
```

**Test Cases** (10 tests):
1. Success on first attempt (no retry)
2. Success on second attempt (1 retry)
3. Failure after max attempts
4. Exponential backoff delays verified
5. Jitter adds randomness
6. Max delay enforced
7. Retryable vs non-retryable errors
8. Reset retry state
9. Concurrent retries don't interfere
10. Retry metrics tracking

---

## Implementation Strategy

### Parallel Agent Approach

Launch 4 agents in parallel:

**Agent 1**: Graceful Degradation (2 hours)
- Create FallbackManager class
- Integrate with PolicyGraph, YARA, IPC
- UI notifications for degraded mode
- 10 test cases

**Agent 2**: Circuit Breakers (1.5 hours)
- Create CircuitBreaker class
- Add circuit breakers to critical paths
- Metrics and monitoring
- 8 test cases

**Agent 3**: Health Checks (1 hour)
- Create HealthCheck class
- Register component health checks
- HTTP endpoints via IPC
- 7 test cases

**Agent 4**: Retry Logic (1.5 hours)
- Create RetryPolicy class
- Apply to database, IPC, file operations
- Exponential backoff with jitter
- 10 test cases

**Total parallel time**: ~2 hours (agents complete concurrently)
**Sequential integration**: 30 minutes
**Testing and validation**: 1 hour
**Documentation**: 30 minutes

---

## Files to Create

### Implementation
```
Libraries/LibCore/CircuitBreaker.cpp           (350 lines)
Libraries/LibCore/CircuitBreaker.h             (120 lines)
Libraries/LibCore/RetryPolicy.cpp              (280 lines)
Libraries/LibCore/RetryPolicy.h                (100 lines)
Services/Sentinel/FallbackManager.cpp          (420 lines)
Services/Sentinel/FallbackManager.h            (150 lines)
Services/Sentinel/HealthCheck.cpp              (380 lines)
Services/Sentinel/HealthCheck.h                (140 lines)
```

### Tests
```
Tests/LibCore/TestCircuitBreaker.cpp           (300 lines)
Tests/LibCore/TestRetryPolicy.cpp              (350 lines)
Tests/Sentinel/TestFallbackManager.cpp         (400 lines)
Tests/Sentinel/TestHealthCheck.cpp             (250 lines)
```

### Documentation
```
docs/SENTINEL_DAY33_COMPLETE.md                (1,500 lines)
docs/SENTINEL_DAY33_TASK1_FALLBACK.md         (800 lines)
docs/SENTINEL_DAY33_TASK2_CIRCUIT_BREAKERS.md (700 lines)
docs/SENTINEL_DAY33_TASK3_HEALTH_CHECKS.md    (600 lines)
docs/SENTINEL_DAY33_TASK4_RETRY_LOGIC.md      (700 lines)
```

**Total new code**: ~3,800 lines
**Total tests**: ~1,300 lines
**Total documentation**: ~4,300 lines

---

## Integration Points

### With Existing Code

1. **PolicyGraph** (Services/Sentinel/PolicyGraph.cpp)
   - Add circuit breaker around database queries
   - Add retry logic for transient failures
   - Register with FallbackManager
   - Implement health check

2. **YARAScanWorkerPool** (Services/RequestServer/YARAScanWorkerPool.cpp)
   - Add circuit breaker around worker operations
   - Add fallback to "allow with warning"
   - Register with FallbackManager
   - Implement health check

3. **ConnectionFromClient** (Services/RequestServer/ConnectionFromClient.cpp)
   - Add retry logic for IPC sends
   - Add circuit breaker for client connections
   - Register with FallbackManager
   - Implement health check

4. **SentinelServer** (Services/Sentinel/SentinelServer.cpp)
   - Integrate HealthCheck endpoints
   - Coordinate FallbackManager state
   - Expose metrics endpoint
   - Implement graceful shutdown

### With Configuration (Day 34)

All resilience parameters will be configurable:
```ini
[Resilience]
enable_circuit_breakers = true
circuit_breaker_threshold = 5
circuit_breaker_timeout_seconds = 30

enable_retry_logic = true
max_retry_attempts = 3
retry_initial_delay_ms = 100
retry_max_delay_ms = 1000

enable_health_checks = true
health_check_interval_seconds = 30

enable_graceful_degradation = true
degraded_mode_notification = true
```

---

## Success Criteria

### Functional Requirements
- ✅ System continues operating when database unavailable
- ✅ Circuit breakers prevent cascade failures
- ✅ Health checks expose system status
- ✅ Retry logic handles transient failures
- ✅ Graceful degradation maintains user experience

### Non-Functional Requirements
- ✅ Overhead: <5% performance impact
- ✅ Latency: <10ms additional per operation
- ✅ Memory: <50MB additional
- ✅ Recovery: <30 seconds to resume normal operation

### Testing Requirements
- ✅ 35+ test cases pass
- ✅ ASAN/UBSAN clean
- ✅ No memory leaks
- ✅ Stress testing under failure conditions

---

## Estimated Metrics

### Before Day 33
- System availability: 95% (single points of failure)
- Recovery time: Manual intervention required
- Failure visibility: Limited logging only
- Cascade failures: Possible

### After Day 33
- System availability: 99.9% (graceful degradation)
- Recovery time: <30 seconds (automatic)
- Failure visibility: Real-time health checks
- Cascade failures: Prevented by circuit breakers

---

## Risk Assessment

### Implementation Risks

**Low Risk** ✅:
- Circuit breaker pattern: Well-established pattern
- Retry logic: Standard approach
- Health checks: Simple implementation

**Medium Risk** ⚠️:
- Graceful degradation: Complex state management
- Integration: Touching many existing components
- Testing: Need realistic failure scenarios

### Mitigation
- Comprehensive unit tests for each component
- Integration tests for failure scenarios
- Gradual rollout with feature flags
- Extensive documentation

---

## Dependencies

### Requires (from previous days)
- ErrorOr<T> pattern (Day 30)
- Proper error propagation (Day 30)
- Audit logging (Day 32)
- Rate limiting (Day 32)

### Required by (future days)
- Configuration system (Day 34)
- Production deployment (Day 35)
- Monitoring dashboards (Phase 6)

---

**Document Version**: 1.0
**Status**: 📋 READY FOR IMPLEMENTATION
**Estimated Duration**: 6 hours (4-5 hours with parallel agents)
**Prerequisites**: Days 29-32 complete ✅

