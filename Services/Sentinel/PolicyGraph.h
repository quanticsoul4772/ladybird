/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
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

    // Milestone 0.3: Credential Protection Structures
    struct CredentialRelationship {
        i64 id { -1 };
        String form_origin;
        String action_origin;
        String relationship_type;
        UnixDateTime created_at;
        String created_by;
        Optional<UnixDateTime> last_used;
        i64 use_count { 0 };
        Optional<UnixDateTime> expires_at;
        String notes;
    };

    struct CredentialAlert {
        i64 id { -1 };
        UnixDateTime detected_at;
        String form_origin;
        String action_origin;
        String alert_type;
        String severity;
        bool has_password_field;
        bool has_email_field;
        bool uses_https;
        bool is_cross_origin;
        Optional<String> user_action;
        Optional<i64> policy_id;
        String alert_json;
    };

    struct PolicyTemplate {
        i64 id { -1 };
        String name;
        String description;
        String category;
        String template_json;
        bool is_builtin;
        UnixDateTime created_at;
        Optional<UnixDateTime> updated_at;
    };

    static ErrorOr<NonnullOwnPtr<PolicyGraph>> create(ByteString const& db_directory);

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

    // Milestone 0.3: Credential Relationship Management
    ErrorOr<i64> create_relationship(CredentialRelationship const& relationship);
    ErrorOr<CredentialRelationship> get_relationship(String const& form_origin, String const& action_origin, String const& type);
    ErrorOr<Vector<CredentialRelationship>> list_relationships(Optional<String> type_filter);
    ErrorOr<void> update_relationship_usage(i64 relationship_id);
    ErrorOr<void> delete_relationship(i64 relationship_id);
    ErrorOr<bool> has_relationship(String const& form_origin, String const& action_origin, String const& type);

    // Milestone 0.3: Credential Alert History
    ErrorOr<i64> record_credential_alert(CredentialAlert const& alert);
    ErrorOr<Vector<CredentialAlert>> get_credential_alerts(Optional<UnixDateTime> since);
    ErrorOr<Vector<CredentialAlert>> get_alerts_by_origin(String const& origin);
    ErrorOr<void> update_alert_action(i64 alert_id, String const& user_action);

    // Milestone 0.3: Policy Templates
    ErrorOr<i64> create_template(PolicyTemplate const& tmpl);
    ErrorOr<PolicyTemplate> get_template(i64 template_id);
    ErrorOr<PolicyTemplate> get_template_by_name(String const& name);
    ErrorOr<Vector<PolicyTemplate>> list_templates(Optional<String> category_filter);
    ErrorOr<void> update_template(i64 template_id, PolicyTemplate const& tmpl);
    ErrorOr<void> delete_template(i64 template_id);
    ErrorOr<Policy> instantiate_template(i64 template_id, HashMap<String, String> const& variables);
    ErrorOr<void> seed_builtin_templates();

    // Milestone 0.3: Import/Export
    ErrorOr<String> export_relationships_json();
    ErrorOr<void> import_relationships_json(String const& json);
    ErrorOr<String> export_templates_json();
    ErrorOr<void> import_templates_json(String const& json);

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

        // Milestone 0.3: Credential Relationships
        Database::StatementID create_relationship { 0 };
        Database::StatementID get_relationship { 0 };
        Database::StatementID list_relationships { 0 };
        Database::StatementID list_relationships_filtered { 0 };
        Database::StatementID update_relationship_usage { 0 };
        Database::StatementID delete_relationship { 0 };
        Database::StatementID has_relationship { 0 };

        // Milestone 0.3: Credential Alerts
        Database::StatementID record_credential_alert { 0 };
        Database::StatementID get_credential_alerts_all { 0 };
        Database::StatementID get_credential_alerts_since { 0 };
        Database::StatementID get_alerts_by_origin { 0 };
        Database::StatementID update_alert_action { 0 };

        // Milestone 0.3: Policy Templates
        Database::StatementID create_template { 0 };
        Database::StatementID get_template { 0 };
        Database::StatementID get_template_by_name { 0 };
        Database::StatementID list_templates_all { 0 };
        Database::StatementID list_templates_filtered { 0 };
        Database::StatementID update_template { 0 };
        Database::StatementID delete_template { 0 };
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
