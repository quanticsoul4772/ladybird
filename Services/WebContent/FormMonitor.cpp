/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FormMonitor.h"
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

        auto result = m_policy_graph->record_credential_alert(db_alert);
        if (result.is_error()) {
            dbgln("FormMonitor: Failed to record alert in database: {}", result.error());
        } else {
            dbgln("FormMonitor: Recorded alert {} in database", result.value());
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

}
