/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "BloomFilter.h"
#include "IPFSThreatSync.h"
#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>

namespace Sentinel {

// Forward declaration
class IPFSThreatSync;

class ThreatFeed {
public:
    // Bloom filter for 100M hashes, 0.1% false positive rate
    static constexpr size_t FILTER_SIZE_BITS = 1'200'000'000; // ~150MB
    static constexpr size_t FILTER_NUM_HASHES = 10;

    // Threat categories
    enum class ThreatCategory {
        Malware,
        Phishing,
        Exploit,
        PUP, // Potentially Unwanted Program
        Unknown
    };

    struct ThreatInfo {
        String sha256_hash;
        ThreatCategory category { ThreatCategory::Unknown };
        u32 severity { 0 }; // 0-10 scale
        Optional<String> family_name;
        UnixDateTime first_seen;
        UnixDateTime last_updated;
    };

    static ErrorOr<NonnullOwnPtr<ThreatFeed>> create();

    // Add a threat hash to the feed
    ErrorOr<void> add_threat_hash(String const& sha256_hash,
        ThreatCategory category = ThreatCategory::Unknown,
        u32 severity = 5);

    // Check if a hash is probably malicious
    bool probably_malicious(String const& sha256_hash) const;
    bool probably_malicious(ReadonlyBytes file_content) const;

    // Get detailed threat info (if available in cache)
    Optional<ThreatInfo> get_threat_info(String const& sha256_hash) const;

    // IPFS integration
    ErrorOr<void> sync_from_peers();
    ErrorOr<void> publish_local_threats();
    ErrorOr<void> subscribe_to_feed(String const& ipfs_hash);

    // Import/export threat list
    ErrorOr<void> import_threat_list(String const& file_path);
    ErrorOr<void> export_threat_list(String const& file_path) const;

    // Statistics
    struct Statistics {
        size_t total_threats { 0 };
        size_t malware_count { 0 };
        size_t phishing_count { 0 };
        size_t exploit_count { 0 };
        size_t pup_count { 0 };
        double false_positive_rate { 0.0 };
        Optional<UnixDateTime> last_sync;
        size_t peer_count { 0 };
    };

    Statistics get_statistics() const;

    // Persistence
    ErrorOr<void> save_to_disk(String const& path) const;
    ErrorOr<void> load_from_disk(String const& path);

    // Configuration
    void set_auto_sync_enabled(bool enabled) { m_auto_sync_enabled = enabled; }
    bool is_auto_sync_enabled() const { return m_auto_sync_enabled; }

    void set_sync_interval(AK::Duration interval) { m_sync_interval = interval; }
    AK::Duration sync_interval() const { return m_sync_interval; }

    // Differential privacy parameters for federated learning
    void set_privacy_epsilon(double epsilon) { m_privacy_epsilon = epsilon; }
    double privacy_epsilon() const { return m_privacy_epsilon; }

private:
    ThreatFeed();

    // Calculate SHA256 hash of file content
    String calculate_sha256(ReadonlyBytes content) const;

    // Main bloom filter for quick checks
    NonnullOwnPtr<BloomFilter> m_filter;

    // Detailed threat info cache (limited size)
    static constexpr size_t MAX_CACHE_SIZE = 10000;
    HashMap<String, ThreatInfo> m_threat_cache;

    // Category-specific counters
    HashMap<ThreatCategory, size_t> m_category_counts;

    // IPFS integration
    OwnPtr<IPFSThreatSync> m_ipfs_sync;

    // Configuration
    bool m_auto_sync_enabled { true };
    AK::Duration m_sync_interval { AK::Duration::from_seconds(1800) }; // 30 minutes
    Optional<UnixDateTime> m_last_sync_time;

    // Differential privacy parameters
    double m_privacy_epsilon { 0.1 }; // Strong privacy by default

    // Peer management
    Vector<String> m_trusted_peers;
    size_t m_peer_count { 0 };
};

}