/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibCore/CircuitBreaker.h>
#include <LibDatabase/Database.h>
#include "LRUCache.h"

namespace Sentinel {

// Policy cache wrapper using O(1) LRU cache
class PolicyGraphCache {
public:
    using CacheMetrics = LRUCache<String, Optional<int>>::CacheMetrics;

    PolicyGraphCache(size_t max_size = 1000)
        : m_lru_cache(max_size)
    {
    }

    Optional<Optional<int>> get_cached(String const& key);
    void cache_policy(String const& key, Optional<int> policy_id);
    void invalidate();

    CacheMetrics get_metrics() const;
    void reset_metrics();

private:
    LRUCache<String, Optional<int>> m_lru_cache;
};

class PolicyGraph {
public:
    enum class PolicyAction {
        Allow,
        Block,
        Quarantine,
        BlockAutofill,    // NEW: Prevent autofill for forms
        WarnUser          // NEW: Show warning, allow if confirmed
    };

    enum class PolicyMatchType {
        DownloadOriginFileType,  // Existing: Download from origin with file type
        FormActionMismatch,      // NEW: Form posts to different origin
        InsecureCredentialPost,  // NEW: Password sent over HTTP
        ThirdPartyFormPost       // NEW: Form posts to third-party
    };

    struct Policy {
        i64 id { -1 };
        String rule_name;
        Optional<String> url_pattern;
        Optional<String> file_hash;
        Optional<String> mime_type;
        PolicyAction action;
        PolicyMatchType match_type { PolicyMatchType::DownloadOriginFileType };
        String enforcement_action;  // Additional action details (e.g., "block_autofill", "warn_user")
        UnixDateTime created_at;
        String created_by;
        Optional<UnixDateTime> expires_at;
        i64 hit_count { 0 };
        Optional<UnixDateTime> last_hit;
    };

    struct ThreatMetadata {
        String url;
        String filename;
        String file_hash;
        String mime_type;
        u64 file_size;
        String rule_name;
        String severity;
    };

    struct ThreatRecord {
        i64 id;
        UnixDateTime detected_at;
        String url;
        String filename;
        String file_hash;
        String mime_type;
        u64 file_size;
        String rule_name;
        String severity;
        String action_taken;
        Optional<i64> policy_id;
        String alert_json;
    };

    static ErrorOr<PolicyGraph> create(ByteString const& db_directory);

    // Policy CRUD operations
    ErrorOr<i64> create_policy(Policy const& policy);
    ErrorOr<Policy> get_policy(i64 policy_id);
    ErrorOr<Vector<Policy>> list_policies();
    ErrorOr<void> update_policy(i64 policy_id, Policy const& policy);
    ErrorOr<void> delete_policy(i64 policy_id);

    // Policy matching (priority: hash > URL pattern > rule name)
    ErrorOr<Optional<Policy>> match_policy(ThreatMetadata const& threat);

    // Threat history
    ErrorOr<void> record_threat(ThreatMetadata const& threat,
                                String action_taken,
                                Optional<i64> policy_id,
                                String alert_json);
    ErrorOr<Vector<ThreatRecord>> get_threat_history(Optional<UnixDateTime> since);
    ErrorOr<Vector<ThreatRecord>> get_threats_by_rule(String const& rule_name);

    // Utility
    ErrorOr<void> cleanup_expired_policies();
    ErrorOr<u64> get_policy_count();
    ErrorOr<u64> get_threat_count();

    // Memory optimization
    ErrorOr<void> cleanup_old_threats(u64 days_to_keep = 30);
    ErrorOr<void> vacuum_database();

    // Database health and error recovery
    ErrorOr<void> verify_database_integrity();
    bool is_database_healthy();

    // Transaction support for atomicity
    ErrorOr<void> begin_transaction();
    ErrorOr<void> commit_transaction();
    ErrorOr<void> rollback_transaction();

    // Cache metrics
    PolicyGraphCache::CacheMetrics get_cache_metrics() const { return m_cache.get_metrics(); }
    void reset_cache_metrics() { m_cache.reset_metrics(); }

    // Circuit breaker metrics
    Core::CircuitBreaker::Metrics get_circuit_breaker_metrics() const { return m_circuit_breaker.get_metrics(); }
    void reset_circuit_breaker() { m_circuit_breaker.reset(); }

    // Conversion utilities (public for testing)
    static PolicyAction string_to_action(String const& action_str);
    static String action_to_string(PolicyAction action);
    static PolicyMatchType string_to_match_type(String const& type_str);
    static String match_type_to_string(PolicyMatchType type);

private:
    struct Statements {
        // Policy CRUD
        Database::StatementID create_policy { 0 };
        Database::StatementID get_last_insert_id { 0 };
        Database::StatementID get_policy { 0 };
        Database::StatementID list_policies { 0 };
        Database::StatementID update_policy { 0 };
        Database::StatementID delete_policy { 0 };
        Database::StatementID increment_hit_count { 0 };
        Database::StatementID update_last_hit { 0 };

        // Policy matching
        Database::StatementID match_by_hash { 0 };
        Database::StatementID match_by_url_pattern { 0 };
        Database::StatementID match_by_rule_name { 0 };

        // Threat history
        Database::StatementID record_threat { 0 };
        Database::StatementID get_threats_since { 0 };
        Database::StatementID get_threats_all { 0 };
        Database::StatementID get_threats_by_rule { 0 };

        // Utility
        Database::StatementID delete_expired_policies { 0 };
        Database::StatementID count_policies { 0 };
        Database::StatementID count_threats { 0 };

        // Memory optimization
        Database::StatementID delete_old_threats { 0 };

        // Transaction support
        Database::StatementID begin_transaction { 0 };
        Database::StatementID commit_transaction { 0 };
        Database::StatementID rollback_transaction { 0 };
    };

    PolicyGraph(NonnullRefPtr<Database::Database>, Statements);

    ErrorOr<String> compute_cache_key(ThreatMetadata const& threat) const;

    NonnullRefPtr<Database::Database> m_database;
    Statements m_statements;
    PolicyGraphCache m_cache;
    bool m_database_healthy { true };

    // Circuit breaker for database operations
    // Prevents cascade failures when database is unavailable
    mutable Core::CircuitBreaker m_circuit_breaker { Core::CircuitBreakerPresets::database("PolicyGraph::Database"sv) };
};

}
