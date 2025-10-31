/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "GracefulDegradation.h"
#include <AK/Debug.h>
#include <AK/Time.h>

namespace Sentinel {

GracefulDegradation::GracefulDegradation()
{
    dbgln("GracefulDegradation: Initialized");
}

void GracefulDegradation::set_service_state(
    String service_name,
    ServiceState state,
    String reason,
    FallbackStrategy fallback)
{
    Threading::MutexLocker locker(m_mutex);

    auto now = UnixDateTime::now();
    auto old_state = ServiceState::Healthy;

    // Check if service already exists
    auto it = m_service_failures.find(service_name);
    if (it != m_service_failures.end()) {
        old_state = it->value.state;

        // If state is improving, update recovery metrics
        if (static_cast<int>(state) < static_cast<int>(old_state)) {
            if (state == ServiceState::Healthy) {
                m_total_recoveries++;
                m_last_recovery_time = now;
                dbgln("GracefulDegradation: Service '{}' recovered from {} to {}",
                    service_name,
                    service_state_to_string(old_state),
                    service_state_to_string(state));
            }
        }
        // If state is degrading, update failure metrics
        else if (static_cast<int>(state) > static_cast<int>(old_state)) {
            m_service_failures.get(service_name).value().failure_count++;
            m_total_failures++;
            m_last_failure_time = now;
            dbgln("GracefulDegradation: Service '{}' degraded from {} to {}: {}",
                service_name,
                service_state_to_string(old_state),
                service_state_to_string(state),
                reason);
        }

        // Update existing failure record
        m_service_failures.get(service_name).value().state = state;
        m_service_failures.get(service_name).value().failure_reason = reason;
        m_service_failures.get(service_name).value().last_check_at = now;
        m_service_failures.get(service_name).value().fallback_strategy = fallback;

        // Reset failure count if recovered
        if (state == ServiceState::Healthy) {
            m_service_failures.get(service_name).value().failure_count = 0;
            m_service_failures.get(service_name).value().recovery_attempts = 0;
        }
    } else {
        // Create new failure record
        ServiceFailure failure {
            .state = state,
            .service_name = service_name,
            .failure_reason = reason,
            .failed_at = now,
            .last_check_at = now,
            .failure_count = static_cast<size_t>((state == ServiceState::Healthy) ? 0 : 1),
            .recovery_attempts = 0,
            .fallback_strategy = fallback,
            .auto_recovery_enabled = true
        };

        m_service_failures.set(service_name, failure);

        if (state != ServiceState::Healthy) {
            m_total_failures++;
            m_last_failure_time = now;
            dbgln("GracefulDegradation: New service failure recorded for '{}': {}",
                service_name, reason);
        }
    }

    // Update metrics
    update_metrics(old_state, state);

    // Notify callbacks if state changed
    if (old_state != state) {
        DegradationEvent event {
            .service_name = service_name,
            .old_state = old_state,
            .new_state = state,
            .reason = reason,
            .timestamp = now
        };
        notify_degradation_event(event);
    }
}

GracefulDegradation::ServiceState GracefulDegradation::get_service_state(String const& service_name) const
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it == m_service_failures.end())
        return ServiceState::Healthy;

    return it->value.state;
}

GracefulDegradation::DegradationLevel GracefulDegradation::get_system_degradation_level() const
{
    Threading::MutexLocker locker(m_mutex);
    return calculate_system_level();
}

Vector<String> GracefulDegradation::get_degraded_services() const
{
    Threading::MutexLocker locker(m_mutex);

    Vector<String> degraded;
    for (auto const& [name, failure] : m_service_failures) {
        if (failure.state == ServiceState::Degraded) {
            degraded.append(name);
        }
    }
    return degraded;
}

Vector<String> GracefulDegradation::get_failed_services() const
{
    Threading::MutexLocker locker(m_mutex);

    Vector<String> failed;
    for (auto const& [name, failure] : m_service_failures) {
        if (failure.state == ServiceState::Failed || failure.state == ServiceState::Critical) {
            failed.append(name);
        }
    }
    return failed;
}

Vector<GracefulDegradation::ServiceFailure> GracefulDegradation::get_all_service_failures() const
{
    Threading::MutexLocker locker(m_mutex);

    Vector<ServiceFailure> failures;
    for (auto const& [_, failure] : m_service_failures) {
        if (failure.state != ServiceState::Healthy) {
            failures.append(failure);
        }
    }
    return failures;
}

bool GracefulDegradation::should_use_fallback(String const& service_name) const
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it == m_service_failures.end())
        return false;

    // Use fallback if service is degraded, failed, or critical
    return it->value.state != ServiceState::Healthy;
}

Optional<GracefulDegradation::FallbackStrategy> GracefulDegradation::get_fallback_strategy(String const& service_name) const
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it == m_service_failures.end())
        return {};

    return it->value.fallback_strategy;
}

Optional<String> GracefulDegradation::get_fallback_reason(String const& service_name) const
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it == m_service_failures.end())
        return {};

    if (it->value.state == ServiceState::Healthy)
        return {};

    return it->value.failure_reason;
}

void GracefulDegradation::mark_service_recovered(String service_name)
{
    set_service_state(service_name, ServiceState::Healthy, "Service recovered"_string, FallbackStrategy::None);
}

void GracefulDegradation::attempt_recovery(String const& service_name)
{
    Threading::MutexLocker locker(m_mutex);

    auto failure_opt = m_service_failures.get(service_name);
    if (!failure_opt.has_value()) {
        dbgln("GracefulDegradation: Cannot attempt recovery for unknown service '{}'", service_name);
        return;
    }

    auto& failure = failure_opt.value();
    failure.recovery_attempts++;
    failure.last_check_at = UnixDateTime::now();

    dbgln("GracefulDegradation: Attempting recovery for '{}' (attempt {}/{})",
        service_name,
        failure.recovery_attempts,
        m_recovery_attempt_limit);

    // Check if we've exceeded recovery attempt limit
    if (failure.recovery_attempts >= m_recovery_attempt_limit) {
        dbgln("GracefulDegradation: Service '{}' exceeded recovery attempt limit, marking as critical",
            service_name);
        failure.state = ServiceState::Critical;
    }
}

bool GracefulDegradation::is_recovery_in_progress(String const& service_name) const
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it == m_service_failures.end())
        return false;

    // Recovery is in progress if state is not healthy and we haven't given up
    return it->value.state != ServiceState::Healthy &&
           it->value.state != ServiceState::Critical &&
           it->value.recovery_attempts > 0 &&
           it->value.recovery_attempts < m_recovery_attempt_limit;
}

void GracefulDegradation::enable_auto_recovery(String service_name, bool enabled)
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it != m_service_failures.end()) {
        m_service_failures.get(service_name).value().auto_recovery_enabled = enabled;
        dbgln("GracefulDegradation: Auto-recovery for '{}' set to {}", service_name, enabled);
    } else {
        // Create a new healthy service entry with auto-recovery setting
        ServiceFailure failure {
            .state = ServiceState::Healthy,
            .service_name = service_name,
            .failure_reason = String {},
            .failed_at = UnixDateTime::now(),
            .last_check_at = UnixDateTime::now(),
            .failure_count = 0,
            .recovery_attempts = 0,
            .fallback_strategy = FallbackStrategy::None,
            .auto_recovery_enabled = enabled
        };
        m_service_failures.set(service_name, failure);
    }
}

bool GracefulDegradation::is_auto_recovery_enabled(String const& service_name) const
{
    Threading::MutexLocker locker(m_mutex);

    auto it = m_service_failures.find(service_name);
    if (it == m_service_failures.end())
        return true; // Default to enabled

    return it->value.auto_recovery_enabled;
}

void GracefulDegradation::register_degradation_callback(DegradationCallback callback)
{
    Threading::MutexLocker locker(m_mutex);
    m_callbacks.append(move(callback));
    dbgln("GracefulDegradation: Registered degradation callback (total: {})", m_callbacks.size());
}

void GracefulDegradation::clear_degradation_callbacks()
{
    Threading::MutexLocker locker(m_mutex);
    m_callbacks.clear();
    dbgln("GracefulDegradation: Cleared all degradation callbacks");
}

GracefulDegradation::DegradationMetrics GracefulDegradation::get_metrics() const
{
    Threading::MutexLocker locker(m_mutex);

    size_t healthy = 0, degraded = 0, failed = 0, critical = 0;

    for (auto const& [_, failure] : m_service_failures) {
        switch (failure.state) {
        case ServiceState::Healthy:
            healthy++;
            break;
        case ServiceState::Degraded:
            degraded++;
            break;
        case ServiceState::Failed:
            failed++;
            break;
        case ServiceState::Critical:
            critical++;
            break;
        }
    }

    return DegradationMetrics {
        .total_services = m_service_failures.size(),
        .healthy_services = healthy,
        .degraded_services = degraded,
        .failed_services = failed,
        .critical_services = critical,
        .total_failures = m_total_failures,
        .total_recoveries = m_total_recoveries,
        .system_level = calculate_system_level(),
        .last_failure_time = m_last_failure_time,
        .last_recovery_time = m_last_recovery_time
    };
}

void GracefulDegradation::reset_metrics()
{
    Threading::MutexLocker locker(m_mutex);

    m_total_failures = 0;
    m_total_recoveries = 0;
    m_last_failure_time = {};
    m_last_recovery_time = {};

    // Reset per-service metrics
    for (auto& [_, failure] : m_service_failures) {
        failure.failure_count = 0;
        failure.recovery_attempts = 0;
    }

    dbgln("GracefulDegradation: Metrics reset");
}

GracefulDegradation::HealthStatus GracefulDegradation::get_health_status() const
{
    Threading::MutexLocker locker(m_mutex);

    auto level = calculate_system_level();
    Vector<String> degraded_list, failed_list;
    Optional<String> critical_msg;

    for (auto const& [name, failure] : m_service_failures) {
        if (failure.state == ServiceState::Degraded) {
            degraded_list.append(name);
        } else if (failure.state == ServiceState::Failed) {
            failed_list.append(name);
        } else if (failure.state == ServiceState::Critical) {
            failed_list.append(name);
            if (!critical_msg.has_value()) {
                critical_msg = MUST(String::formatted("Critical failure in service: {}", name));
            }
        }
    }

    return HealthStatus {
        .is_healthy = (level == DegradationLevel::Normal),
        .level = level,
        .degraded_services = move(degraded_list),
        .failed_services = move(failed_list),
        .critical_message = critical_msg
    };
}

void GracefulDegradation::notify_degradation_event(DegradationEvent const& event)
{
    // Note: Caller must hold mutex
    dbgln("GracefulDegradation: Notifying {} callbacks about service '{}' state change",
        m_callbacks.size(), event.service_name);

    for (auto const& callback : m_callbacks) {
        callback(event);
    }
}

GracefulDegradation::DegradationLevel GracefulDegradation::calculate_system_level() const
{
    // Note: Caller must hold mutex

    bool has_critical = false;
    bool has_failed = false;
    bool has_degraded = false;

    for (auto const& [_, failure] : m_service_failures) {
        switch (failure.state) {
        case ServiceState::Critical:
            has_critical = true;
            break;
        case ServiceState::Failed:
            has_failed = true;
            break;
        case ServiceState::Degraded:
            has_degraded = true;
            break;
        case ServiceState::Healthy:
            break;
        }
    }

    // Critical failure if any service is critical
    if (has_critical)
        return DegradationLevel::CriticalFailure;

    // Degraded if any service is failed or degraded
    if (has_failed || has_degraded)
        return DegradationLevel::Degraded;

    return DegradationLevel::Normal;
}

void GracefulDegradation::update_metrics(ServiceState old_state, ServiceState new_state)
{
    // Note: Caller must hold mutex

    // Metrics are updated in set_service_state, this is a placeholder
    // for any additional metric calculations that might be needed
    (void)old_state;
    (void)new_state;
}

// Helper functions

String degradation_level_to_string(GracefulDegradation::DegradationLevel level)
{
    switch (level) {
    case GracefulDegradation::DegradationLevel::Normal:
        return "Normal"_string;
    case GracefulDegradation::DegradationLevel::Degraded:
        return "Degraded"_string;
    case GracefulDegradation::DegradationLevel::CriticalFailure:
        return "CriticalFailure"_string;
    }
    return "Unknown"_string;
}

String service_state_to_string(GracefulDegradation::ServiceState state)
{
    switch (state) {
    case GracefulDegradation::ServiceState::Healthy:
        return "Healthy"_string;
    case GracefulDegradation::ServiceState::Degraded:
        return "Degraded"_string;
    case GracefulDegradation::ServiceState::Failed:
        return "Failed"_string;
    case GracefulDegradation::ServiceState::Critical:
        return "Critical"_string;
    }
    return "Unknown"_string;
}

String fallback_strategy_to_string(GracefulDegradation::FallbackStrategy strategy)
{
    switch (strategy) {
    case GracefulDegradation::FallbackStrategy::None:
        return "None"_string;
    case GracefulDegradation::FallbackStrategy::UseCache:
        return "UseCache"_string;
    case GracefulDegradation::FallbackStrategy::AllowWithWarning:
        return "AllowWithWarning"_string;
    case GracefulDegradation::FallbackStrategy::SkipWithLog:
        return "SkipWithLog"_string;
    case GracefulDegradation::FallbackStrategy::RetryWithBackoff:
        return "RetryWithBackoff"_string;
    case GracefulDegradation::FallbackStrategy::QueueForRetry:
        return "QueueForRetry"_string;
    }
    return "Unknown"_string;
}

}
