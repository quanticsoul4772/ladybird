/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "GracefulDegradation.h"
#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/Optional.h>

namespace Sentinel {

// Helper class to integrate GracefulDegradation with existing services
// Provides wrapper methods that automatically handle fallback behavior
class GracefulDegradationIntegration {
public:
    explicit GracefulDegradationIntegration(GracefulDegradation& degradation)
        : m_degradation(degradation)
    {
    }

    // Execute an operation with automatic fallback handling
    // If the service is degraded/failed, the fallback function is called instead
    template<typename T>
    ErrorOr<T> execute_with_fallback(
        String service_name,
        Function<ErrorOr<T>()> operation,
        Function<ErrorOr<T>()> fallback
    )
    {
        // Check if service should use fallback
        if (m_degradation.should_use_fallback(service_name)) {
            auto strategy = m_degradation.get_fallback_strategy(service_name);
            dbgln("GracefulDegradationIntegration: Using fallback for service '{}' (strategy: {})",
                service_name,
                strategy.has_value() ? fallback_strategy_to_string(strategy.value()) : "Unknown"_string);

            return fallback();
        }

        // Try normal operation
        auto result = operation();

        // If operation failed, mark service as degraded and try fallback
        if (result.is_error()) {
            dbgln("GracefulDegradationIntegration: Operation failed for service '{}': {}",
                service_name, result.error());

            // Mark service as degraded on first failure
            m_degradation.set_service_state(
                service_name,
                GracefulDegradation::ServiceState::Degraded,
                String::formatted("Operation failed: {}", result.error()),
                GracefulDegradation::FallbackStrategy::UseCache
            );

            return fallback();
        }

        // Operation succeeded - ensure service is marked healthy
        if (m_degradation.get_service_state(service_name) != GracefulDegradation::ServiceState::Healthy) {
            dbgln("GracefulDegradationIntegration: Service '{}' recovered", service_name);
            m_degradation.mark_service_recovered(service_name);
        }

        return result;
    }

    // Execute an operation that may fail, with automatic degradation tracking
    // No fallback is provided - just tracks health and returns error
    template<typename T>
    ErrorOr<T> execute_with_tracking(
        String service_name,
        Function<ErrorOr<T>()> operation
    )
    {
        auto result = operation();

        if (result.is_error()) {
            // Track failure
            auto current_state = m_degradation.get_service_state(service_name);

            // Escalate state based on current state
            GracefulDegradation::ServiceState new_state;
            if (current_state == GracefulDegradation::ServiceState::Healthy) {
                new_state = GracefulDegradation::ServiceState::Degraded;
            } else if (current_state == GracefulDegradation::ServiceState::Degraded) {
                new_state = GracefulDegradation::ServiceState::Failed;
            } else {
                new_state = GracefulDegradation::ServiceState::Critical;
            }

            m_degradation.set_service_state(
                service_name,
                new_state,
                String::formatted("Operation failed: {}", result.error())
            );
        } else {
            // Mark as healthy on success
            if (m_degradation.get_service_state(service_name) != GracefulDegradation::ServiceState::Healthy) {
                m_degradation.mark_service_recovered(service_name);
            }
        }

        return result;
    }

    // Try an operation with automatic retry based on degradation state
    template<typename T>
    ErrorOr<T> try_with_recovery(
        String service_name,
        Function<ErrorOr<T>()> operation,
        size_t max_retries = 3
    )
    {
        for (size_t attempt = 0; attempt < max_retries; ++attempt) {
            auto result = operation();

            if (!result.is_error()) {
                // Success - mark as recovered if it was degraded
                if (m_degradation.get_service_state(service_name) != GracefulDegradation::ServiceState::Healthy) {
                    m_degradation.mark_service_recovered(service_name);
                }
                return result;
            }

            // Operation failed
            if (attempt == 0) {
                // First failure - mark as degraded
                m_degradation.set_service_state(
                    service_name,
                    GracefulDegradation::ServiceState::Degraded,
                    String::formatted("Attempt {} failed: {}", attempt + 1, result.error()),
                    GracefulDegradation::FallbackStrategy::RetryWithBackoff
                );
            } else if (attempt < max_retries - 1) {
                // Subsequent failures - attempt recovery
                m_degradation.attempt_recovery(service_name);
            } else {
                // Final attempt failed - mark as failed
                m_degradation.set_service_state(
                    service_name,
                    GracefulDegradation::ServiceState::Failed,
                    String::formatted("All {} attempts failed", max_retries),
                    GracefulDegradation::FallbackStrategy::None
                );
            }

            // Last attempt - return error
            if (attempt == max_retries - 1) {
                return result;
            }

            // Wait before retry (exponential backoff)
            // In production, this would use proper async waiting
            // For now, just a placeholder
        }

        return Error::from_string_literal("All retry attempts exhausted");
    }

private:
    GracefulDegradation& m_degradation;
};

}
