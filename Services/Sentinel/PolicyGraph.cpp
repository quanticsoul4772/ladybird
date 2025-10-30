/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PolicyGraph.h"
#include <AK/StringBuilder.h>
#include <LibCore/StandardPaths.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>

namespace Sentinel {

// Security validation functions

static bool is_safe_url_pattern(StringView pattern)
{
    // Allow only: alphanumeric, /, -, _, ., *, %
    for (auto ch : pattern) {
        if (!isalnum(ch) && ch != '/' && ch != '-' && ch != '_' &&
            ch != '.' && ch != '*' && ch != '%')
            return false;
    }

    // Limit length to prevent DoS
    if (pattern.length() > 2048)
        return false;

    return true;
}

static ErrorOr<void> validate_policy_inputs(PolicyGraph::Policy const& policy)
{
    // Validate rule_name
    if (policy.rule_name.is_empty())
        return Error::from_string_literal("Invalid policy: rule_name cannot be empty");

    if (policy.rule_name.bytes().size() > 256)
        return Error::from_string_literal("Invalid policy: rule_name too long (max 256 bytes)");

    // Validate url_pattern if present
    if (policy.url_pattern.has_value()) {
        auto const& pattern = policy.url_pattern.value();
        if (!pattern.is_empty() && !is_safe_url_pattern(pattern))
            return Error::from_string_literal("Invalid policy: url_pattern contains unsafe characters");
    }

    // Validate file_hash if present
    if (policy.file_hash.has_value()) {
        auto const& hash = policy.file_hash.value();
        if (!hash.is_empty()) {
            // SHA256 hash should be 64 hex characters
            if (hash.bytes().size() > 128)
                return Error::from_string_literal("Invalid policy: file_hash too long (max 128 bytes)");

            // Verify hex characters only
            for (auto byte : hash.bytes()) {
                if (!isxdigit(byte))
                    return Error::from_string_literal("Invalid policy: file_hash must contain only hex characters");
            }
        }
    }

    // Validate mime_type if present
    if (policy.mime_type.has_value()) {
        auto const& mime = policy.mime_type.value();
        if (!mime.is_empty() && mime.bytes().size() > 256)
            return Error::from_string_literal("Invalid policy: mime_type too long (max 256 bytes)");
    }

    // Validate created_by
    if (policy.created_by.is_empty())
        return Error::from_string_literal("Invalid policy: created_by cannot be empty");

    if (policy.created_by.bytes().size() > 256)
        return Error::from_string_literal("Invalid policy: created_by too long (max 256 bytes)");

    // Validate enforcement_action
    if (policy.enforcement_action.bytes().size() > 256)
        return Error::from_string_literal("Invalid policy: enforcement_action too long (max 256 bytes)");

    return {};
}

// PolicyGraphCache implementation


Optional<Optional<int>> PolicyGraphCache::get_cached(String const& key)
{
    auto it = m_cache.find(key);
    if (it == m_cache.end()) {
        m_cache_misses++;
        return {};
    }

    // Cache hit
    m_cache_hits++;

    // Update LRU order
    update_lru(key);

    return it->value;
}

void PolicyGraphCache::cache_policy(String const& key, Optional<int> policy_id)
{
    // If cache is full, evict least recently used entry
    if (m_cache.size() >= m_max_size && !m_cache.contains(key)) {
        if (!m_lru_order.is_empty()) {
            auto lru_key = m_lru_order.take_first();
            m_cache.remove(lru_key);
            m_cache_evictions++;
        }
    }

    m_cache.set(key, policy_id);
    update_lru(key);
}

void PolicyGraphCache::invalidate()
{
    m_cache.clear();
    m_lru_order.clear();
    m_cache_invalidations++;
}

PolicyGraphCache::CacheMetrics PolicyGraphCache::get_metrics() const
{
    return CacheMetrics {
        .hits = m_cache_hits,
        .misses = m_cache_misses,
        .evictions = m_cache_evictions,
        .invalidations = m_cache_invalidations,
        .current_size = m_cache.size(),
        .max_size = m_max_size
    };
}

void PolicyGraphCache::reset_metrics()
{
    m_cache_hits = 0;
    m_cache_misses = 0;
    m_cache_evictions = 0;
    m_cache_invalidations = 0;
}

void PolicyGraphCache::update_lru(String const& key)
{
    // Remove key from current position
    m_lru_order.remove_all_matching([&](auto const& k) { return k == key; });

    // Add to end (most recently used)
    m_lru_order.append(key);
}

// PolicyGraph implementation

ErrorOr<PolicyGraph> PolicyGraph::create(ByteString const& db_directory)
{
    // Ensure directory exists with restrictive permissions (owner only)
    // SECURITY: Policy database contains threat history and user decisions
    if (!FileSystem::exists(db_directory))
        TRY(Core::System::mkdir(db_directory, 0700));

    // Create/open database
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
        "DELETE FROM policies WHERE expires_at IS NOT NULL AND expires_at <= ?;"sv));

    statements.count_policies = TRY(database->prepare_statement(
        "SELECT COUNT(*) FROM policies;"sv));

    statements.count_threats = TRY(database->prepare_statement(
        "SELECT COUNT(*) FROM threat_history;"sv));

    // Memory optimization statements
    statements.delete_old_threats = TRY(database->prepare_statement(
        "DELETE FROM threat_history WHERE detected_at < ?;"sv));

    auto policy_graph = PolicyGraph { move(database), statements };

    // Cleanup old threats on initialization
    auto cleanup_result = policy_graph.cleanup_old_threats();
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

    // Convert action enum to string
    auto action_str = action_to_string(policy.action);
    auto match_type_str = match_type_to_string(policy.match_type);

    // Convert optional expiration to i64 or null
    Optional<i64> expires_ms;
    if (policy.expires_at.has_value())
        expires_ms = policy.expires_at->milliseconds_since_epoch();

    // Execute insert with validated data
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

    // Get last insert ID
    i64 last_id = 0;
    m_database->execute_statement(
        m_statements.get_last_insert_id,
        [&](auto statement_id) {
            last_id = m_database->result_column<i64>(statement_id, 0);
        }
    );

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

String PolicyGraph::compute_cache_key(ThreatMetadata const& threat) const
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

    // Use the string's hash trait for a quick hash
    auto input_string = MUST(builder.to_string());
    auto hash_value = Traits<String>::hash(input_string);

    return MUST(String::formatted("{:x}", hash_value));
}

ErrorOr<Optional<PolicyGraph::Policy>> PolicyGraph::match_policy(ThreatMetadata const& threat)
{
    // Check cache first
    auto cache_key = compute_cache_key(threat);
    auto cached_result = m_cache.get_cached(cache_key);
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
            m_cache.cache_policy(cache_key, {});
            // Fall through to normal query
        } else {
            // Update hit statistics
            auto now = UnixDateTime::now().milliseconds_since_epoch();
            m_database->execute_statement(m_statements.increment_hit_count, {}, now, policy_id.value());
            return policy_result.value();
        }
    }

    // Cache miss - perform database query
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
            // Cache the match
            m_cache.cache_policy(cache_key, match->id);
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
            // Cache the match
            m_cache.cache_policy(cache_key, match->id);
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
            // Cache the match
            m_cache.cache_policy(cache_key, match->id);
            return match;
        }
    }

    // No matching policy found - cache the miss
    m_cache.cache_policy(cache_key, {});
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

}
