/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PolicyGraph.h"
#include "DatabaseMigrations.h"
#include "InputValidator.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/StringBuilder.h>
#include <LibCore/RetryPolicy.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibCrypto/ConstantTimeComparison.h>
#include <LibFileSystem/FileSystem.h>

namespace Sentinel {

using AK::Duration;

// Retry policy for database operations with exponential backoff
// Max attempts: 3, Initial delay: 100ms, Max delay: 1 second, Backoff: 2x
static Core::RetryPolicy s_db_retry_policy(
    3,                                          // max_attempts
    Duration::from_milliseconds(100),           // initial_delay
    Duration::from_seconds(1),                  // max_delay
    2.0,                                        // backoff_multiplier
    0.1                                         // jitter_factor
);

static void initialize_db_retry_policy()
{
    static bool initialized = false;
    if (!initialized) {
        s_db_retry_policy.set_retry_predicate(Core::RetryPolicy::database_retry_predicate());
        initialized = true;
    }
}

// Security validation functions using InputValidator framework

static ErrorOr<void> validate_policy_inputs(PolicyGraph::Policy const& policy)
{
    // Validate rule_name (non-empty, length, no control chars)
    auto rule_name_result = InputValidator::validate_rule_name(policy.rule_name);
    if (!rule_name_result.is_valid)
        return InputValidator::to_error(rule_name_result);

    // Validate url_pattern if present
    if (policy.url_pattern.has_value()) {
        auto const& pattern = policy.url_pattern.value();
        if (!pattern.is_empty()) {
            auto url_result = InputValidator::validate_url_pattern(pattern);
            if (!url_result.is_valid)
                return InputValidator::to_error(url_result);
        }
    }

    // Validate file_hash if present (SHA256 = 64 hex chars)
    if (policy.file_hash.has_value()) {
        auto const& hash = policy.file_hash.value();
        if (!hash.is_empty()) {
            auto hash_result = InputValidator::validate_sha256(hash);
            if (!hash_result.is_valid)
                return InputValidator::to_error(hash_result);
        }
    }

    // Validate mime_type if present
    if (policy.mime_type.has_value()) {
        auto const& mime = policy.mime_type.value();
        if (!mime.is_empty()) {
            auto mime_result = InputValidator::validate_mime_type(mime);
            if (!mime_result.is_valid)
                return InputValidator::to_error(mime_result);
        }
    }

    // Validate created_by (non-empty, length)
    auto created_by_result = InputValidator::validate_length(policy.created_by, 1, 256, "created_by"sv);
    if (!created_by_result.is_valid)
        return InputValidator::to_error(created_by_result);

    auto created_by_control_result = InputValidator::validate_no_control_chars(policy.created_by, "created_by"sv);
    if (!created_by_control_result.is_valid)
        return InputValidator::to_error(created_by_control_result);

    // Validate enforcement_action (length, no control chars)
    auto action_length_result = InputValidator::validate_length(policy.enforcement_action, 0, 256, "enforcement_action"sv);
    if (!action_length_result.is_valid)
        return InputValidator::to_error(action_length_result);

    auto action_control_result = InputValidator::validate_no_control_chars(policy.enforcement_action, "enforcement_action"sv);
    if (!action_control_result.is_valid)
        return InputValidator::to_error(action_control_result);

    // Validate action enum (allow, block, quarantine, etc.)
    auto action_str = PolicyGraph::action_to_string(policy.action);
    auto action_enum_result = InputValidator::validate_action(action_str);
    if (!action_enum_result.is_valid)
        return InputValidator::to_error(action_enum_result);

    // Validate timestamps
    auto created_at_ms = policy.created_at.milliseconds_since_epoch();
    auto timestamp_result = InputValidator::validate_timestamp(created_at_ms);
    if (!timestamp_result.is_valid)
        return InputValidator::to_error(timestamp_result);

    if (policy.expires_at.has_value()) {
        auto expires_at_ms = policy.expires_at->milliseconds_since_epoch();
        auto expiry_result = InputValidator::validate_expiry(expires_at_ms);
        if (!expiry_result.is_valid)
            return InputValidator::to_error(expiry_result);
    }

    if (policy.last_hit.has_value()) {
        auto last_hit_ms = policy.last_hit->milliseconds_since_epoch();
        // Last hit must be in the past (not future)
        auto now_ms = UnixDateTime::now().milliseconds_since_epoch();
        if (last_hit_ms > now_ms)
            return Error::from_string_literal("Invalid policy: last_hit cannot be in the future");
    }

    // Validate hit_count is non-negative
    if (policy.hit_count < 0)
        return Error::from_string_literal("Invalid policy: hit_count cannot be negative");

    return {};
}

// PolicyGraphCache implementation using O(1) LRU cache

Optional<Optional<int>> PolicyGraphCache::get_cached(String const& key)
{
    auto cached_value = m_lru_cache.get(key);
    if (!cached_value.has_value()) {
        // Cache miss
        return {};
    }

    // Cache hit - return the Optional<int> value
    return cached_value.value();
}

void PolicyGraphCache::cache_policy(String const& key, Optional<int> policy_id)
{
    // Put into LRU cache - it handles eviction automatically
    auto result = m_lru_cache.put(key, policy_id);
    if (result.is_error()) {
        dbgln("PolicyGraphCache: Failed to cache policy: {}", result.error());
    }
}

void PolicyGraphCache::invalidate()
{
    m_lru_cache.invalidate();
}

PolicyGraphCache::CacheMetrics PolicyGraphCache::get_metrics() const
{
    return m_lru_cache.get_metrics();
}

void PolicyGraphCache::reset_metrics()
{
    m_lru_cache.reset_metrics();
}

// PolicyGraph implementation

ErrorOr<NonnullOwnPtr<PolicyGraph>> PolicyGraph::create(ByteString const& db_directory)
{
    // Initialize retry policy for database operations
    initialize_db_retry_policy();

    // Ensure directory exists with restrictive permissions (owner only)
    // SECURITY: Policy database contains threat history and user decisions
    if (!FileSystem::exists(db_directory))
        TRY(Core::System::mkdir(db_directory, 0700));

    // Create/open database
    // FIXME: Re-add retry logic once RetryPolicy template is fixed
    auto database = TRY(Database::Database::create(db_directory, "policy_graph"sv));

    // Create policies table
    auto create_policies_table = TRY(database->prepare_statement(R"#(
        CREATE TABLE IF NOT EXISTS policies (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            rule_name TEXT NOT NULL,
            url_pattern TEXT,
            file_hash TEXT,
            mime_type TEXT,
            action TEXT NOT NULL,
            match_type TEXT DEFAULT 'download',
            enforcement_action TEXT DEFAULT '',
            created_at INTEGER NOT NULL,
            created_by TEXT NOT NULL,
            expires_at INTEGER,
            hit_count INTEGER DEFAULT 0,
            last_hit INTEGER
        );
    )#"sv));
    database->execute_statement(create_policies_table, {});

    // Create indexes on policies table
    auto create_rule_name_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_rule_name ON policies(rule_name);"sv));
    database->execute_statement(create_rule_name_index, {});

    auto create_file_hash_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_file_hash ON policies(file_hash);"sv));
    database->execute_statement(create_file_hash_index, {});

    auto create_url_pattern_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_policies_url_pattern ON policies(url_pattern);"sv));
    database->execute_statement(create_url_pattern_index, {});

    // Create threat_history table
    auto create_threats_table = TRY(database->prepare_statement(R"#(
        CREATE TABLE IF NOT EXISTS threat_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            detected_at INTEGER NOT NULL,
            url TEXT NOT NULL,
            filename TEXT NOT NULL,
            file_hash TEXT NOT NULL,
            mime_type TEXT,
            file_size INTEGER NOT NULL,
            rule_name TEXT NOT NULL,
            severity TEXT NOT NULL,
            action_taken TEXT NOT NULL,
            policy_id INTEGER,
            alert_json TEXT NOT NULL,
            FOREIGN KEY (policy_id) REFERENCES policies(id)
        );
    )#"sv));
    database->execute_statement(create_threats_table, {});

    // Create indexes on threat_history table
    auto create_detected_at_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_detected_at ON threat_history(detected_at);"sv));
    database->execute_statement(create_detected_at_index, {});

    auto create_threat_rule_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_rule_name ON threat_history(rule_name);"sv));
    database->execute_statement(create_threat_rule_index, {});

    auto create_threat_hash_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_threat_history_file_hash ON threat_history(file_hash);"sv));
    database->execute_statement(create_threat_hash_index, {});

    // Create credential_relationships table (Milestone 0.3)
    auto create_relationships_table = TRY(database->prepare_statement(R"#(
        CREATE TABLE IF NOT EXISTS credential_relationships (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            form_origin TEXT NOT NULL,
            action_origin TEXT NOT NULL,
            relationship_type TEXT NOT NULL,
            created_at INTEGER NOT NULL,
            created_by TEXT NOT NULL,
            last_used INTEGER,
            use_count INTEGER DEFAULT 0,
            expires_at INTEGER,
            notes TEXT,
            UNIQUE(form_origin, action_origin, relationship_type)
        );
    )#"sv));
    database->execute_statement(create_relationships_table, {});

    // Create indexes on credential_relationships table
    auto create_rel_origins_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_relationships_origins ON credential_relationships(form_origin, action_origin);"sv));
    database->execute_statement(create_rel_origins_index, {});

    auto create_rel_type_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_relationships_type ON credential_relationships(relationship_type);"sv));
    database->execute_statement(create_rel_type_index, {});

    // Create credential_alerts table (Milestone 0.3)
    auto create_alerts_table = TRY(database->prepare_statement(R"#(
        CREATE TABLE IF NOT EXISTS credential_alerts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            detected_at INTEGER NOT NULL,
            form_origin TEXT NOT NULL,
            action_origin TEXT NOT NULL,
            alert_type TEXT NOT NULL,
            severity TEXT NOT NULL,
            has_password_field INTEGER NOT NULL,
            has_email_field INTEGER NOT NULL,
            uses_https INTEGER NOT NULL,
            is_cross_origin INTEGER NOT NULL,
            user_action TEXT,
            policy_id INTEGER,
            alert_json TEXT,
            FOREIGN KEY(policy_id) REFERENCES policies(id)
        );
    )#"sv));
    database->execute_statement(create_alerts_table, {});

    // Create indexes on credential_alerts table
    auto create_alerts_time_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_alerts_time ON credential_alerts(detected_at);"sv));
    database->execute_statement(create_alerts_time_index, {});

    auto create_alerts_origins_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_alerts_origins ON credential_alerts(form_origin, action_origin);"sv));
    database->execute_statement(create_alerts_origins_index, {});

    auto create_alerts_type_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_alerts_type ON credential_alerts(alert_type);"sv));
    database->execute_statement(create_alerts_type_index, {});

    // Create policy_templates table (Milestone 0.3)
    auto create_templates_table = TRY(database->prepare_statement(R"#(
        CREATE TABLE IF NOT EXISTS policy_templates (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            description TEXT NOT NULL,
            category TEXT NOT NULL,
            template_json TEXT NOT NULL,
            is_builtin INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL,
            updated_at INTEGER
        );
    )#"sv));
    database->execute_statement(create_templates_table, {});

    // Create index on policy_templates table
    auto create_templates_category_index = TRY(database->prepare_statement(
        "CREATE INDEX IF NOT EXISTS idx_templates_category ON policy_templates(category);"sv));
    database->execute_statement(create_templates_category_index, {});

    // Run database migrations to add performance indexes
    // This will check schema version and apply any pending migrations
    dbgln("PolicyGraph: Checking for database migrations");
    auto migration_result = DatabaseMigrations::migrate(*database);
    if (migration_result.is_error()) {
        dbgln("PolicyGraph: Warning - migration failed: {}", migration_result.error());
        // Continue anyway - migrations are optional optimizations
        // The database will still work with just the basic indexes above
    } else {
        dbgln("PolicyGraph: Database migrations complete");
    }

    // Prepare all statements
    Statements statements {};

    // Policy CRUD statements
    statements.create_policy = TRY(database->prepare_statement(R"#(
        INSERT INTO policies (rule_name, url_pattern, file_hash, mime_type, action,
                             match_type, enforcement_action, created_at, created_by, expires_at, hit_count, last_hit)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0, NULL);
    )#"sv));

    statements.get_last_insert_id = TRY(database->prepare_statement(
        "SELECT last_insert_rowid();"sv));

    statements.get_policy = TRY(database->prepare_statement(
        "SELECT * FROM policies WHERE id = ?;"sv));

    statements.list_policies = TRY(database->prepare_statement(
        "SELECT * FROM policies ORDER BY created_at DESC;"sv));

    statements.update_policy = TRY(database->prepare_statement(R"#(
        UPDATE policies
        SET rule_name = ?, url_pattern = ?, file_hash = ?, mime_type = ?,
            action = ?, match_type = ?, enforcement_action = ?, expires_at = ?
        WHERE id = ?;
    )#"sv));

    statements.delete_policy = TRY(database->prepare_statement(
        "DELETE FROM policies WHERE id = ?;"sv));

    statements.increment_hit_count = TRY(database->prepare_statement(
        "UPDATE policies SET hit_count = hit_count + 1, last_hit = ? WHERE id = ?;"sv));

    statements.update_last_hit = TRY(database->prepare_statement(
        "UPDATE policies SET last_hit = ? WHERE id = ?;"sv));

    // Policy matching statements (priority order)
    statements.match_by_hash = TRY(database->prepare_statement(R"#(
        SELECT * FROM policies
        WHERE file_hash = ?
          AND (expires_at = -1 OR expires_at > ?)
        LIMIT 1;
    )#"sv));

    // SECURITY FIX: Added ESCAPE clause to prevent SQL injection via URL patterns
    statements.match_by_url_pattern = TRY(database->prepare_statement(R"#(
        SELECT * FROM policies
        WHERE url_pattern != ''
          AND ? LIKE url_pattern ESCAPE '\'
          AND (expires_at = -1 OR expires_at > ?)
        LIMIT 1;
    )#"sv));

    statements.match_by_rule_name = TRY(database->prepare_statement(R"#(
        SELECT * FROM policies
        WHERE rule_name = ?
          AND (file_hash = '' OR file_hash IS NULL)
          AND (url_pattern = '' OR url_pattern IS NULL)
          AND (expires_at = -1 OR expires_at > ?)
        LIMIT 1;
    )#"sv));

    // Threat history statements
    statements.record_threat = TRY(database->prepare_statement(R"#(
        INSERT INTO threat_history
            (detected_at, url, filename, file_hash, mime_type, file_size,
             rule_name, severity, action_taken, policy_id, alert_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )#"sv));

    statements.get_threats_since = TRY(database->prepare_statement(
        "SELECT * FROM threat_history WHERE detected_at >= ? ORDER BY detected_at DESC;"sv));

    statements.get_threats_all = TRY(database->prepare_statement(
        "SELECT * FROM threat_history ORDER BY detected_at DESC;"sv));

    statements.get_threats_by_rule = TRY(database->prepare_statement(
        "SELECT * FROM threat_history WHERE rule_name = ? ORDER BY detected_at DESC;"sv));

    // Utility statements
    statements.delete_expired_policies = TRY(database->prepare_statement(
        "DELETE FROM policies WHERE expires_at IS NOT NULL AND expires_at != -1 AND expires_at <= ?;"sv));

    statements.count_policies = TRY(database->prepare_statement(
        "SELECT COUNT(*) FROM policies;"sv));

    statements.count_threats = TRY(database->prepare_statement(
        "SELECT COUNT(*) FROM threat_history;"sv));

    // Memory optimization statements
    statements.delete_old_threats = TRY(database->prepare_statement(
        "DELETE FROM threat_history WHERE detected_at < ?;"sv));

    // Transaction support statements
    statements.begin_transaction = TRY(database->prepare_statement(
        "BEGIN IMMEDIATE TRANSACTION;"sv));

    statements.commit_transaction = TRY(database->prepare_statement(
        "COMMIT TRANSACTION;"sv));

    statements.rollback_transaction = TRY(database->prepare_statement(
        "ROLLBACK TRANSACTION;"sv));

    // Milestone 0.3: Credential Relationship statements
    statements.create_relationship = TRY(database->prepare_statement(R"#(
        INSERT INTO credential_relationships
            (form_origin, action_origin, relationship_type, created_at, created_by,
             last_used, use_count, expires_at, notes)
        VALUES (?, ?, ?, ?, ?, NULL, 0, ?, ?);
    )#"sv));

    statements.get_relationship = TRY(database->prepare_statement(
        "SELECT * FROM credential_relationships WHERE form_origin = ? AND action_origin = ? AND relationship_type = ?;"sv));

    statements.list_relationships = TRY(database->prepare_statement(
        "SELECT * FROM credential_relationships ORDER BY created_at DESC;"sv));

    statements.list_relationships_filtered = TRY(database->prepare_statement(
        "SELECT * FROM credential_relationships WHERE relationship_type = ? ORDER BY created_at DESC;"sv));

    statements.update_relationship_usage = TRY(database->prepare_statement(
        "UPDATE credential_relationships SET use_count = use_count + 1, last_used = ? WHERE id = ?;"sv));

    statements.delete_relationship = TRY(database->prepare_statement(
        "DELETE FROM credential_relationships WHERE id = ?;"sv));

    statements.has_relationship = TRY(database->prepare_statement(
        "SELECT COUNT(*) FROM credential_relationships WHERE form_origin = ? AND action_origin = ? AND relationship_type = ?;"sv));

    // Milestone 0.3: Credential Alert statements
    statements.record_credential_alert = TRY(database->prepare_statement(R"#(
        INSERT INTO credential_alerts
            (detected_at, form_origin, action_origin, alert_type, severity,
             has_password_field, has_email_field, uses_https, is_cross_origin,
             user_action, policy_id, alert_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
    )#"sv));

    statements.get_credential_alerts_all = TRY(database->prepare_statement(
        "SELECT * FROM credential_alerts ORDER BY detected_at DESC;"sv));

    statements.get_credential_alerts_since = TRY(database->prepare_statement(
        "SELECT * FROM credential_alerts WHERE detected_at >= ? ORDER BY detected_at DESC;"sv));

    statements.get_alerts_by_origin = TRY(database->prepare_statement(
        "SELECT * FROM credential_alerts WHERE form_origin = ? OR action_origin = ? ORDER BY detected_at DESC;"sv));

    statements.update_alert_action = TRY(database->prepare_statement(
        "UPDATE credential_alerts SET user_action = ? WHERE id = ?;"sv));

    // Milestone 0.3: Policy Template statements
    statements.create_template = TRY(database->prepare_statement(R"#(
        INSERT INTO policy_templates
            (name, description, category, template_json, is_builtin, created_at, updated_at)
        VALUES (?, ?, ?, ?, ?, ?, NULL);
    )#"sv));

    statements.get_template = TRY(database->prepare_statement(
        "SELECT * FROM policy_templates WHERE id = ?;"sv));

    statements.get_template_by_name = TRY(database->prepare_statement(
        "SELECT * FROM policy_templates WHERE name = ?;"sv));

    statements.list_templates_all = TRY(database->prepare_statement(
        "SELECT * FROM policy_templates ORDER BY created_at DESC;"sv));

    statements.list_templates_filtered = TRY(database->prepare_statement(
        "SELECT * FROM policy_templates WHERE category = ? ORDER BY created_at DESC;"sv));

    statements.update_template = TRY(database->prepare_statement(R"#(
        UPDATE policy_templates
        SET name = ?, description = ?, category = ?, template_json = ?, updated_at = ?
        WHERE id = ?;
    )#"sv));

    statements.delete_template = TRY(database->prepare_statement(
        "DELETE FROM policy_templates WHERE id = ?;"sv));

    auto policy_graph = adopt_own(*new PolicyGraph(move(database), statements));

    // Cleanup old threats on initialization
    auto cleanup_result = policy_graph->cleanup_old_threats();
    if (cleanup_result.is_error())
        dbgln("PolicyGraph: Warning - failed to cleanup old threats: {}", cleanup_result.error());

    return policy_graph;
}

PolicyGraph::PolicyGraph(NonnullRefPtr<Database::Database> database, Statements statements)
    : m_database(move(database))
    , m_statements(statements)
{
}

// Policy CRUD implementations

ErrorOr<i64> PolicyGraph::create_policy(Policy const& policy)
{
    // SECURITY: Validate all policy inputs before database insertion
    TRY(validate_policy_inputs(policy));

    // Check for duplicate policy before INSERT to avoid constraint violations
    // Check by (rule_name, url_pattern, file_hash) combination
    auto existing_policies = TRY(list_policies());
    for (auto const& existing : existing_policies) {
        bool rule_name_match = existing.rule_name == policy.rule_name;
        bool url_pattern_match = existing.url_pattern == policy.url_pattern;

        // Use constant-time comparison for hash to prevent timing attacks
        // Handle Optional<String> properly:
        // - If both have values, unwrap and compare
        // - If only one has value, they don't match
        // - If neither has value, they match
        bool file_hash_match = false;
        if (existing.file_hash.has_value() && policy.file_hash.has_value()) {
            // Both have values - use constant-time comparison
            file_hash_match = Crypto::ConstantTimeComparison::compare_hashes(
                existing.file_hash.value(),
                policy.file_hash.value()
            );
        } else if (!existing.file_hash.has_value() && !policy.file_hash.has_value()) {
            // Neither has value - consider them matching
            file_hash_match = true;
        }
        // else: only one has value - file_hash_match remains false

        if (rule_name_match && url_pattern_match && file_hash_match) {
            return Error::from_string_literal("Policy with same rule_name, url_pattern, and file_hash already exists");
        }
    }

    // Convert action enum to string
    auto action_str = action_to_string(policy.action);
    auto match_type_str = match_type_to_string(policy.match_type);

    // Convert optional expiration to i64 or null
    Optional<i64> expires_ms;
    if (policy.expires_at.has_value())
        expires_ms = policy.expires_at->milliseconds_since_epoch();

    // Execute insert with validated data
    // Note: execute_statement uses SQL_MUST internally which crashes on errors
    // We can't catch SQLite errors here, but we can check if the operation succeeded
    m_database->execute_statement(
        m_statements.create_policy,
        {},
        policy.rule_name,
        policy.url_pattern.has_value() ? policy.url_pattern.value() : String {},
        policy.file_hash.has_value() ? policy.file_hash.value() : String {},
        policy.mime_type.has_value() ? policy.mime_type.value() : String {},
        action_str,
        match_type_str,
        policy.enforcement_action,
        policy.created_at.milliseconds_since_epoch(),
        policy.created_by,
        expires_ms.has_value() ? expires_ms.value() : -1
    );

    // Get last insert ID - ISSUE-010 FIX: Check if callback was invoked
    i64 last_id = 0;
    bool callback_invoked = false;
    m_database->execute_statement(
        m_statements.get_last_insert_id,
        [&](auto statement_id) {
            last_id = m_database->result_column<i64>(statement_id, 0);
            callback_invoked = true;
        }
    );

    // ISSUE-010 FIX: Verify callback was invoked and ID is valid
    if (!callback_invoked || last_id == 0) {
        return Error::from_string_literal("Failed to retrieve policy ID after insertion");
    }

    // Invalidate cache since policies changed
    m_cache.invalidate();

    return last_id;
}

ErrorOr<PolicyGraph::Policy> PolicyGraph::get_policy(i64 policy_id)
{
    Optional<Policy> result;

    m_database->execute_statement(
        m_statements.get_policy,
        [&](auto statement_id) {
            Policy policy;
            int col = 0;
            policy.id = m_database->result_column<i64>(statement_id, col++);
            policy.rule_name = m_database->result_column<String>(statement_id, col++);

            auto url_pattern = m_database->result_column<String>(statement_id, col++);
            if (!url_pattern.is_empty())
                policy.url_pattern = url_pattern;

            auto file_hash = m_database->result_column<String>(statement_id, col++);
            if (!file_hash.is_empty())
                policy.file_hash = file_hash;

            auto mime_type = m_database->result_column<String>(statement_id, col++);
            if (!mime_type.is_empty())
                policy.mime_type = mime_type;

            auto action_str = m_database->result_column<String>(statement_id, col++);
            policy.action = string_to_action(action_str);

            auto match_type_str = m_database->result_column<String>(statement_id, col++);
            policy.match_type = string_to_match_type(match_type_str);

            policy.enforcement_action = m_database->result_column<String>(statement_id, col++);

            policy.created_at = m_database->result_column<UnixDateTime>(statement_id, col++);
            policy.created_by = m_database->result_column<String>(statement_id, col++);

            auto expires_ms = m_database->result_column<i64>(statement_id, col++);
            if (expires_ms > 0)
                policy.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

            policy.hit_count = m_database->result_column<i64>(statement_id, col++);

            auto last_hit_ms = m_database->result_column<i64>(statement_id, col++);
            if (last_hit_ms > 0)
                policy.last_hit = UnixDateTime::from_milliseconds_since_epoch(last_hit_ms);

            result = policy;
        },
        policy_id
    );

    if (!result.has_value())
        return Error::from_string_literal("Policy not found");

    return result.release_value();
}

ErrorOr<Vector<PolicyGraph::Policy>> PolicyGraph::list_policies()
{
    Vector<Policy> policies;

    m_database->execute_statement(
        m_statements.list_policies,
        [&](auto statement_id) {
            Policy policy;
            int col = 0;
            policy.id = m_database->result_column<i64>(statement_id, col++);
            policy.rule_name = m_database->result_column<String>(statement_id, col++);

            auto url_pattern = m_database->result_column<String>(statement_id, col++);
            if (!url_pattern.is_empty())
                policy.url_pattern = url_pattern;

            auto file_hash = m_database->result_column<String>(statement_id, col++);
            if (!file_hash.is_empty())
                policy.file_hash = file_hash;

            auto mime_type = m_database->result_column<String>(statement_id, col++);
            if (!mime_type.is_empty())
                policy.mime_type = mime_type;

            auto action_str = m_database->result_column<String>(statement_id, col++);
            policy.action = string_to_action(action_str);

            auto match_type_str = m_database->result_column<String>(statement_id, col++);
            policy.match_type = string_to_match_type(match_type_str);

            policy.enforcement_action = m_database->result_column<String>(statement_id, col++);

            policy.created_at = m_database->result_column<UnixDateTime>(statement_id, col++);
            policy.created_by = m_database->result_column<String>(statement_id, col++);

            auto expires_ms = m_database->result_column<i64>(statement_id, col++);
            if (expires_ms > 0)
                policy.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

            policy.hit_count = m_database->result_column<i64>(statement_id, col++);

            auto last_hit_ms = m_database->result_column<i64>(statement_id, col++);
            if (last_hit_ms > 0)
                policy.last_hit = UnixDateTime::from_milliseconds_since_epoch(last_hit_ms);

            policies.append(move(policy));
        }
    );

    return policies;
}

ErrorOr<void> PolicyGraph::update_policy(i64 policy_id, Policy const& policy)
{
    // SECURITY: Validate all policy inputs before database update
    TRY(validate_policy_inputs(policy));

    auto action_str = action_to_string(policy.action);
    auto match_type_str = match_type_to_string(policy.match_type);

    Optional<i64> expires_ms;
    if (policy.expires_at.has_value())
        expires_ms = policy.expires_at->milliseconds_since_epoch();

    m_database->execute_statement(
        m_statements.update_policy,
        {},
        policy.rule_name,
        policy.url_pattern.has_value() ? policy.url_pattern.value() : String {},
        policy.file_hash.has_value() ? policy.file_hash.value() : String {},
        policy.mime_type.has_value() ? policy.mime_type.value() : String {},
        action_str,
        match_type_str,
        policy.enforcement_action,
        expires_ms.has_value() ? expires_ms.value() : -1,
        policy_id
    );

    // Invalidate cache since policies changed
    m_cache.invalidate();

    return {};
}

ErrorOr<void> PolicyGraph::delete_policy(i64 policy_id)
{
    m_database->execute_statement(m_statements.delete_policy, {}, policy_id);

    // Invalidate cache since policies changed
    m_cache.invalidate();

    return {};
}

// Policy matching implementation

ErrorOr<String> PolicyGraph::compute_cache_key(ThreatMetadata const& threat) const
{
    // Create cache key: hash(url + filename + mime_type + file_hash)
    // Using simple concatenation with separator for uniqueness
    StringBuilder builder;
    builder.append(threat.url);
    builder.append('|');
    builder.append(threat.filename);
    builder.append('|');
    builder.append(threat.mime_type);
    builder.append('|');
    builder.append(threat.file_hash);

    // Use the string's hash trait for a quick hash - now with error propagation
    auto input_string = TRY(builder.to_string());
    auto hash_value = Traits<String>::hash(input_string);

    return TRY(String::formatted("{:x}", hash_value));
}

ErrorOr<Optional<PolicyGraph::Policy>> PolicyGraph::match_policy(ThreatMetadata const& threat)
{
    // Try to generate cache key - if it fails, we skip caching but continue
    Optional<String> cache_key;
    auto cache_key_result = compute_cache_key(threat);
    if (cache_key_result.is_error()) {
        // Cache key generation failed - skip cache, go straight to database query
        dbgln("PolicyGraph: Cache key generation failed: {}, falling back to database query",
              cache_key_result.error());
        // cache_key remains empty, so cache operations will be skipped
    } else {
        cache_key = cache_key_result.release_value();

        // Check cache if key is available
        auto cached_result = m_cache.get_cached(cache_key.value());
        if (cached_result.has_value()) {
            // Cache hit
            auto policy_id = cached_result.value();
            if (!policy_id.has_value()) {
                // Cached "no match"
                return Optional<Policy> {};
            }

            // Cached policy ID - fetch and return policy
            auto policy_result = get_policy(policy_id.value());
            if (policy_result.is_error()) {
                // Policy was deleted or invalid, invalidate cache entry
                m_cache.cache_policy(cache_key.value(), {});
                // Fall through to normal query
            } else {
                // Update hit statistics
                auto now = UnixDateTime::now().milliseconds_since_epoch();
                m_database->execute_statement(m_statements.increment_hit_count, {}, now, policy_id.value());
                return policy_result.value();
            }
        }
    }

    // Cache miss or cache skipped - perform database query
    auto now = UnixDateTime::now().milliseconds_since_epoch();

    // Priority 1: Match by file hash (most specific)
    if (!threat.file_hash.is_empty()) {
        Optional<Policy> match;
        m_database->execute_statement(
            m_statements.match_by_hash,
            [&](auto statement_id) {
                Policy policy;
                int col = 0;
                policy.id = m_database->result_column<i64>(statement_id, col++);
                policy.rule_name = m_database->result_column<String>(statement_id, col++);

                auto url_pattern = m_database->result_column<String>(statement_id, col++);
                if (!url_pattern.is_empty())
                    policy.url_pattern = url_pattern;

                auto file_hash = m_database->result_column<String>(statement_id, col++);
                if (!file_hash.is_empty())
                    policy.file_hash = file_hash;

                auto mime_type = m_database->result_column<String>(statement_id, col++);
                if (!mime_type.is_empty())
                    policy.mime_type = mime_type;

                auto action_str = m_database->result_column<String>(statement_id, col++);
                policy.action = string_to_action(action_str);

                policy.created_at = m_database->result_column<UnixDateTime>(statement_id, col++);
                policy.created_by = m_database->result_column<String>(statement_id, col++);

                auto expires_ms = m_database->result_column<i64>(statement_id, col++);
                if (expires_ms > 0)
                    policy.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

                policy.hit_count = m_database->result_column<i64>(statement_id, col++);

                auto last_hit_ms = m_database->result_column<i64>(statement_id, col++);
                if (last_hit_ms > 0)
                    policy.last_hit = UnixDateTime::from_milliseconds_since_epoch(last_hit_ms);

                match = policy;
            },
            threat.file_hash,
            now
        );

        if (match.has_value()) {
            // Update hit statistics
            m_database->execute_statement(m_statements.increment_hit_count, {}, now, match->id);
            // Cache the match (only if cache key is available)
            if (cache_key.has_value())
                m_cache.cache_policy(cache_key.value(), match->id);
            return match;
        }
    }

    // Priority 2: Match by URL pattern
    {
        Optional<Policy> match;
        m_database->execute_statement(
            m_statements.match_by_url_pattern,
            [&](auto statement_id) {
                Policy policy;
                int col = 0;
                policy.id = m_database->result_column<i64>(statement_id, col++);
                policy.rule_name = m_database->result_column<String>(statement_id, col++);

                auto url_pattern = m_database->result_column<String>(statement_id, col++);
                if (!url_pattern.is_empty())
                    policy.url_pattern = url_pattern;

                auto file_hash = m_database->result_column<String>(statement_id, col++);
                if (!file_hash.is_empty())
                    policy.file_hash = file_hash;

                auto mime_type = m_database->result_column<String>(statement_id, col++);
                if (!mime_type.is_empty())
                    policy.mime_type = mime_type;

                auto action_str = m_database->result_column<String>(statement_id, col++);
                policy.action = string_to_action(action_str);

                policy.created_at = m_database->result_column<UnixDateTime>(statement_id, col++);
                policy.created_by = m_database->result_column<String>(statement_id, col++);

                auto expires_ms = m_database->result_column<i64>(statement_id, col++);
                if (expires_ms > 0)
                    policy.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

                policy.hit_count = m_database->result_column<i64>(statement_id, col++);

                auto last_hit_ms = m_database->result_column<i64>(statement_id, col++);
                if (last_hit_ms > 0)
                    policy.last_hit = UnixDateTime::from_milliseconds_since_epoch(last_hit_ms);

                match = policy;
            },
            threat.url,
            now
        );

        if (match.has_value()) {
            m_database->execute_statement(m_statements.increment_hit_count, {}, now, match->id);
            // Cache the match (only if cache key is available)
            if (cache_key.has_value())
                m_cache.cache_policy(cache_key.value(), match->id);
            return match;
        }
    }

    // Priority 3: Match by rule name (least specific)
    {
        Optional<Policy> match;
        m_database->execute_statement(
            m_statements.match_by_rule_name,
            [&](auto statement_id) {
                Policy policy;
                int col = 0;
                policy.id = m_database->result_column<i64>(statement_id, col++);
                policy.rule_name = m_database->result_column<String>(statement_id, col++);

                auto url_pattern = m_database->result_column<String>(statement_id, col++);
                if (!url_pattern.is_empty())
                    policy.url_pattern = url_pattern;

                auto file_hash = m_database->result_column<String>(statement_id, col++);
                if (!file_hash.is_empty())
                    policy.file_hash = file_hash;

                auto mime_type = m_database->result_column<String>(statement_id, col++);
                if (!mime_type.is_empty())
                    policy.mime_type = mime_type;

                auto action_str = m_database->result_column<String>(statement_id, col++);
                policy.action = string_to_action(action_str);

                policy.created_at = m_database->result_column<UnixDateTime>(statement_id, col++);
                policy.created_by = m_database->result_column<String>(statement_id, col++);

                auto expires_ms = m_database->result_column<i64>(statement_id, col++);
                if (expires_ms > 0)
                    policy.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

                policy.hit_count = m_database->result_column<i64>(statement_id, col++);

                auto last_hit_ms = m_database->result_column<i64>(statement_id, col++);
                if (last_hit_ms > 0)
                    policy.last_hit = UnixDateTime::from_milliseconds_since_epoch(last_hit_ms);

                match = policy;
            },
            threat.rule_name,
            now
        );

        if (match.has_value()) {
            m_database->execute_statement(m_statements.increment_hit_count, {}, now, match->id);
            // Cache the match (only if cache key is available)
            if (cache_key.has_value())
                m_cache.cache_policy(cache_key.value(), match->id);
            return match;
        }
    }

    // No matching policy found - cache the miss (only if cache key is available)
    if (cache_key.has_value())
        m_cache.cache_policy(cache_key.value(), {});
    return Optional<Policy> {};
}

// Threat history implementations

ErrorOr<void> PolicyGraph::record_threat(ThreatMetadata const& threat,
                                        String action_taken,
                                        Optional<i64> policy_id,
                                        String alert_json)
{
    m_database->execute_statement(
        m_statements.record_threat,
        {},
        UnixDateTime::now().milliseconds_since_epoch(),
        threat.url,
        threat.filename,
        threat.file_hash,
        threat.mime_type,
        threat.file_size,
        threat.rule_name,
        threat.severity,
        action_taken,
        policy_id.has_value() ? policy_id.value() : -1,
        alert_json
    );

    return {};
}

ErrorOr<Vector<PolicyGraph::ThreatRecord>> PolicyGraph::get_threat_history(Optional<UnixDateTime> since)
{
    Vector<ThreatRecord> threats;

    if (since.has_value()) {
        m_database->execute_statement(
            m_statements.get_threats_since,
            [&](auto stmt_id) {
                ThreatRecord record;
                int col = 0;
                record.id = m_database->result_column<i64>(stmt_id, col++);
                record.detected_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
                record.url = m_database->result_column<String>(stmt_id, col++);
                record.filename = m_database->result_column<String>(stmt_id, col++);
                record.file_hash = m_database->result_column<String>(stmt_id, col++);
                record.mime_type = m_database->result_column<String>(stmt_id, col++);
                record.file_size = m_database->result_column<u64>(stmt_id, col++);
                record.rule_name = m_database->result_column<String>(stmt_id, col++);
                record.severity = m_database->result_column<String>(stmt_id, col++);
                record.action_taken = m_database->result_column<String>(stmt_id, col++);

                auto policy_id = m_database->result_column<i64>(stmt_id, col++);
                if (policy_id > 0)
                    record.policy_id = policy_id;

                record.alert_json = m_database->result_column<String>(stmt_id, col++);

                threats.append(move(record));
            },
            since->milliseconds_since_epoch()
        );
    } else {
        m_database->execute_statement(
            m_statements.get_threats_all,
            [&](auto stmt_id) {
                ThreatRecord record;
                int col = 0;
                record.id = m_database->result_column<i64>(stmt_id, col++);
                record.detected_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
                record.url = m_database->result_column<String>(stmt_id, col++);
                record.filename = m_database->result_column<String>(stmt_id, col++);
                record.file_hash = m_database->result_column<String>(stmt_id, col++);
                record.mime_type = m_database->result_column<String>(stmt_id, col++);
                record.file_size = m_database->result_column<u64>(stmt_id, col++);
                record.rule_name = m_database->result_column<String>(stmt_id, col++);
                record.severity = m_database->result_column<String>(stmt_id, col++);
                record.action_taken = m_database->result_column<String>(stmt_id, col++);

                auto policy_id = m_database->result_column<i64>(stmt_id, col++);
                if (policy_id > 0)
                    record.policy_id = policy_id;

                record.alert_json = m_database->result_column<String>(stmt_id, col++);

                threats.append(move(record));
            }
        );
    }

    return threats;
}

ErrorOr<Vector<PolicyGraph::ThreatRecord>> PolicyGraph::get_threats_by_rule(String const& rule_name)
{
    Vector<ThreatRecord> threats;

    m_database->execute_statement(
        m_statements.get_threats_by_rule,
        [&](auto stmt_id) {
            ThreatRecord record;
            int col = 0;
            record.id = m_database->result_column<i64>(stmt_id, col++);
            record.detected_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
            record.url = m_database->result_column<String>(stmt_id, col++);
            record.filename = m_database->result_column<String>(stmt_id, col++);
            record.file_hash = m_database->result_column<String>(stmt_id, col++);
            record.mime_type = m_database->result_column<String>(stmt_id, col++);
            record.file_size = m_database->result_column<u64>(stmt_id, col++);
            record.rule_name = m_database->result_column<String>(stmt_id, col++);
            record.severity = m_database->result_column<String>(stmt_id, col++);
            record.action_taken = m_database->result_column<String>(stmt_id, col++);

            auto policy_id = m_database->result_column<i64>(stmt_id, col++);
            if (policy_id > 0)
                record.policy_id = policy_id;

            record.alert_json = m_database->result_column<String>(stmt_id, col++);

            threats.append(move(record));
        },
        rule_name
    );

    return threats;
}

// Utility implementations

ErrorOr<void> PolicyGraph::cleanup_expired_policies()
{
    auto now = UnixDateTime::now().milliseconds_since_epoch();
    m_database->execute_statement(m_statements.delete_expired_policies, {}, now);

    // Invalidate cache since policies changed
    m_cache.invalidate();

    return {};
}

ErrorOr<u64> PolicyGraph::get_policy_count()
{
    u64 count = 0;
    m_database->execute_statement(
        m_statements.count_policies,
        [&](auto statement_id) {
            count = m_database->result_column<u64>(statement_id, 0);
        }
    );
    return count;
}

ErrorOr<u64> PolicyGraph::get_threat_count()
{
    u64 count = 0;
    m_database->execute_statement(
        m_statements.count_threats,
        [&](auto statement_id) {
            count = m_database->result_column<u64>(statement_id, 0);
        }
    );
    return count;
}

// Utility conversion functions

PolicyGraph::PolicyAction PolicyGraph::string_to_action(String const& action_str)
{
    if (action_str == "allow"sv)
        return PolicyAction::Allow;
    if (action_str == "block"sv)
        return PolicyAction::Block;
    if (action_str == "quarantine"sv)
        return PolicyAction::Quarantine;
    if (action_str == "block_autofill"sv)
        return PolicyAction::BlockAutofill;
    if (action_str == "warn_user"sv)
        return PolicyAction::WarnUser;

    return PolicyAction::Block; // Default to block for safety
}

String PolicyGraph::action_to_string(PolicyAction action)
{
    switch (action) {
    case PolicyAction::Allow:
        return "allow"_string;
    case PolicyAction::Block:
        return "block"_string;
    case PolicyAction::Quarantine:
        return "quarantine"_string;
    case PolicyAction::BlockAutofill:
        return "block_autofill"_string;
    case PolicyAction::WarnUser:
        return "warn_user"_string;
    }
    VERIFY_NOT_REACHED();
}

PolicyGraph::PolicyMatchType PolicyGraph::string_to_match_type(String const& type_str)
{
    if (type_str == "download"sv)
        return PolicyMatchType::DownloadOriginFileType;
    if (type_str == "form_mismatch"sv)
        return PolicyMatchType::FormActionMismatch;
    if (type_str == "insecure_cred"sv)
        return PolicyMatchType::InsecureCredentialPost;
    if (type_str == "third_party_form"sv)
        return PolicyMatchType::ThirdPartyFormPost;

    return PolicyMatchType::DownloadOriginFileType; // Default
}

String PolicyGraph::match_type_to_string(PolicyMatchType type)
{
    switch (type) {
    case PolicyMatchType::DownloadOriginFileType:
        return "download"_string;
    case PolicyMatchType::FormActionMismatch:
        return "form_mismatch"_string;
    case PolicyMatchType::InsecureCredentialPost:
        return "insecure_cred"_string;
    case PolicyMatchType::ThirdPartyFormPost:
        return "third_party_form"_string;
    }
    VERIFY_NOT_REACHED();
}

// Memory optimization implementations

ErrorOr<void> PolicyGraph::cleanup_old_threats(u64 days_to_keep)
{
    // Calculate cutoff timestamp
    auto now = UnixDateTime::now();
    auto cutoff_timestamp = now.seconds_since_epoch() - (days_to_keep * 24 * 60 * 60);
    auto cutoff_ms = cutoff_timestamp * 1000;

    dbgln("PolicyGraph: Cleaning up threats older than {} days (cutoff: {})", days_to_keep, cutoff_ms);

    // Delete old threats
    m_database->execute_statement(m_statements.delete_old_threats, {}, cutoff_ms);

    return {};
}

ErrorOr<void> PolicyGraph::vacuum_database()
{
    dbgln("PolicyGraph: Vacuuming database to reclaim space");

    // VACUUM command compacts the SQLite database
    auto vacuum_stmt = TRY(m_database->prepare_statement("VACUUM;"sv));
    m_database->execute_statement(vacuum_stmt, {});

    return {};
}

// Database health and error recovery

ErrorOr<void> PolicyGraph::verify_database_integrity()
{
    dbgln("PolicyGraph: Verifying database integrity");

    // Use SQLite's PRAGMA integrity_check
    auto integrity_stmt = TRY(m_database->prepare_statement("PRAGMA integrity_check;"sv));

    bool is_ok = false;
    m_database->execute_statement(
        integrity_stmt,
        [&](auto statement_id) {
            auto result = m_database->result_column<String>(statement_id, 0);
            if (result == "ok"sv) {
                is_ok = true;
            } else {
                dbgln("PolicyGraph: Integrity check failed: {}", result);
            }
        }
    );

    if (!is_ok) {
        m_database_healthy = false;
        return Error::from_string_literal("Database integrity check failed");
    }

    m_database_healthy = true;
    dbgln("PolicyGraph: Database integrity verified");
    return {};
}

bool PolicyGraph::is_database_healthy()
{
    // Quick health check without full integrity verification
    if (!m_database_healthy) {
        return false;
    }

    // Try a simple query to test database responsiveness
    auto result = get_policy_count();
    if (result.is_error()) {
        dbgln("PolicyGraph: Database health check failed: {}", result.error());
        m_database_healthy = false;
        return false;
    }

    return true;
}

// Transaction support implementations

ErrorOr<void> PolicyGraph::begin_transaction()
{
    dbgln("PolicyGraph: Beginning transaction");

    // Execute BEGIN TRANSACTION
    // Note: execute_statement uses SQL_MUST which crashes on error
    // We rely on SQLite's transaction semantics for safety
    m_database->execute_statement(m_statements.begin_transaction, {});

    return {};
}

ErrorOr<void> PolicyGraph::commit_transaction()
{
    dbgln("PolicyGraph: Committing transaction");

    // Execute COMMIT TRANSACTION
    m_database->execute_statement(m_statements.commit_transaction, {});

    // Invalidate cache since database state changed
    m_cache.invalidate();

    return {};
}

ErrorOr<void> PolicyGraph::rollback_transaction()
{
    dbgln("PolicyGraph: Rolling back transaction");

    // Execute ROLLBACK TRANSACTION
    m_database->execute_statement(m_statements.rollback_transaction, {});

    // Invalidate cache since we're reverting changes
    m_cache.invalidate();

    return {};
}

// Milestone 0.3: Credential Relationship Management

ErrorOr<i64> PolicyGraph::create_relationship(CredentialRelationship const& relationship)
{
    m_database->execute_statement(
        m_statements.create_relationship,
        {},
        relationship.form_origin,
        relationship.action_origin,
        relationship.relationship_type,
        relationship.created_at.milliseconds_since_epoch(),
        relationship.created_by,
        relationship.expires_at.has_value() ? relationship.expires_at->milliseconds_since_epoch() : -1,
        relationship.notes
    );

    i64 relationship_id = -1;
    m_database->execute_statement(
        m_statements.get_last_insert_id,
        [&](auto statement_id) {
            relationship_id = m_database->result_column<i64>(statement_id, 0);
        }
    );

    dbgln("PolicyGraph: Created credential relationship {} ({} -> {})", relationship_id, relationship.form_origin, relationship.action_origin);
    return relationship_id;
}

ErrorOr<PolicyGraph::CredentialRelationship> PolicyGraph::get_relationship(String const& form_origin, String const& action_origin, String const& type)
{
    CredentialRelationship relationship;
    bool found = false;

    m_database->execute_statement(
        m_statements.get_relationship,
        [&](auto stmt_id) {
            found = true;
            int col = 0;
            relationship.id = m_database->result_column<i64>(stmt_id, col++);
            relationship.form_origin = m_database->result_column<String>(stmt_id, col++);
            relationship.action_origin = m_database->result_column<String>(stmt_id, col++);
            relationship.relationship_type = m_database->result_column<String>(stmt_id, col++);
            relationship.created_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
            relationship.created_by = m_database->result_column<String>(stmt_id, col++);

            auto last_used_ms = m_database->result_column<i64>(stmt_id, col++);
            if (last_used_ms > 0)
                relationship.last_used = UnixDateTime::from_milliseconds_since_epoch(last_used_ms);

            relationship.use_count = m_database->result_column<i64>(stmt_id, col++);

            auto expires_ms = m_database->result_column<i64>(stmt_id, col++);
            if (expires_ms > 0)
                relationship.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

            relationship.notes = m_database->result_column<String>(stmt_id, col++);
        },
        form_origin,
        action_origin,
        type
    );

    if (!found)
        return Error::from_string_literal("Relationship not found");

    return relationship;
}

ErrorOr<Vector<PolicyGraph::CredentialRelationship>> PolicyGraph::list_relationships(Optional<String> type_filter)
{
    Vector<CredentialRelationship> relationships;

    auto statement_id = type_filter.has_value()
        ? m_statements.list_relationships_filtered
        : m_statements.list_relationships;

    m_database->execute_statement(
        statement_id,
        [&](auto stmt_id) {
            CredentialRelationship relationship;
            int col = 0;
            relationship.id = m_database->result_column<i64>(stmt_id, col++);
            relationship.form_origin = m_database->result_column<String>(stmt_id, col++);
            relationship.action_origin = m_database->result_column<String>(stmt_id, col++);
            relationship.relationship_type = m_database->result_column<String>(stmt_id, col++);
            relationship.created_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
            relationship.created_by = m_database->result_column<String>(stmt_id, col++);

            auto last_used_ms = m_database->result_column<i64>(stmt_id, col++);
            if (last_used_ms > 0)
                relationship.last_used = UnixDateTime::from_milliseconds_since_epoch(last_used_ms);

            relationship.use_count = m_database->result_column<i64>(stmt_id, col++);

            auto expires_ms = m_database->result_column<i64>(stmt_id, col++);
            if (expires_ms > 0)
                relationship.expires_at = UnixDateTime::from_milliseconds_since_epoch(expires_ms);

            relationship.notes = m_database->result_column<String>(stmt_id, col++);

            relationships.append(move(relationship));
        },
        type_filter.has_value() ? type_filter.value() : ""_string
    );

    return relationships;
}

ErrorOr<void> PolicyGraph::update_relationship_usage(i64 relationship_id)
{
    auto now = UnixDateTime::now();

    m_database->execute_statement(
        m_statements.update_relationship_usage,
        {},
        now.milliseconds_since_epoch(),
        relationship_id
    );

    return {};
}

ErrorOr<void> PolicyGraph::delete_relationship(i64 relationship_id)
{
    m_database->execute_statement(
        m_statements.delete_relationship,
        {},
        relationship_id
    );

    dbgln("PolicyGraph: Deleted credential relationship {}", relationship_id);
    return {};
}

ErrorOr<bool> PolicyGraph::has_relationship(String const& form_origin, String const& action_origin, String const& type)
{
    bool exists = false;

    m_database->execute_statement(
        m_statements.has_relationship,
        [&](auto) {
            exists = true;
        },
        form_origin,
        action_origin,
        type
    );

    return exists;
}

// Milestone 0.3: Credential Alert History

ErrorOr<i64> PolicyGraph::record_credential_alert(CredentialAlert const& alert)
{
    m_database->execute_statement(
        m_statements.record_credential_alert,
        {},
        alert.detected_at.milliseconds_since_epoch(),
        alert.form_origin,
        alert.action_origin,
        alert.alert_type,
        alert.severity,
        alert.has_password_field ? 1 : 0,
        alert.has_email_field ? 1 : 0,
        alert.uses_https ? 1 : 0,
        alert.is_cross_origin ? 1 : 0,
        alert.user_action.has_value() ? alert.user_action.value() : ""_string,
        alert.policy_id.has_value() ? alert.policy_id.value() : -1,
        alert.alert_json
    );

    i64 alert_id = -1;
    m_database->execute_statement(
        m_statements.get_last_insert_id,
        [&](auto statement_id) {
            alert_id = m_database->result_column<i64>(statement_id, 0);
        }
    );

    dbgln("PolicyGraph: Recorded credential alert {} ({} -> {})", alert_id, alert.form_origin, alert.action_origin);
    return alert_id;
}

ErrorOr<Vector<PolicyGraph::CredentialAlert>> PolicyGraph::get_credential_alerts(Optional<UnixDateTime> since)
{
    Vector<CredentialAlert> alerts;

    auto statement_id = since.has_value()
        ? m_statements.get_credential_alerts_since
        : m_statements.get_credential_alerts_all;

    m_database->execute_statement(
        statement_id,
        [&](auto stmt_id) {
            CredentialAlert alert;
            int col = 0;
            alert.id = m_database->result_column<i64>(stmt_id, col++);
            alert.detected_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
            alert.form_origin = m_database->result_column<String>(stmt_id, col++);
            alert.action_origin = m_database->result_column<String>(stmt_id, col++);
            alert.alert_type = m_database->result_column<String>(stmt_id, col++);
            alert.severity = m_database->result_column<String>(stmt_id, col++);
            alert.has_password_field = m_database->result_column<i64>(stmt_id, col++) != 0;
            alert.has_email_field = m_database->result_column<i64>(stmt_id, col++) != 0;
            alert.uses_https = m_database->result_column<i64>(stmt_id, col++) != 0;
            alert.is_cross_origin = m_database->result_column<i64>(stmt_id, col++) != 0;

            auto user_action_str = m_database->result_column<String>(stmt_id, col++);
            if (!user_action_str.is_empty())
                alert.user_action = user_action_str;

            auto policy_id = m_database->result_column<i64>(stmt_id, col++);
            if (policy_id > 0)
                alert.policy_id = policy_id;

            alert.alert_json = m_database->result_column<String>(stmt_id, col++);

            alerts.append(move(alert));
        },
        since.has_value() ? since->milliseconds_since_epoch() : 0
    );

    return alerts;
}

ErrorOr<Vector<PolicyGraph::CredentialAlert>> PolicyGraph::get_alerts_by_origin(String const& origin)
{
    Vector<CredentialAlert> alerts;

    m_database->execute_statement(
        m_statements.get_alerts_by_origin,
        [&](auto stmt_id) {
            CredentialAlert alert;
            int col = 0;
            alert.id = m_database->result_column<i64>(stmt_id, col++);
            alert.detected_at = m_database->result_column<UnixDateTime>(stmt_id, col++);
            alert.form_origin = m_database->result_column<String>(stmt_id, col++);
            alert.action_origin = m_database->result_column<String>(stmt_id, col++);
            alert.alert_type = m_database->result_column<String>(stmt_id, col++);
            alert.severity = m_database->result_column<String>(stmt_id, col++);
            alert.has_password_field = m_database->result_column<i64>(stmt_id, col++) != 0;
            alert.has_email_field = m_database->result_column<i64>(stmt_id, col++) != 0;
            alert.uses_https = m_database->result_column<i64>(stmt_id, col++) != 0;
            alert.is_cross_origin = m_database->result_column<i64>(stmt_id, col++) != 0;

            auto user_action_str = m_database->result_column<String>(stmt_id, col++);
            if (!user_action_str.is_empty())
                alert.user_action = user_action_str;

            auto policy_id = m_database->result_column<i64>(stmt_id, col++);
            if (policy_id > 0)
                alert.policy_id = policy_id;

            alert.alert_json = m_database->result_column<String>(stmt_id, col++);

            alerts.append(move(alert));
        },
        origin
    );

    return alerts;
}

ErrorOr<void> PolicyGraph::update_alert_action(i64 alert_id, String const& user_action)
{
    m_database->execute_statement(
        m_statements.update_alert_action,
        {},
        user_action,
        alert_id
    );

    return {};
}

// Milestone 0.3: Policy Templates

ErrorOr<i64> PolicyGraph::create_template(PolicyTemplate const& tmpl)
{
    m_database->execute_statement(
        m_statements.create_template,
        {},
        tmpl.name,
        tmpl.description,
        tmpl.category,
        tmpl.template_json,
        tmpl.is_builtin ? 1 : 0,
        tmpl.created_at.milliseconds_since_epoch(),
        tmpl.updated_at.has_value() ? tmpl.updated_at->milliseconds_since_epoch() : -1
    );

    i64 template_id = -1;
    m_database->execute_statement(
        m_statements.get_last_insert_id,
        [&](auto statement_id) {
            template_id = m_database->result_column<i64>(statement_id, 0);
        }
    );

    dbgln("PolicyGraph: Created template {} ({})", template_id, tmpl.name);
    return template_id;
}

ErrorOr<PolicyGraph::PolicyTemplate> PolicyGraph::get_template(i64 template_id)
{
    PolicyTemplate tmpl;
    bool found = false;

    m_database->execute_statement(
        m_statements.get_template,
        [&](auto stmt_id) {
            found = true;
            int col = 0;
            tmpl.id = m_database->result_column<i64>(stmt_id, col++);
            tmpl.name = m_database->result_column<String>(stmt_id, col++);
            tmpl.description = m_database->result_column<String>(stmt_id, col++);
            tmpl.category = m_database->result_column<String>(stmt_id, col++);
            tmpl.template_json = m_database->result_column<String>(stmt_id, col++);
            tmpl.is_builtin = m_database->result_column<i64>(stmt_id, col++) != 0;
            tmpl.created_at = m_database->result_column<UnixDateTime>(stmt_id, col++);

            auto updated_ms = m_database->result_column<i64>(stmt_id, col++);
            if (updated_ms > 0)
                tmpl.updated_at = UnixDateTime::from_milliseconds_since_epoch(updated_ms);
        },
        template_id
    );

    if (!found)
        return Error::from_string_literal("Template not found");

    return tmpl;
}

ErrorOr<PolicyGraph::PolicyTemplate> PolicyGraph::get_template_by_name(String const& name)
{
    PolicyTemplate tmpl;
    bool found = false;

    m_database->execute_statement(
        m_statements.get_template_by_name,
        [&](auto stmt_id) {
            found = true;
            int col = 0;
            tmpl.id = m_database->result_column<i64>(stmt_id, col++);
            tmpl.name = m_database->result_column<String>(stmt_id, col++);
            tmpl.description = m_database->result_column<String>(stmt_id, col++);
            tmpl.category = m_database->result_column<String>(stmt_id, col++);
            tmpl.template_json = m_database->result_column<String>(stmt_id, col++);
            tmpl.is_builtin = m_database->result_column<i64>(stmt_id, col++) != 0;
            tmpl.created_at = m_database->result_column<UnixDateTime>(stmt_id, col++);

            auto updated_ms = m_database->result_column<i64>(stmt_id, col++);
            if (updated_ms > 0)
                tmpl.updated_at = UnixDateTime::from_milliseconds_since_epoch(updated_ms);
        },
        name
    );

    if (!found)
        return Error::from_string_literal("Template not found");

    return tmpl;
}

ErrorOr<Vector<PolicyGraph::PolicyTemplate>> PolicyGraph::list_templates(Optional<String> category_filter)
{
    Vector<PolicyTemplate> templates;

    auto statement_id = category_filter.has_value()
        ? m_statements.list_templates_filtered
        : m_statements.list_templates_all;

    m_database->execute_statement(
        statement_id,
        [&](auto stmt_id) {
            PolicyTemplate tmpl;
            int col = 0;
            tmpl.id = m_database->result_column<i64>(stmt_id, col++);
            tmpl.name = m_database->result_column<String>(stmt_id, col++);
            tmpl.description = m_database->result_column<String>(stmt_id, col++);
            tmpl.category = m_database->result_column<String>(stmt_id, col++);
            tmpl.template_json = m_database->result_column<String>(stmt_id, col++);
            tmpl.is_builtin = m_database->result_column<i64>(stmt_id, col++) != 0;
            tmpl.created_at = m_database->result_column<UnixDateTime>(stmt_id, col++);

            auto updated_ms = m_database->result_column<i64>(stmt_id, col++);
            if (updated_ms > 0)
                tmpl.updated_at = UnixDateTime::from_milliseconds_since_epoch(updated_ms);

            templates.append(move(tmpl));
        },
        category_filter.has_value() ? category_filter.value() : ""_string
    );

    return templates;
}

ErrorOr<void> PolicyGraph::update_template(i64 template_id, PolicyTemplate const& tmpl)
{
    auto now = UnixDateTime::now();

    m_database->execute_statement(
        m_statements.update_template,
        {},
        tmpl.name,
        tmpl.description,
        tmpl.category,
        tmpl.template_json,
        now.milliseconds_since_epoch(),
        template_id
    );

    dbgln("PolicyGraph: Updated template {}", template_id);
    return {};
}

ErrorOr<void> PolicyGraph::delete_template(i64 template_id)
{
    m_database->execute_statement(
        m_statements.delete_template,
        {},
        template_id
    );

    dbgln("PolicyGraph: Deleted template {}", template_id);
    return {};
}

ErrorOr<PolicyGraph::Policy> PolicyGraph::instantiate_template(i64 template_id, HashMap<String, String> const& variables)
{
    auto tmpl = TRY(get_template(template_id));

    auto template_json = tmpl.template_json;
    for (auto const& [key, value] : variables) {
        auto placeholder = MUST(String::formatted("{{{}}}", key));
        template_json = MUST(template_json.replace(placeholder, value, ReplaceMode::All));
    }

    auto json = TRY(JsonValue::from_string(template_json));
    if (!json.is_object())
        return Error::from_string_literal("Invalid template JSON");

    auto const& obj = json.as_object();

    Policy policy;
    policy.rule_name = obj.get_string("rule_name"sv).value_or("Instantiated Policy"_string);

    if (obj.has_string("url_pattern"sv))
        policy.url_pattern = obj.get_string("url_pattern"sv).value();

    if (obj.has_string("file_hash"sv))
        policy.file_hash = obj.get_string("file_hash"sv).value();

    if (obj.has_string("mime_type"sv))
        policy.mime_type = obj.get_string("mime_type"sv).value();

    auto action_str = obj.get_string("action"sv).value_or("allow"_string);
    policy.action = string_to_action(action_str);

    auto match_type_str = obj.get_string("match_type"sv).value_or("download"_string);
    policy.match_type = string_to_match_type(match_type_str);

    policy.enforcement_action = obj.get_string("enforcement_action"sv).value_or(""_string);
    policy.created_at = UnixDateTime::now();
    policy.created_by = "template"_string;

    return policy;
}

// Milestone 0.3: Import/Export

ErrorOr<String> PolicyGraph::export_relationships_json()
{
    auto relationships = TRY(list_relationships({}));

    JsonArray json_array;
    for (auto const& rel : relationships) {
        JsonObject obj;
        obj.set("form_origin"sv, rel.form_origin);
        obj.set("action_origin"sv, rel.action_origin);
        obj.set("relationship_type"sv, rel.relationship_type);
        obj.set("created_at"sv, rel.created_at.milliseconds_since_epoch());
        obj.set("created_by"sv, rel.created_by);

        if (rel.last_used.has_value())
            obj.set("last_used"sv, rel.last_used->milliseconds_since_epoch());

        obj.set("use_count"sv, rel.use_count);

        if (rel.expires_at.has_value())
            obj.set("expires_at"sv, rel.expires_at->milliseconds_since_epoch());

        obj.set("notes"sv, rel.notes);

        json_array.must_append(move(obj));
    }

    return JsonValue(json_array).serialized();
}

ErrorOr<void> PolicyGraph::import_relationships_json(String const& json)
{
    auto parsed = TRY(JsonValue::from_string(json));
    if (!parsed.is_array())
        return Error::from_string_literal("Invalid JSON: expected array");

    auto const& array = parsed.as_array();
    for (auto const& item : array.values()) {
        if (!item.is_object())
            continue;

        auto const& obj = item.as_object();

        CredentialRelationship rel;
        rel.form_origin = obj.get_string("form_origin"sv).value_or(""_string);
        rel.action_origin = obj.get_string("action_origin"sv).value_or(""_string);
        rel.relationship_type = obj.get_string("relationship_type"sv).value_or(""_string);
        rel.created_at = UnixDateTime::from_milliseconds_since_epoch(obj.get_i64("created_at"sv).value_or(0));
        rel.created_by = obj.get_string("created_by"sv).value_or("import"_string);

        if (obj.has_i64("last_used"sv))
            rel.last_used = UnixDateTime::from_milliseconds_since_epoch(obj.get_i64("last_used"sv).value());

        rel.use_count = obj.get_i64("use_count"sv).value_or(0);

        if (obj.has_i64("expires_at"sv))
            rel.expires_at = UnixDateTime::from_milliseconds_since_epoch(obj.get_i64("expires_at"sv).value());

        rel.notes = obj.get_string("notes"sv).value_or(""_string);

        auto existing = has_relationship(rel.form_origin, rel.action_origin, rel.relationship_type);
        if (!existing.is_error() && !existing.value()) {
            TRY(create_relationship(rel));
        }
    }

    return {};
}

ErrorOr<String> PolicyGraph::export_templates_json()
{
    auto templates = TRY(list_templates({}));

    JsonArray json_array;
    for (auto const& tmpl : templates) {
        JsonObject obj;
        obj.set("name"sv, tmpl.name);
        obj.set("description"sv, tmpl.description);
        obj.set("category"sv, tmpl.category);
        obj.set("template_json"sv, tmpl.template_json);
        obj.set("is_builtin"sv, tmpl.is_builtin);
        obj.set("created_at"sv, tmpl.created_at.milliseconds_since_epoch());

        if (tmpl.updated_at.has_value())
            obj.set("updated_at"sv, tmpl.updated_at->milliseconds_since_epoch());

        json_array.must_append(move(obj));
    }

    return JsonValue(json_array).serialized();
}

ErrorOr<void> PolicyGraph::import_templates_json(String const& json)
{
    auto parsed = TRY(JsonValue::from_string(json));
    if (!parsed.is_array())
        return Error::from_string_literal("Invalid JSON: expected array");

    auto const& array = parsed.as_array();
    for (auto const& item : array.values()) {
        if (!item.is_object())
            continue;

        auto const& obj = item.as_object();

        PolicyTemplate tmpl;
        tmpl.name = obj.get_string("name"sv).value_or(""_string);
        tmpl.description = obj.get_string("description"sv).value_or(""_string);
        tmpl.category = obj.get_string("category"sv).value_or(""_string);
        tmpl.template_json = obj.get_string("template_json"sv).value_or("{}"_string);
        tmpl.is_builtin = obj.get_bool("is_builtin"sv).value_or(false);
        tmpl.created_at = UnixDateTime::from_milliseconds_since_epoch(obj.get_i64("created_at"sv).value_or(0));

        if (obj.has_i64("updated_at"sv))
            tmpl.updated_at = UnixDateTime::from_milliseconds_since_epoch(obj.get_i64("updated_at"sv).value());

        auto existing = get_template_by_name(tmpl.name);
        if (existing.is_error()) {
            TRY(create_template(tmpl));
        }
    }

    return {};
}

}
