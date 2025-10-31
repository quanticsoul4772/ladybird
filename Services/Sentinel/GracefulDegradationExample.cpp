/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

// This file demonstrates practical integration of GracefulDegradation
// with existing Sentinel services. These examples show how to modify
// existing code to support graceful degradation.

#include "GracefulDegradation.h"
#include "GracefulDegradationIntegration.h"
#include "PolicyGraph.h"
#include <AK/Debug.h>

namespace Sentinel {

// Example 1: PolicyGraph with Database Fallback
// ============================================
// This example shows how to modify PolicyGraph::match_policy to use
// graceful degradation with cache fallback when database is unavailable.

class PolicyGraphWithFallback {
public:
    PolicyGraphWithFallback(PolicyGraph& policy_graph, GracefulDegradation& degradation)
        : m_policy_graph(policy_graph)
        , m_degradation(degradation)
        , m_integration(degradation)
    {
    }

    ErrorOr<Optional<PolicyGraph::Policy>> match_policy_with_fallback(
        PolicyGraph::ThreatMetadata const& threat)
    {
        // Use the integration helper to automatically handle fallback
        return m_integration.execute_with_fallback<Optional<PolicyGraph::Policy>>(
            String { Services::Database },
            // Normal operation: query database
            [&]() -> ErrorOr<Optional<PolicyGraph::Policy>> {
                return m_policy_graph.match_policy(threat);
            },
            // Fallback: use cache only (database unavailable)
            [&]() -> ErrorOr<Optional<PolicyGraph::Policy>> {
                dbgln("PolicyGraph: Using cache-only mode due to database failure");

                // In cache-only mode, we can only return cached results
                // This is a degraded mode, but keeps the system operational

                // Note: In real implementation, you would access the cache directly
                // For this example, we return empty Optional (no match found)
                return Optional<PolicyGraph::Policy> {};
            }
        );
    }

    // Record threat with fallback to in-memory queue
    ErrorOr<void> record_threat_with_fallback(
        PolicyGraph::ThreatMetadata const& threat,
        String action_taken,
        Optional<i64> policy_id,
        String alert_json)
    {
        auto result = m_policy_graph.record_threat(threat, action_taken, policy_id, alert_json);

        if (result.is_error()) {
            // Database write failed - use fallback
            m_degradation.set_service_state(
                String { Services::Database },
                GracefulDegradation::ServiceState::Degraded,
                String::formatted("Failed to record threat: {}", result.error()),
                GracefulDegradation::FallbackStrategy::QueueForRetry
            );

            // Queue for later retry (in production, this would be a proper queue)
            dbgln("PolicyGraph: Queueing threat record for retry: {}", threat.url);
            m_pending_threat_records.append({
                .threat = threat,
                .action_taken = action_taken,
                .policy_id = policy_id,
                .alert_json = alert_json
            });

            // Return success - we've queued it
            return {};
        }

        // Success - mark as healthy and flush any queued records
        if (m_degradation.get_service_state(String { Services::Database }) !=
            GracefulDegradation::ServiceState::Healthy) {

            m_degradation.mark_service_recovered(String { Services::Database });

            // Try to flush queued records
            flush_queued_records();
        }

        return {};
    }

private:
    void flush_queued_records()
    {
        if (m_pending_threat_records.is_empty())
            return;

        dbgln("PolicyGraph: Flushing {} queued threat records", m_pending_threat_records.size());

        Vector<size_t> successful_indices;

        for (size_t i = 0; i < m_pending_threat_records.size(); ++i) {
            auto const& record = m_pending_threat_records[i];
            auto result = m_policy_graph.record_threat(
                record.threat,
                record.action_taken,
                record.policy_id,
                record.alert_json
            );

            if (!result.is_error()) {
                successful_indices.append(i);
            }
        }

        // Remove successful records (in reverse order to maintain indices)
        for (auto it = successful_indices.rbegin(); it != successful_indices.rend(); ++it) {
            m_pending_threat_records.remove(*it);
        }

        dbgln("PolicyGraph: Flushed {}/{} queued records",
            successful_indices.size(),
            successful_indices.size() + m_pending_threat_records.size());
    }

    struct PendingThreatRecord {
        PolicyGraph::ThreatMetadata threat;
        String action_taken;
        Optional<i64> policy_id;
        String alert_json;
    };

    PolicyGraph& m_policy_graph;
    GracefulDegradation& m_degradation;
    GracefulDegradationIntegration m_integration;
    Vector<PendingThreatRecord> m_pending_threat_records;
};

// Example 2: YARA Scanner with Fallback
// ====================================
// This example shows how to handle YARA scanner failures gracefully

class YARAScannerWithFallback {
public:
    YARAScannerWithFallback(GracefulDegradation& degradation)
        : m_degradation(degradation)
    {
    }

    struct ScanResult {
        bool is_threat { false };
        bool was_scanned { true }; // False if scan was skipped due to fallback
        Optional<ByteString> alert_json;
        Optional<String> warning_message;
    };

    ErrorOr<ScanResult> scan_with_fallback(ReadonlyBytes content)
    {
        // Check if scanner is in fallback mode
        if (m_degradation.should_use_fallback(String { Services::YARAScanner })) {
            auto strategy = m_degradation.get_fallback_strategy(String { Services::YARAScanner });

            if (strategy == GracefulDegradation::FallbackStrategy::AllowWithWarning) {
                dbgln("YARAScanWorker: Scanner unavailable, allowing download with warning");

                return ScanResult {
                    .is_threat = false,
                    .was_scanned = false,
                    .warning_message = "Malware scanner temporarily unavailable. Download allowed but not scanned."_string
                };
            }

            if (strategy == GracefulDegradation::FallbackStrategy::SkipWithLog) {
                dbgln("YARAScanWorker: Scanner failed, skipping scan");

                return ScanResult {
                    .is_threat = false,
                    .was_scanned = false,
                    .warning_message = "Malware scanning skipped due to service failure."_string
                };
            }
        }

        // Attempt normal scan
        auto result = perform_yara_scan(content);

        if (result.is_error()) {
            // Scan failed - update degradation state
            auto current_state = m_degradation.get_service_state(String { Services::YARAScanner });

            if (current_state == GracefulDegradation::ServiceState::Healthy) {
                // First failure - mark as degraded, allow with warning
                m_degradation.set_service_state(
                    String { Services::YARAScanner },
                    GracefulDegradation::ServiceState::Degraded,
                    String::formatted("Scan failed: {}", result.error()),
                    GracefulDegradation::FallbackStrategy::AllowWithWarning
                );
            } else {
                // Subsequent failure - escalate to failed
                m_degradation.set_service_state(
                    String { Services::YARAScanner },
                    GracefulDegradation::ServiceState::Failed,
                    String::formatted("Repeated scan failure: {}", result.error()),
                    GracefulDegradation::FallbackStrategy::SkipWithLog
                );
            }

            // Return safe result with warning
            return ScanResult {
                .is_threat = false,
                .was_scanned = false,
                .warning_message = "Malware scan failed. Download allowed as precautionary measure."_string
            };
        }

        // Success - mark as recovered if it was degraded
        if (m_degradation.get_service_state(String { Services::YARAScanner }) !=
            GracefulDegradation::ServiceState::Healthy) {

            dbgln("YARAScanWorker: Scanner recovered");
            m_degradation.mark_service_recovered(String { Services::YARAScanner });
        }

        return result.value();
    }

private:
    ErrorOr<ScanResult> perform_yara_scan(ReadonlyBytes content)
    {
        // This would be the actual YARA scanning logic
        // For this example, we just return a placeholder
        (void)content;

        // Simulate occasional failure for testing
        static int call_count = 0;
        call_count++;

        if (call_count % 10 == 0) {
            return Error::from_string_literal("Simulated YARA scanner failure");
        }

        return ScanResult { .is_threat = false, .was_scanned = true };
    }

    GracefulDegradation& m_degradation;
};

// Example 3: IPC Connection with Retry
// ===================================
// This example shows how to handle IPC connection failures with retry

class IPCConnectionWithFallback {
public:
    IPCConnectionWithFallback(GracefulDegradation& degradation)
        : m_degradation(degradation)
        , m_integration(degradation)
    {
    }

    ErrorOr<void> send_message_with_retry(ByteString const& message)
    {
        return m_integration.try_with_recovery<void>(
            String { Services::IPCServer },
            [&]() -> ErrorOr<void> {
                return send_message_impl(message);
            },
            /* max_retries */ 5
        );
    }

private:
    ErrorOr<void> send_message_impl(ByteString const& message)
    {
        // This would be the actual IPC send logic
        (void)message;

        // Simulate occasional connection failure
        static int call_count = 0;
        call_count++;

        if (call_count % 15 == 0) {
            return Error::from_string_literal("IPC connection lost");
        }

        return {};
    }

    GracefulDegradation& m_degradation;
    GracefulDegradationIntegration m_integration;
};

// Example 4: Comprehensive Service Health Monitor
// ==============================================
// This example shows how to monitor all services and provide health status

class ServiceHealthMonitor {
public:
    ServiceHealthMonitor(GracefulDegradation& degradation)
        : m_degradation(degradation)
    {
        // Register callback to receive degradation events
        m_degradation.register_degradation_callback([this](auto const& event) {
            handle_degradation_event(event);
        });
    }

    void print_health_status() const
    {
        auto health = m_degradation.get_health_status();

        dbgln("\n=== Sentinel Service Health Status ===");
        dbgln("Overall Status: {}", health.is_healthy ? "HEALTHY" : "DEGRADED");
        dbgln("Degradation Level: {}", degradation_level_to_string(health.level));

        if (!health.degraded_services.is_empty()) {
            dbgln("\nDegraded Services:");
            for (auto const& service : health.degraded_services) {
                auto reason = m_degradation.get_fallback_reason(service);
                dbgln("  - {}: {}", service, reason.value_or("Unknown reason"_string));
            }
        }

        if (!health.failed_services.is_empty()) {
            dbgln("\nFailed Services:");
            for (auto const& service : health.failed_services) {
                auto reason = m_degradation.get_fallback_reason(service);
                auto strategy = m_degradation.get_fallback_strategy(service);
                dbgln("  - {}: {} (Fallback: {})",
                    service,
                    reason.value_or("Unknown reason"_string),
                    strategy.has_value() ? fallback_strategy_to_string(strategy.value()) : "None"_string);
            }
        }

        if (health.critical_message.has_value()) {
            dbgln("\n⚠️  CRITICAL: {}", health.critical_message.value());
        }

        // Print metrics
        auto metrics = m_degradation.get_metrics();
        dbgln("\n=== Service Metrics ===");
        dbgln("Total Services: {}", metrics.total_services);
        dbgln("Healthy: {} | Degraded: {} | Failed: {} | Critical: {}",
            metrics.healthy_services,
            metrics.degraded_services,
            metrics.failed_services,
            metrics.critical_services);
        dbgln("Total Failures: {} | Total Recoveries: {}",
            metrics.total_failures,
            metrics.total_recoveries);

        dbgln("=====================================\n");
    }

private:
    void handle_degradation_event(GracefulDegradation::DegradationEvent const& event)
    {
        dbgln("ServiceHealthMonitor: Degradation event for '{}': {} -> {}",
            event.service_name,
            service_state_to_string(event.old_state),
            service_state_to_string(event.new_state));

        // In production, this would:
        // 1. Send notification to UI
        // 2. Log to audit system
        // 3. Alert monitoring systems
        // 4. Update dashboard

        if (event.new_state == GracefulDegradation::ServiceState::Failed) {
            dbgln("⚠️  Service '{}' has FAILED. Reason: {}", event.service_name, event.reason);
        } else if (event.new_state == GracefulDegradation::ServiceState::Critical) {
            dbgln("🚨 Service '{}' is CRITICAL. Immediate attention required!", event.service_name);
        } else if (event.new_state == GracefulDegradation::ServiceState::Healthy &&
                   event.old_state != GracefulDegradation::ServiceState::Healthy) {
            dbgln("✅ Service '{}' has RECOVERED", event.service_name);
        }
    }

    GracefulDegradation& m_degradation;
};

// Example 5: Complete Integration Example
// ======================================
// This shows how to set up graceful degradation in SentinelServer

void example_sentinel_server_integration()
{
    // Create degradation manager (should be owned by SentinelServer)
    GracefulDegradation degradation;

    // Set up health monitor
    ServiceHealthMonitor health_monitor(degradation);

    // Configure thresholds
    degradation.set_failure_threshold(3);
    degradation.set_recovery_attempt_limit(5);

    // Enable auto-recovery for all services
    degradation.enable_auto_recovery(String { Services::Database }, true);
    degradation.enable_auto_recovery(String { Services::YARAScanner }, true);
    degradation.enable_auto_recovery(String { Services::IPCServer }, true);

    dbgln("Sentinel: Graceful degradation initialized");
    health_monitor.print_health_status();

    // Simulate some service failures and recoveries
    dbgln("\n--- Simulating database connection loss ---");
    degradation.set_service_state(
        String { Services::Database },
        GracefulDegradation::ServiceState::Failed,
        "Connection timeout after 30s"_string,
        GracefulDegradation::FallbackStrategy::UseCache
    );
    health_monitor.print_health_status();

    dbgln("\n--- Simulating YARA scanner crash ---");
    degradation.set_service_state(
        String { Services::YARAScanner },
        GracefulDegradation::ServiceState::Failed,
        "Segmentation fault in scanner worker"_string,
        GracefulDegradation::FallbackStrategy::AllowWithWarning
    );
    health_monitor.print_health_status();

    dbgln("\n--- Simulating database recovery ---");
    degradation.mark_service_recovered(String { Services::Database });
    health_monitor.print_health_status();

    dbgln("\n--- Simulating YARA scanner recovery ---");
    degradation.mark_service_recovered(String { Services::YARAScanner });
    health_monitor.print_health_status();
}

}
