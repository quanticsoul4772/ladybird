/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel {

// Forward declaration to avoid circular dependency
class ThreatFeed;

class IPFSThreatSync {
public:
    // Threat category matching ThreatFeed
    enum class ThreatCategory {
        Malware,
        Phishing,
        Exploit,
        PUP,
        Unknown
    };

    struct ThreatHashInfo {
        String hash;
        ThreatCategory category { ThreatCategory::Unknown };
        u32 severity { 5 };
        Optional<UnixDateTime> timestamp;
    };

    struct PeerInfo {
        String peer_id;
        String multiaddr;
        bool is_trusted { false };
        size_t threats_shared { 0 };
        Optional<UnixDateTime> last_seen;
    };

    static ErrorOr<NonnullOwnPtr<IPFSThreatSync>> create();
    ~IPFSThreatSync();

    // Fetch latest threats from connected peers
    ErrorOr<Vector<ThreatHashInfo>> fetch_latest_threats();

    // Publish threats to the network
    ErrorOr<void> publish_threats(Vector<ThreatHashInfo> const& threats);

    // Subscribe to a specific threat feed
    ErrorOr<void> subscribe_to_feed(String const& ipfs_hash);

    // Unsubscribe from a feed
    ErrorOr<void> unsubscribe_from_feed(String const& ipfs_hash);

    // Get list of connected peers
    Vector<PeerInfo> get_connected_peers() const;

    // Get number of connected peers
    size_t connected_peer_count() const { return m_connected_peers.size(); }

    // Add a trusted peer
    ErrorOr<void> add_trusted_peer(String const& peer_id);

    // Remove a trusted peer
    void remove_trusted_peer(String const& peer_id);

    // Check if IPFS is available
    bool is_ipfs_available() const { return m_ipfs_available; }

    // Get IPFS node status
    struct NodeStatus {
        bool is_online { false };
        String node_id;
        size_t num_peers { 0 };
        size_t bandwidth_in { 0 };
        size_t bandwidth_out { 0 };
        Optional<String> version;
    };

    NodeStatus get_node_status() const;

    // Configuration
    void set_max_peers(size_t max_peers) { m_max_peers = max_peers; }
    size_t max_peers() const { return m_max_peers; }

    void set_bandwidth_limit(size_t bytes_per_second) { m_bandwidth_limit = bytes_per_second; }
    size_t bandwidth_limit() const { return m_bandwidth_limit; }

private:
    IPFSThreatSync();

    // Initialize connection to IPFS daemon
    ErrorOr<void> initialize_ipfs_connection();

    // Mock implementation for when IPFS is not available
    Vector<ThreatHashInfo> generate_mock_threats() const;

    // IPFS availability flag
    bool m_ipfs_available { false };

    // Connected peers
    Vector<PeerInfo> m_connected_peers;

    // Trusted peer IDs
    Vector<String> m_trusted_peer_ids;

    // Subscribed feeds
    Vector<String> m_subscribed_feeds;

    // Configuration
    size_t m_max_peers { 100 };
    size_t m_bandwidth_limit { 1024 * 1024 }; // 1 MB/s

    // Mock data for testing
    mutable size_t m_mock_threat_counter { 0 };

    // IPFS daemon connection details
    String m_api_endpoint { "http://127.0.0.1:5001"_string };
    Optional<String> m_node_id;
};

}