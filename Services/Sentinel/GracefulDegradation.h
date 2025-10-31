/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibThreading/Mutex.h>

namespace Sentinel {

using AK::Duration;

// Graceful degradation manager for Sentinel service failures
// Handles fallback behavior when critical services become unavailable
// Thread-safe for concurrent access from multiple components
class GracefulDegradation {
public:
    // Service degradation levels
    enum class DegradationLevel {
        Normal,          // All services operating normally
        Degraded,        // Some services degraded, using fallback mechanisms
        CriticalFailure  // Critical services failed, limited functionality
    };

    // Service health status
    enum class ServiceState {
        Healthy,   // Service operating normally
        Degraded,  // Service operating with reduced functionality
        Failed,    // Service unavailable, using fallback
        Critical   // Fallback also failed, system unstable
    };

    // Fallback strategy for a service
    enum class FallbackStrategy {
        None,                  // No fallback, fail immediately
        UseCache,              // Use in-memory cache
        AllowWithWarning,      // Allow operation but warn user
        SkipWithLog,           // Skip operation, log only
        RetryWithBackoff,      // Retry with exponential backoff
        QueueForRetry          // Queue for later retry
    };

    // Service failure information
    struct ServiceFailure {
        ServiceState state;
        String service_name;
        String failure_reason;
        UnixDateTime failed_at;
        UnixDateTime last_check_at;
        size_t failure_count { 0 };
        size_t recovery_attempts { 0 };
        FallbackStrategy fallback_strategy { FallbackStrategy::None };
        bool auto_recovery_enabled { true };
    };

    // Degradation event notification
    struct DegradationEvent {
        String service_name;
        ServiceState old_state;
        ServiceState new_state;
        String reason;
        UnixDateTime timestamp;
    };

    // Callback for degradation events
    using DegradationCallback = Function<void(DegradationEvent const&)>;

    GracefulDegradation();
    ~GracefulDegradation() = default;

    // Service state management
    void set_service_state(
        String service_name,
        ServiceState state,
        String reason,
        FallbackStrategy fallback = FallbackStrategy::None
    );

    ServiceState get_service_state(String const& service_name) const;
    DegradationLevel get_system_degradation_level() const;

    // Query degraded services
    Vector<String> get_degraded_services() const;
    Vector<String> get_failed_services() const;
    Vector<ServiceFailure> get_all_service_failures() const;

    // Fallback behavior
    bool should_use_fallback(String const& service_name) const;
    Optional<FallbackStrategy> get_fallback_strategy(String const& service_name) const;
    Optional<String> get_fallback_reason(String const& service_name) const;

    // Recovery detection
    void mark_service_recovered(String service_name);
    void attempt_recovery(String const& service_name);
    bool is_recovery_in_progress(String const& service_name) const;

    // Auto-recovery configuration
    void enable_auto_recovery(String service_name, bool enabled = true);
    bool is_auto_recovery_enabled(String const& service_name) const;

    // Event notifications
    void register_degradation_callback(DegradationCallback callback);
    void clear_degradation_callbacks();

    // Statistics and metrics
    struct DegradationMetrics {
        size_t total_services;
        size_t healthy_services;
        size_t degraded_services;
        size_t failed_services;
        size_t critical_services;
        size_t total_failures;
        size_t total_recoveries;
        DegradationLevel system_level;
        Optional<UnixDateTime> last_failure_time;
        Optional<UnixDateTime> last_recovery_time;
    };

    DegradationMetrics get_metrics() const;
    void reset_metrics();

    // Health check support
    struct HealthStatus {
        bool is_healthy;
        DegradationLevel level;
        Vector<String> degraded_services;
        Vector<String> failed_services;
        Optional<String> critical_message;
    };

    HealthStatus get_health_status() const;

    // Configuration
    void set_failure_threshold(size_t threshold) { m_failure_threshold = threshold; }
    void set_recovery_attempt_limit(size_t limit) { m_recovery_attempt_limit = limit; }
    size_t failure_threshold() const { return m_failure_threshold; }
    size_t recovery_attempt_limit() const { return m_recovery_attempt_limit; }

private:
    void notify_degradation_event(DegradationEvent const& event);
    DegradationLevel calculate_system_level() const;
    void update_metrics(ServiceState old_state, ServiceState new_state);

    // Service failure tracking
    HashMap<String, ServiceFailure> m_service_failures;

    // Event callbacks
    Vector<DegradationCallback> m_callbacks;

    // Metrics
    size_t m_total_failures { 0 };
    size_t m_total_recoveries { 0 };
    Optional<UnixDateTime> m_last_failure_time;
    Optional<UnixDateTime> m_last_recovery_time;

    // Configuration
    size_t m_failure_threshold { 3 };      // Mark as failed after N failures
    size_t m_recovery_attempt_limit { 5 }; // Give up after N recovery attempts

    // Thread safety
    mutable Threading::Mutex m_mutex;
};

// Predefined service names for consistency
namespace Services {
    constexpr StringView PolicyGraph = "PolicyGraph"sv;
    constexpr StringView YARAScanner = "YARAScanner"sv;
    constexpr StringView IPCServer = "IPCServer"sv;
    constexpr StringView Database = "Database"sv;
    constexpr StringView Quarantine = "Quarantine"sv;
    constexpr StringView SecurityTap = "SecurityTap"sv;
    constexpr StringView NetworkLayer = "NetworkLayer"sv;
}

// Helper function to convert enums to strings for logging
String degradation_level_to_string(GracefulDegradation::DegradationLevel level);
String service_state_to_string(GracefulDegradation::ServiceState state);
String fallback_strategy_to_string(GracefulDegradation::FallbackStrategy strategy);

}
