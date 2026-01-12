/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "ClientRateLimiter.h"
#include "HealthCheck.h"
#include "MalwareML.h"
#include "ThreatFeed.h"
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Vector.h>
#include <LibCore/CircuitBreaker.h>
#include <LibCore/EventLoop.h>
#include <LibCore/LocalServer.h>
#include <LibCore/Socket.h>
#include <LibIPC/BufferedIPCReader.h>

namespace Sentinel {

// Forward declarations
class PolicyGraph;

class SentinelServer {
public:
    static ErrorOr<NonnullOwnPtr<SentinelServer>> create();
    ~SentinelServer() = default;

    // Get rate limiter telemetry
    ClientRateLimiter const& rate_limiter() const { return m_rate_limiter; }

    // Health check system
    HealthCheck& health_check() { return m_health_check; }
    HealthCheck const& health_check() const { return m_health_check; }

    // Initialize health checks
    void initialize_health_checks(PolicyGraph* policy_graph);

    // Get number of active connections
    size_t active_connection_count() const { return m_clients.size(); }

    // Circuit breaker metrics
    Core::CircuitBreaker::Metrics get_yara_circuit_breaker_metrics() const { return m_yara_circuit_breaker.get_metrics(); }

private:
    SentinelServer(NonnullRefPtr<Core::LocalServer>);

    void handle_client(NonnullOwnPtr<Core::Socket>);
    ErrorOr<void> process_message(Core::Socket&, String const& message);

    ErrorOr<ByteString> scan_file(ByteString const& file_path);
    ErrorOr<ByteString> scan_content(ReadonlyBytes content);

    // Get client ID for rate limiting
    int get_client_id(Core::Socket const* socket);

    NonnullRefPtr<Core::LocalServer> m_server;
    Vector<NonnullOwnPtr<Core::Socket>> m_clients;

    // Per-client buffered readers for handling partial IPC messages
    HashMap<Core::Socket*, IPC::BufferedIPCReader> m_client_readers;

    // Per-client rate limiter (DoS protection)
    ClientRateLimiter m_rate_limiter;

    // Socket to client ID mapping
    HashMap<Core::Socket const*, int> m_socket_to_client_id;
    int m_next_client_id { 1 };

    // Health check system
    HealthCheck m_health_check;

    // Circuit breaker for YARA scanning operations
    // Prevents cascade failures when YARA scanner crashes or hangs
    mutable Core::CircuitBreaker m_yara_circuit_breaker { Core::CircuitBreakerPresets::yara_scanner("SentinelServer::YARA"sv) };

    // ML-based malware detection (Milestone 0.4)
    OwnPtr<MalwareMLDetector> m_ml_detector;

    // Federated threat intelligence (Milestone 0.4 Phase 3)
    OwnPtr<ThreatFeed> m_threat_feed;
};

}
