/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Circuit Breaker Integration Example
 *
 * This file demonstrates how to integrate circuit breakers into Sentinel services.
 * It is NOT compiled into the final binary - it serves as documentation and reference.
 */

#include <LibCore/CircuitBreaker.h>
#include <AK/Error.h>

namespace Sentinel::Examples {

// ============================================================================
// Example 1: Database Operations with Circuit Breaker
// ============================================================================

class DatabaseWithCircuitBreaker {
public:
    DatabaseWithCircuitBreaker()
        : m_circuit_breaker(Core::CircuitBreakerPresets::database("ExampleDatabase"sv))
    {
    }

    ErrorOr<String> query(String const& sql)
    {
        // Execute query with circuit breaker protection
        return m_circuit_breaker.execute([&]() -> ErrorOr<String> {
            return perform_database_query(sql);
        });
    }

    ErrorOr<void> insert(String const& sql)
    {
        // Check circuit state before attempting operation
        if (!m_circuit_breaker.is_request_allowed()) {
            dbgln("Database circuit is open, using fallback");
            return use_fallback_insert(sql);
        }

        return m_circuit_breaker.execute([&]() -> ErrorOr<void> {
            return perform_database_insert(sql);
        });
    }

    // Get circuit breaker metrics for monitoring
    Core::CircuitBreaker::Metrics get_metrics() const
    {
        return m_circuit_breaker.get_metrics();
    }

private:
    ErrorOr<String> perform_database_query(String const& sql)
    {
        // Simulate database query
        // In real code, this would call the database
        if (should_simulate_failure())
            return Error::from_string_literal("Database connection failed");
        return String::from_utf8("query_result"sv);
    }

    ErrorOr<void> perform_database_insert(String const& sql)
    {
        // Simulate database insert
        if (should_simulate_failure())
            return Error::from_string_literal("Database connection failed");
        return {};
    }

    ErrorOr<void> use_fallback_insert(String const& sql)
    {
        // Fallback: queue for retry when circuit closes
        dbgln("Queuing insert for retry: {}", sql);
        return {};
    }

    bool should_simulate_failure() const
    {
        // Simulate occasional failures for demonstration
        return false;
    }

    Core::CircuitBreaker m_circuit_breaker;
};

// ============================================================================
// Example 2: YARA Scanner with Circuit Breaker and Graceful Degradation
// ============================================================================

class YARAScannerWithCircuitBreaker {
public:
    YARAScannerWithCircuitBreaker()
        : m_circuit_breaker(Core::CircuitBreakerPresets::yara_scanner("ExampleYARA"sv))
    {
    }

    ErrorOr<String> scan_content(ReadonlyBytes content)
    {
        auto result = m_circuit_breaker.execute([&]() -> ErrorOr<String> {
            return perform_yara_scan(content);
        });

        // Handle circuit open state with graceful degradation
        if (result.is_error()) {
            auto state = m_circuit_breaker.state();
            if (state == Core::CircuitBreaker::State::Open) {
                // Circuit is open - YARA scanner is failing repeatedly
                // Graceful degradation: allow content with warning
                dbgln("YARA circuit open, allowing content with warning");
                return String::from_utf8("allowed_with_warning"sv);
            }
            // Normal error - propagate
            return result.error();
        }

        return result.value();
    }

    // Check if scanner is healthy
    bool is_healthy() const
    {
        auto state = m_circuit_breaker.state();
        return state == Core::CircuitBreaker::State::Closed;
    }

    // Get degraded mode status
    bool is_degraded() const
    {
        auto state = m_circuit_breaker.state();
        return state == Core::CircuitBreaker::State::HalfOpen;
    }

private:
    ErrorOr<String> perform_yara_scan(ReadonlyBytes content)
    {
        // Simulate YARA scan
        if (should_simulate_scanner_crash())
            return Error::from_string_literal("YARA scanner crashed");
        return String::from_utf8("clean"sv);
    }

    bool should_simulate_scanner_crash() const
    {
        return false;
    }

    Core::CircuitBreaker m_circuit_breaker;
};

// ============================================================================
// Example 3: IPC Client with Circuit Breaker and Retry
// ============================================================================

class IPCClientWithCircuitBreaker {
public:
    IPCClientWithCircuitBreaker()
        : m_circuit_breaker(Core::CircuitBreakerPresets::ipc_client("ExampleIPC"sv))
    {
    }

    ErrorOr<void> send_message(String const& message)
    {
        return m_circuit_breaker.execute([&]() -> ErrorOr<void> {
            return perform_ipc_send(message);
        });
    }

    ErrorOr<String> send_request_sync(String const& request)
    {
        auto result = m_circuit_breaker.execute([&]() -> ErrorOr<String> {
            return perform_ipc_request(request);
        });

        // Handle circuit open with connection retry
        if (result.is_error()) {
            if (m_circuit_breaker.state() == Core::CircuitBreaker::State::Open) {
                dbgln("IPC circuit open, attempting reconnection");
                TRY(attempt_reconnection());
                // After successful reconnection, circuit breaker will
                // transition to HalfOpen on next timeout
            }
            return result.error();
        }

        return result.value();
    }

private:
    ErrorOr<void> perform_ipc_send(String const& message)
    {
        // Simulate IPC send
        if (should_simulate_connection_error())
            return Error::from_string_literal("IPC connection lost");
        return {};
    }

    ErrorOr<String> perform_ipc_request(String const& request)
    {
        // Simulate IPC request/response
        if (should_simulate_connection_error())
            return Error::from_string_literal("IPC connection lost");
        return String::from_utf8("response"sv);
    }

    ErrorOr<void> attempt_reconnection()
    {
        // Attempt to reconnect IPC client
        dbgln("Reconnecting IPC client...");
        return {};
    }

    bool should_simulate_connection_error() const
    {
        return false;
    }

    Core::CircuitBreaker m_circuit_breaker;
};

// ============================================================================
// Example 4: Custom Circuit Breaker Configuration
// ============================================================================

class CustomServiceWithCircuitBreaker {
public:
    CustomServiceWithCircuitBreaker()
        : m_circuit_breaker(Core::CircuitBreaker::Config {
            .failure_threshold = 10,          // Open after 10 failures
            .timeout = Duration::from_seconds(120), // Wait 2 minutes before retry
            .success_threshold = 5,           // Need 5 successes to close
            .name = "CustomService"sv,
        })
    {
    }

    ErrorOr<void> perform_operation()
    {
        return m_circuit_breaker.execute([&]() -> ErrorOr<void> {
            // Your custom operation here
            return {};
        });
    }

private:
    Core::CircuitBreaker m_circuit_breaker;
};

// ============================================================================
// Example 5: Monitoring and Metrics
// ============================================================================

class ServiceWithMonitoring {
public:
    ServiceWithMonitoring()
        : m_circuit_breaker(Core::CircuitBreakerPresets::database("MonitoredService"sv))
    {
    }

    void print_health_status() const
    {
        auto metrics = m_circuit_breaker.get_metrics();

        dbgln("Circuit Breaker Health Report:");
        dbgln("  Service: {}", m_circuit_breaker.name());

        // State
        switch (metrics.state) {
        case Core::CircuitBreaker::State::Closed:
            dbgln("  State: CLOSED (healthy)");
            break;
        case Core::CircuitBreaker::State::Open:
            dbgln("  State: OPEN (failing)");
            break;
        case Core::CircuitBreaker::State::HalfOpen:
            dbgln("  State: HALF-OPEN (testing recovery)");
            break;
        }

        // Metrics
        dbgln("  Total successes: {}", metrics.total_successes);
        dbgln("  Total failures: {}", metrics.total_failures);
        dbgln("  Consecutive failures: {}", metrics.consecutive_failures);
        dbgln("  Consecutive successes: {}", metrics.consecutive_successes);
        dbgln("  State changes: {}", metrics.state_changes);

        // Success rate
        auto total = metrics.total_successes + metrics.total_failures;
        if (total > 0) {
            auto success_rate = (metrics.total_successes * 100.0) / total;
            dbgln("  Success rate: {:.2f}%", success_rate);
        }

        // Time in open state
        if (metrics.total_open_time.to_seconds() > 0) {
            dbgln("  Total downtime: {}s", metrics.total_open_time.to_seconds());
        }
    }

    bool should_alert() const
    {
        auto metrics = m_circuit_breaker.get_metrics();

        // Alert if circuit is open
        if (metrics.state == Core::CircuitBreaker::State::Open)
            return true;

        // Alert if success rate drops below 95%
        auto total = metrics.total_successes + metrics.total_failures;
        if (total > 0) {
            auto success_rate = (metrics.total_successes * 100.0) / total;
            if (success_rate < 95.0)
                return true;
        }

        // Alert if consecutive failures > 3
        if (metrics.consecutive_failures > 3)
            return true;

        return false;
    }

private:
    Core::CircuitBreaker m_circuit_breaker;
};

// ============================================================================
// Example 6: Integration with Fallback Manager (Future)
// ============================================================================

class ServiceWithFallback {
public:
    ServiceWithFallback()
        : m_circuit_breaker(Core::CircuitBreakerPresets::database("ServiceWithFallback"sv))
    {
    }

    ErrorOr<String> get_data(String const& key)
    {
        // Try primary source (protected by circuit breaker)
        auto result = m_circuit_breaker.execute([&]() -> ErrorOr<String> {
            return get_from_database(key);
        });

        // If primary fails and circuit is open, use fallback
        if (result.is_error()) {
            if (m_circuit_breaker.state() == Core::CircuitBreaker::State::Open) {
                dbgln("Database circuit open, using cache fallback");
                return get_from_cache(key);
            }
            return result.error();
        }

        return result.value();
    }

private:
    ErrorOr<String> get_from_database(String const& key)
    {
        // Primary data source
        return String::from_utf8("database_value"sv);
    }

    ErrorOr<String> get_from_cache(String const& key)
    {
        // Fallback data source
        return String::from_utf8("cached_value"sv);
    }

    Core::CircuitBreaker m_circuit_breaker;
};

} // namespace Sentinel::Examples

/*
 * USAGE NOTES:
 *
 * 1. Circuit Breaker Lifecycle:
 *    - Created in constructor (or as static member)
 *    - Survives for lifetime of service
 *    - Automatic cleanup on destruction
 *
 * 2. Error Handling:
 *    - Always check if result.is_error()
 *    - Check circuit state for fallback decisions
 *    - Log circuit state changes for monitoring
 *
 * 3. Fallback Strategies:
 *    - Cache serving (stale data is better than no data)
 *    - Queue for retry (asynchronous fallback)
 *    - Graceful degradation (reduced functionality)
 *    - Error with user notification
 *
 * 4. Monitoring:
 *    - Export metrics to Prometheus/Grafana
 *    - Alert on circuit open state
 *    - Track success rate trends
 *    - Monitor state change frequency
 *
 * 5. Testing:
 *    - Test all state transitions
 *    - Test fallback behavior
 *    - Test recovery scenarios
 *    - Test concurrent access
 */
