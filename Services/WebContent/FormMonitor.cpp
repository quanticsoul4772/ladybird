/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FormMonitor.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibURL/URL.h>
#include <Services/Sentinel/PolicyGraph.h>

namespace WebContent {

ErrorOr<NonnullOwnPtr<FormMonitor>> FormMonitor::create_with_policy_graph(ByteString const& db_directory)
{
    auto monitor = make<FormMonitor>();
    monitor->m_policy_graph = TRY(Sentinel::PolicyGraph::create(db_directory));
    TRY(monitor->load_relationships_from_database());
    return monitor;
}

ErrorOr<void> FormMonitor::load_relationships_from_database()
{
    if (!m_policy_graph)
        return {};

    // Load all credential relationships from database
    auto relationships = TRY(m_policy_graph->list_relationships({}));

    dbgln("FormMonitor: Loading {} credential relationships from database", relationships.size());

    for (auto const& rel : relationships) {
        if (rel.relationship_type == "trusted"sv) {
            // Add to in-memory cache for fast lookup
            if (!m_trusted_relationships.contains(rel.form_origin)) {
                m_trusted_relationships.set(rel.form_origin, HashTable<String> {});
            }
            auto& action_origins = m_trusted_relationships.get(rel.form_origin).value();
            action_origins.set(rel.action_origin);
        } else if (rel.relationship_type == "blocked"sv) {
            // Add to in-memory cache for fast lookup
            if (!m_blocked_submissions.contains(rel.form_origin)) {
                m_blocked_submissions.set(rel.form_origin, HashTable<String> {});
            }
            auto& action_origins = m_blocked_submissions.get(rel.form_origin).value();
            action_origins.set(rel.action_origin);
        }
    }

    dbgln("FormMonitor: Loaded {} trusted and {} blocked relationships",
          m_trusted_relationships.size(), m_blocked_submissions.size());

    return {};
}

void FormMonitor::on_form_submit(FormSubmitEvent const& event)
{
    // Milestone 0.3 Phase 6: Track submission timestamp for frequency analysis
    auto form_origin = extract_origin(event.document_url);
    if (!m_submission_timestamps.contains(form_origin)) {
        m_submission_timestamps.set(form_origin, Vector<UnixDateTime> {});
    }
    auto& timestamps = m_submission_timestamps.get(form_origin).value();
    timestamps.append(event.timestamp);

    // Keep only last 10 timestamps to prevent unbounded memory growth
    if (timestamps.size() > 10) {
        timestamps.remove(0);
    }

    // Check if submission is suspicious
    if (!is_suspicious_submission(event))
        return;

    // Analyze and generate alert if needed
    auto alert = analyze_submission(event);
    if (!alert.has_value())
        return;

    // Log the alert for now (will be sent to Sentinel via IPC in full implementation)
    dbgln("FormMonitor: Suspicious form submission detected");
    dbgln("  Form origin: {}", alert->form_origin);
    dbgln("  Action origin: {}", alert->action_origin);
    dbgln("  Severity: {}", alert->severity);
    dbgln("  Alert type: {}", alert->alert_type);

    // Milestone 0.3 Phase 6: Calculate anomaly score
    auto anomaly = calculate_anomaly_score(event);

    // Record alert in database if PolicyGraph is available
    if (m_policy_graph) {
        Sentinel::PolicyGraph::CredentialAlert db_alert;
        db_alert.detected_at = alert->timestamp;
        db_alert.form_origin = alert->form_origin;
        db_alert.action_origin = alert->action_origin;
        db_alert.alert_type = alert->alert_type;
        db_alert.severity = alert->severity;
        db_alert.has_password_field = alert->has_password_field;
        db_alert.has_email_field = false; // TODO: track email fields
        db_alert.uses_https = alert->uses_https;
        db_alert.is_cross_origin = alert->is_cross_origin;
        db_alert.alert_json = alert->to_json();

        // Milestone 0.3 Phase 6: Add anomaly data
        // Convert float score (0.0-1.0) to integer (0-1000) for database storage
        db_alert.anomaly_score = static_cast<i32>(anomaly.score * 1000.0f);

        // Convert anomaly indicators to JSON array string
        JsonArray indicators_array;
        for (auto const& indicator : anomaly.indicators) {
            indicators_array.must_append(indicator);
        }
        db_alert.anomaly_indicators = JsonValue(indicators_array).serialized();

        auto result = m_policy_graph->record_credential_alert(db_alert);
        if (result.is_error()) {
            dbgln("FormMonitor: Failed to record alert in database: {}", result.error());
        } else {
            dbgln("FormMonitor: Recorded alert {} in database with anomaly score {:.2f}", result.value(), anomaly.score);
        }
    }
}

bool FormMonitor::is_suspicious_submission(FormSubmitEvent const& event) const
{
    // Check if this is a trusted relationship first
    auto form_origin = extract_origin(event.document_url);
    auto action_origin = extract_origin(event.action_url);
    if (is_trusted_relationship(form_origin, action_origin)) {
        dbgln("FormMonitor: Form submission from {} to {} is trusted, skipping checks", form_origin, action_origin);
        return false;
    }

    // Check for password or email fields
    bool has_sensitive_fields = event.has_password_field || event.has_email_field;
    if (!has_sensitive_fields)
        return false;

    // Check for cross-origin submission
    if (is_cross_origin_submission(event.document_url, event.action_url))
        return true;

    // Check for insecure credential submission
    if (is_insecure_credential_submission(event))
        return true;

    // Check for third-party submission
    if (is_third_party_submission(event.document_url, event.action_url))
        return true;

    return false;
}

Optional<FormMonitor::CredentialAlert> FormMonitor::analyze_submission(FormSubmitEvent const& event) const
{
    CredentialAlert alert;
    alert.timestamp = UnixDateTime::now();
    alert.form_origin = extract_origin(event.document_url);
    alert.action_origin = extract_origin(event.action_url);
    alert.has_password_field = event.has_password_field;

    // Determine if action URL uses HTTPS
    alert.uses_https = event.action_url.scheme() == "https"sv;

    // Check if cross-origin
    alert.is_cross_origin = is_cross_origin_submission(event.document_url, event.action_url);

    // Determine alert type and severity based on detected issues
    if (event.has_password_field && !alert.uses_https) {
        alert.alert_type = "insecure_credential_post"_string;
        alert.severity = "critical"_string;
    } else if (alert.is_cross_origin && event.has_password_field) {
        alert.alert_type = "credential_exfiltration"_string;
        alert.severity = "high"_string;
    } else if (is_third_party_submission(event.document_url, event.action_url)) {
        alert.alert_type = "third_party_form_post"_string;
        alert.severity = "medium"_string;
    } else if (alert.is_cross_origin) {
        alert.alert_type = "form_action_mismatch"_string;
        alert.severity = "medium"_string;
    } else {
        // No specific threat detected
        return {};
    }

    return alert;
}

String FormMonitor::extract_origin(URL::URL const& url) const
{
    // Origin = scheme + "://" + host + (port if non-default)
    StringBuilder origin_builder;
    origin_builder.append(url.scheme());
    origin_builder.append("://"sv);
    origin_builder.append(url.host().map([](auto const& h) { return h.serialize(); }).value_or(String{}));

    // Include port if it's not the default for the scheme
    if (url.port().has_value()) {
        auto port = url.port().value();
        bool is_default_port = (url.scheme() == "http"sv && port == 80) ||
                               (url.scheme() == "https"sv && port == 443);

        if (!is_default_port) {
            origin_builder.append(':');
            origin_builder.append(String::number(port));
        }
    }

    return MUST(origin_builder.to_string());
}

bool FormMonitor::is_cross_origin_submission(URL::URL const& form_url, URL::URL const& action_url) const
{
    // Compare origins (scheme, host, port)
    if (form_url.scheme() != action_url.scheme())
        return true;

    if (form_url.host() != action_url.host())
        return true;

    // Check port (considering defaults)
    auto form_port = form_url.port().value_or(
        form_url.scheme() == "https"sv ? 443 : 80
    );
    auto action_port = action_url.port().value_or(
        action_url.scheme() == "https"sv ? 443 : 80
    );

    if (form_port != action_port)
        return true;

    return false;
}

bool FormMonitor::is_insecure_credential_submission(FormSubmitEvent const& event) const
{
    // Check if password is being sent over HTTP (not HTTPS)
    if (event.has_password_field && event.action_url.scheme() != "https"sv)
        return true;

    return false;
}

bool FormMonitor::is_third_party_submission(URL::URL const& form_url, URL::URL const& action_url) const
{
    // For now, we consider any cross-origin submission as potentially third-party
    // In a full implementation, we would check against a list of known tracking domains
    // or use eTLD+1 comparison to determine if domains are related

    auto form_host = form_url.host().map([](auto const& h) { return h.serialize(); }).value_or(String{});
    auto action_host = action_url.host().map([](auto const& h) { return h.serialize(); }).value_or(String{});

    // If hosts are different, it's a third-party submission
    if (form_host != action_host)
        return true;

    return false;
}

String FormMonitor::CredentialAlert::to_json() const
{
    JsonObject json;
    json.set("alert_type"sv, alert_type);
    json.set("severity"sv, severity);
    json.set("form_origin"sv, form_origin);
    json.set("action_origin"sv, action_origin);
    json.set("uses_https"sv, uses_https);
    json.set("has_password_field"sv, has_password_field);
    json.set("is_cross_origin"sv, is_cross_origin);
    json.set("timestamp"sv, MUST(String::formatted("{}", timestamp.seconds_since_epoch())));

    StringBuilder builder;
    json.serialize(builder);
    return MUST(builder.to_string());
}

void FormMonitor::learn_trusted_relationship(String const& form_origin, String const& action_origin)
{
    dbgln("FormMonitor: Learning trusted relationship from {} to {}", form_origin, action_origin);

    // Get or create the HashTable for this form_origin
    if (!m_trusted_relationships.contains(form_origin)) {
        m_trusted_relationships.set(form_origin, HashTable<String> {});
    }

    // Add the action_origin to the set
    auto& action_origins = m_trusted_relationships.get(form_origin).value();
    action_origins.set(action_origin);

    // Persist to database if PolicyGraph is available
    if (m_policy_graph) {
        Sentinel::PolicyGraph::CredentialRelationship relationship;
        relationship.form_origin = form_origin;
        relationship.action_origin = action_origin;
        relationship.relationship_type = "trusted"_string;
        relationship.created_at = UnixDateTime::now();
        relationship.created_by = "user_decision"_string;
        relationship.notes = "User clicked Trust in security alert"_string;

        auto result = m_policy_graph->create_relationship(relationship);
        if (result.is_error()) {
            dbgln("FormMonitor: Failed to persist trusted relationship: {}", result.error());
        } else {
            dbgln("FormMonitor: Persisted trusted relationship with ID {}", result.value());
        }
    }

    dbgln("FormMonitor: Trusted relationship learned. Now have {} trusted origins for {}",
          action_origins.size(), form_origin);
}

bool FormMonitor::is_trusted_relationship(String const& form_origin, String const& action_origin) const
{
    // Check in-memory cache first
    auto it = m_trusted_relationships.find(form_origin);
    if (it != m_trusted_relationships.end() && it->value.contains(action_origin))
        return true;

    // Check database if PolicyGraph is available
    if (m_policy_graph) {
        auto result = m_policy_graph->has_relationship(form_origin, action_origin, "trusted"_string);
        if (!result.is_error() && result.value()) {
            dbgln("FormMonitor: Found trusted relationship in database but not in cache");
            return true;
        }
    }

    return false;
}

void FormMonitor::block_submission(String const& form_origin, String const& action_origin)
{
    dbgln("FormMonitor: Blocking submission from {} to {}", form_origin, action_origin);

    // Get or create the HashTable for this form_origin
    if (!m_blocked_submissions.contains(form_origin)) {
        m_blocked_submissions.set(form_origin, HashTable<String> {});
    }

    // Add the action_origin to the blocked set
    auto& action_origins = m_blocked_submissions.get(form_origin).value();
    action_origins.set(action_origin);

    // Persist to database if PolicyGraph is available
    if (m_policy_graph) {
        Sentinel::PolicyGraph::CredentialRelationship relationship;
        relationship.form_origin = form_origin;
        relationship.action_origin = action_origin;
        relationship.relationship_type = "blocked"_string;
        relationship.created_at = UnixDateTime::now();
        relationship.created_by = "user_decision"_string;
        relationship.notes = "User clicked Block in security alert"_string;

        auto result = m_policy_graph->create_relationship(relationship);
        if (result.is_error()) {
            dbgln("FormMonitor: Failed to persist blocked relationship: {}", result.error());
        } else {
            dbgln("FormMonitor: Persisted blocked relationship with ID {}", result.value());
        }
    }

    dbgln("FormMonitor: Submission blocked. Now have {} blocked origins for {}",
          action_origins.size(), form_origin);
}

bool FormMonitor::is_blocked_submission(String const& form_origin, String const& action_origin) const
{
    // Check in-memory cache first
    auto it = m_blocked_submissions.find(form_origin);
    if (it != m_blocked_submissions.end() && it->value.contains(action_origin))
        return true;

    // Check database if PolicyGraph is available
    if (m_policy_graph) {
        auto result = m_policy_graph->has_relationship(form_origin, action_origin, "blocked"_string);
        if (!result.is_error() && result.value()) {
            dbgln("FormMonitor: Found blocked relationship in database but not in cache");
            return true;
        }
    }

    return false;
}

void FormMonitor::grant_autofill_override(String const& form_origin, String const& action_origin)
{
    dbgln("FormMonitor: Granting one-time autofill override from {} to {}", form_origin, action_origin);

    // Get or create the HashTable for this form_origin
    if (!m_autofill_overrides.contains(form_origin)) {
        m_autofill_overrides.set(form_origin, HashTable<String> {});
    }

    // Add the action_origin to the override set
    auto& action_origins = m_autofill_overrides.get(form_origin).value();
    action_origins.set(action_origin);

    dbgln("FormMonitor: One-time autofill override granted");
}

bool FormMonitor::has_autofill_override(String const& form_origin, String const& action_origin) const
{
    auto it = m_autofill_overrides.find(form_origin);
    if (it == m_autofill_overrides.end())
        return false;

    return it->value.contains(action_origin);
}

void FormMonitor::consume_autofill_override(String const& form_origin, String const& action_origin)
{
    dbgln("FormMonitor: Consuming one-time autofill override from {} to {}", form_origin, action_origin);

    auto it = m_autofill_overrides.find(form_origin);
    if (it == m_autofill_overrides.end())
        return;

    // Remove the action_origin from the override set
    it->value.remove(action_origin);

    // Clean up empty HashTable to prevent memory leak
    if (it->value.is_empty()) {
        m_autofill_overrides.remove(form_origin);
        dbgln("FormMonitor: Removed empty override set for {}", form_origin);
    }

    dbgln("FormMonitor: One-time autofill override consumed and removed");
}

// Milestone 0.3 Phase 6: Form Anomaly Detection Implementation

float FormMonitor::check_hidden_field_ratio(FormSubmitEvent const& event) const
{
    if (event.fields.is_empty())
        return 0.0f;

    size_t hidden_count = 0;
    for (auto const& field : event.fields) {
        if (field.type == FieldType::Hidden)
            hidden_count++;
    }

    float ratio = static_cast<float>(hidden_count) / static_cast<float>(event.fields.size());

    // Normalize to 0-1 score: ratios above 0.5 are highly suspicious
    if (ratio < 0.3f)
        return 0.0f;  // Normal
    if (ratio < 0.5f)
        return (ratio - 0.3f) / 0.2f * 0.5f;  // Slightly suspicious (0-0.5)

    return 0.5f + (ratio - 0.5f) / 0.5f * 0.5f;  // Very suspicious (0.5-1.0)
}

float FormMonitor::check_field_count(FormSubmitEvent const& event) const
{
    size_t field_count = event.fields.size();

    // Normal forms have 2-15 fields
    if (field_count <= 15)
        return 0.0f;

    // Data harvesting forms often have 20+ fields
    if (field_count <= 25)
        return (static_cast<float>(field_count) - 15.0f) / 10.0f * 0.5f;  // 0-0.5

    // Extremely suspicious: 25+ fields
    if (field_count <= 50)
        return 0.5f + (static_cast<float>(field_count) - 25.0f) / 25.0f * 0.5f;  // 0.5-1.0

    return 1.0f;  // 50+ fields is maximum suspicion
}

float FormMonitor::check_action_domain_reputation(URL::URL const& action_url) const
{
    auto host = action_url.host();
    if (!host.has_value())
        return 0.0f;

    auto host_str = host->serialize();

    // Check for known suspicious patterns
    Vector<StringView> suspicious_patterns = {
        "data-collect"sv,
        "analytics"sv,
        "tracking"sv,
        "logger"sv,
        "harvester"sv,
        "phishing"sv,
        "fake-"sv,
        "scam"sv,
        ".tk"sv,    // Free TLD often used for phishing
        ".ml"sv,    // Free TLD
        ".ga"sv,    // Free TLD
        ".cf"sv,    // Free TLD
        ".gq"sv     // Free TLD
    };

    for (auto const& pattern : suspicious_patterns) {
        if (host_str.contains(pattern))
            return 0.8f;  // High suspicion for known patterns
    }

    // Check for IP address instead of domain (common in phishing)
    // Simple heuristic: if it starts with a digit and contains dots, likely an IP
    auto host_view = host_str.bytes_as_string_view();
    if (!host_view.is_empty() && is_ascii_digit(host_view[0]) && host_view.contains('.'))
        return 0.7f;  // IP addresses are suspicious

    // Check for very long domains (possible typosquatting)
    if (host_str.bytes().size() > 40)
        return 0.6f;

    return 0.0f;  // No obvious red flags
}

float FormMonitor::check_submission_frequency(String const& form_origin) const
{
    auto it = m_submission_timestamps.find(form_origin);
    if (it == m_submission_timestamps.end())
        return 0.0f;  // First submission, not suspicious

    auto const& timestamps = it->value;
    if (timestamps.size() < 3)
        return 0.0f;  // Not enough data yet

    // Check for rapid-fire submissions in last 5 seconds (bot behavior)
    auto now = UnixDateTime::now();
    auto five_seconds_ago = UnixDateTime::from_milliseconds_since_epoch(now.milliseconds_since_epoch() - 5000);

    size_t recent_count = 0;
    for (auto const& timestamp : timestamps) {
        if (timestamp >= five_seconds_ago)
            recent_count++;
    }

    if (recent_count >= 5)
        return 1.0f;  // 5+ submissions in 5 seconds = bot
    if (recent_count >= 3)
        return 0.7f;  // 3-4 submissions in 5 seconds = suspicious

    // Check for consistent interval pattern (automated testing)
    if (timestamps.size() >= 5) {
        Vector<i64> intervals;
        for (size_t i = 1; i < timestamps.size(); ++i) {
            i64 interval = timestamps[i].milliseconds_since_epoch() - timestamps[i - 1].milliseconds_since_epoch();
            intervals.append(interval);
        }

        // Calculate variance in intervals
        i64 sum = 0;
        for (auto interval : intervals)
            sum += interval;
        i64 avg = sum / intervals.size();

        i64 variance_sum = 0;
        for (auto interval : intervals) {
            i64 diff = interval - avg;
            variance_sum += diff * diff;
        }

        // Low variance = consistent timing = automated
        if (variance_sum < 1000000)  // Less than 1 second variance
            return 0.6f;
    }

    return 0.0f;
}

FormMonitor::FormAnomalyScore FormMonitor::calculate_anomaly_score(FormSubmitEvent const& event) const
{
    float score = 0.0f;
    Vector<String> indicators;

    // 1. Hidden field ratio (weight: 0.3)
    float hidden_ratio = check_hidden_field_ratio(event);
    if (hidden_ratio > 0.5f) {
        score += 0.3f * hidden_ratio;
        indicators.append(MUST(String::formatted("High hidden field ratio ({:.1f}%)", hidden_ratio * 100.0f)));
    }

    // 2. Excessive fields (weight: 0.2)
    float field_score = check_field_count(event);
    if (field_score > 0.7f) {
        score += 0.2f * field_score;
        indicators.append(MUST(String::formatted("Excessive field count ({} fields)", event.fields.size())));
    }

    // 3. Domain reputation (weight: 0.3)
    float domain_score = check_action_domain_reputation(event.action_url);
    if (domain_score > 0.5f) {
        score += 0.3f * domain_score;
        auto host_str = event.action_url.host().has_value()
            ? event.action_url.host()->serialize()
            : "unknown"_string;
        indicators.append(MUST(String::formatted("Suspicious action domain: {}", host_str)));
    }

    // 4. Submission frequency (weight: 0.2)
    auto form_origin = extract_origin(event.document_url);
    float freq_score = check_submission_frequency(form_origin);
    if (freq_score > 0.8f) {
        score += 0.2f * freq_score;
        indicators.append("Unusual submission frequency detected"_string);
    }

    dbgln("FormMonitor: Anomaly score {:.2f} with {} indicators", score, indicators.size());

    return FormAnomalyScore { score, move(indicators) };
}

}
