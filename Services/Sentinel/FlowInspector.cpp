/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FlowInspector.h"
#include "PolicyGraph.h"
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>

namespace Sentinel {

FlowInspector::FlowInspector(PolicyGraph* policy_graph)
    : m_policy_graph(policy_graph)
{
    // Load existing trusted relationships from database if policy_graph exists
    if (m_policy_graph) {
        auto result = load_trusted_relationships();
        if (result.is_error()) {
            dbgln("FlowInspector: Failed to load trusted relationships: {}", result.error());
        }
    }
}

ErrorOr<Optional<FlowInspector::CredentialAlert>> FlowInspector::analyze_form_submission(FormSubmissionEvent const& event)
{
    // Check if this is a trusted relationship
    if (is_trusted_relationship(event.form_origin, event.action_origin)) {
        dbgln("FlowInspector: Form submission from {} to {} is trusted",
              event.form_origin, event.action_origin);
        return Optional<CredentialAlert> {};
    }

    // Generate alert based on event characteristics
    auto alert = generate_alert(event);
    if (!alert.has_value())
        return Optional<CredentialAlert> {};

    // Store alert in memory
    m_alerts.append(alert.value());

    dbgln("FlowInspector: Generated {} alert for form submission",
          severity_to_string(alert->severity));
    dbgln("  Form origin: {}", alert->form_origin);
    dbgln("  Action origin: {}", alert->action_origin);
    dbgln("  Alert type: {}", alert_type_to_string(alert->alert_type));

    return alert;
}

Vector<FlowInspector::DetectionRule> FlowInspector::create_detection_rules() const
{
    Vector<DetectionRule> rules;

    // Rule 1: Insecure credential post (Critical severity)
    rules.append({
        .name = MUST(String::from_utf8("insecure_credential_post"sv)),
        .type = AlertType::InsecureCredentialPost,
        .severity = AlertSeverity::Critical,
        .predicate = [](FormSubmissionEvent const& e) {
            return e.has_password_field && !e.uses_https;
        }
    });

    // Rule 2: Cross-origin credential exfiltration (High severity)
    rules.append({
        .name = MUST(String::from_utf8("cross_origin_credential_post"sv)),
        .type = AlertType::CredentialExfiltration,
        .severity = AlertSeverity::High,
        .predicate = [](FormSubmissionEvent const& e) {
            return e.is_cross_origin && e.has_password_field && e.uses_https;
        }
    });

    // Rule 3: Third-party form post with email (Medium severity)
    rules.append({
        .name = MUST(String::from_utf8("third_party_form_post"sv)),
        .type = AlertType::ThirdPartyFormPost,
        .severity = AlertSeverity::Medium,
        .predicate = [](FormSubmissionEvent const& e) {
            return e.is_cross_origin && e.has_email_field && !e.has_password_field;
        }
    });

    // Rule 4: Form action mismatch (Low severity)
    rules.append({
        .name = MUST(String::from_utf8("form_action_mismatch"sv)),
        .type = AlertType::FormActionMismatch,
        .severity = AlertSeverity::Low,
        .predicate = [](FormSubmissionEvent const& e) {
            return e.is_cross_origin && !e.has_password_field && !e.has_email_field;
        }
    });

    return rules;
}

Optional<FlowInspector::CredentialAlert> FlowInspector::generate_alert(FormSubmissionEvent const& event) const
{
    // Only alert if there are password or email fields, or cross-origin
    if (!event.has_password_field && !event.has_email_field && !event.is_cross_origin)
        return {};

    // Get detection rules
    auto rules = create_detection_rules();

    // Find first matching rule
    for (auto const& rule : rules) {
        if (rule.predicate(event)) {
            CredentialAlert alert;
            alert.timestamp = UnixDateTime::now();
            alert.form_origin = event.form_origin;
            alert.action_origin = event.action_origin;
            alert.uses_https = event.uses_https;
            alert.has_password_field = event.has_password_field;
            alert.is_cross_origin = event.is_cross_origin;
            alert.alert_type = rule.type;
            alert.severity = rule.severity;
            alert.description = generate_description(alert);

            return alert;
        }
    }

    return {};
}

// Removed old if-else detection methods - now using rule-based system in generate_alert()

String FlowInspector::generate_description(CredentialAlert const& alert) const
{
    StringBuilder desc;

    switch (alert.alert_type) {
    case AlertType::InsecureCredentialPost:
        desc.append("Password field submitted over insecure HTTP connection"sv);
        break;
    case AlertType::CredentialExfiltration:
        desc.append("Password field submitted to different origin: "sv);
        desc.append(alert.action_origin);
        break;
    case AlertType::ThirdPartyFormPost:
        desc.append("Form data submitted to third-party domain: "sv);
        desc.append(alert.action_origin);
        break;
    case AlertType::FormActionMismatch:
        desc.append("Form action URL differs from page origin"sv);
        break;
    }

    return MUST(desc.to_string());
}

ErrorOr<void> FlowInspector::learn_trusted_relationship(String const& form_origin, String const& action_origin)
{
    // Create or update trusted relationship
    TrustedFormRelationship relationship;
    relationship.form_origin = form_origin;
    relationship.action_origin = action_origin;
    relationship.learned_at = UnixDateTime::now();
    relationship.submission_count = 1;

    // Store in hash map keyed by form_origin
    if (!m_trusted_relationships.contains(form_origin)) {
        m_trusted_relationships.set(form_origin, Vector<TrustedFormRelationship> {});
    }

    auto& relationships = m_trusted_relationships.get(form_origin).value();

    // Check if relationship already exists
    bool found = false;
    for (auto& rel : relationships) {
        if (rel.action_origin == action_origin) {
            rel.submission_count++;
            // Calculate confidence: increases with submission count, capped at 1.0
            // Formula: min(1.0, submission_count / 10.0)
            // 1 submission = 0.1, 10 submissions = 1.0
            rel.confidence_score = min(1.0, static_cast<double>(rel.submission_count) / 10.0);
            found = true;
            break;
        }
    }

    if (!found) {
        // Initial confidence for new relationship
        relationship.confidence_score = 0.1; // Start at 10%
        relationships.append(relationship);
    }

    dbgln("FlowInspector: Learned trusted relationship: {} -> {}", form_origin, action_origin);

    // Persist to database if PolicyGraph is available
    if (m_policy_graph) {
        auto persist_result = persist_trusted_relationship(relationship);
        if (persist_result.is_error()) {
            dbgln("FlowInspector: Failed to persist trusted relationship: {}", persist_result.error());
        }
    }

    return {};
}

bool FlowInspector::is_trusted_relationship(String const& form_origin, String const& action_origin) const
{
    auto it = m_trusted_relationships.find(form_origin);
    if (it == m_trusted_relationships.end())
        return false;

    auto const& relationships = it->value;
    for (auto const& rel : relationships) {
        if (rel.action_origin == action_origin)
            return true;
    }

    return false;
}

Vector<FlowInspector::CredentialAlert> const& FlowInspector::get_recent_alerts(Optional<UnixDateTime>) const
{
    // For now, return all alerts
    // In full implementation, would filter by timestamp
    return m_alerts;
}

void FlowInspector::cleanup_old_alerts(u64 hours_to_keep)
{
    auto now = UnixDateTime::now();
    auto cutoff = now.seconds_since_epoch() - (hours_to_keep * 3600);

    // Remove alerts older than cutoff
    m_alerts.remove_all_matching([cutoff](auto const& alert) {
        return static_cast<u64>(alert.timestamp.seconds_since_epoch()) < cutoff;
    });

    dbgln("FlowInspector: Cleaned up old alerts (kept {} hours)", hours_to_keep);
}

// Static conversion functions

String FlowInspector::CredentialAlert::to_json() const
{
    JsonObject json;
    json.set("alert_type"sv, FlowInspector::alert_type_to_string(alert_type));
    json.set("severity"sv, FlowInspector::severity_to_string(severity));
    json.set("form_origin"sv, form_origin);
    json.set("action_origin"sv, action_origin);
    json.set("uses_https"sv, uses_https);
    json.set("has_password_field"sv, has_password_field);
    json.set("is_cross_origin"sv, is_cross_origin);
    json.set("timestamp"sv, MUST(String::formatted("{}", timestamp.seconds_since_epoch())));
    json.set("description"sv, description);

    StringBuilder builder;
    json.serialize(builder);
    return MUST(builder.to_string());
}

String FlowInspector::alert_type_to_string(AlertType type)
{
    switch (type) {
    case AlertType::CredentialExfiltration:
        return MUST(String::from_utf8("credential_exfiltration"sv));
    case AlertType::FormActionMismatch:
        return MUST(String::from_utf8("form_action_mismatch"sv));
    case AlertType::InsecureCredentialPost:
        return MUST(String::from_utf8("insecure_credential_post"sv));
    case AlertType::ThirdPartyFormPost:
        return MUST(String::from_utf8("third_party_form_post"sv));
    }
    VERIFY_NOT_REACHED();
}

String FlowInspector::severity_to_string(AlertSeverity severity)
{
    switch (severity) {
    case AlertSeverity::Low:
        return MUST(String::from_utf8("low"sv));
    case AlertSeverity::Medium:
        return MUST(String::from_utf8("medium"sv));
    case AlertSeverity::High:
        return MUST(String::from_utf8("high"sv));
    case AlertSeverity::Critical:
        return MUST(String::from_utf8("critical"sv));
    }
    VERIFY_NOT_REACHED();
}

ErrorOr<void> FlowInspector::persist_trusted_relationship(TrustedFormRelationship const& relationship)
{
    if (!m_policy_graph)
        return Error::from_string_literal("PolicyGraph not available");

    // Create a policy to represent this trusted relationship
    PolicyGraph::Policy policy;
    policy.rule_name = MUST(String::formatted("trusted_form_{}_{}", relationship.form_origin, relationship.action_origin));
    policy.url_pattern = relationship.form_origin;
    policy.action = PolicyGraph::PolicyAction::Allow;
    policy.match_type = PolicyGraph::PolicyMatchType::FormActionMismatch;
    policy.enforcement_action = MUST(String::formatted("allow_form_post_to:{}", relationship.action_origin));
    policy.created_at = relationship.learned_at;
    policy.created_by = "FlowInspector"_string;
    policy.hit_count = relationship.submission_count;

    TRY(m_policy_graph->create_policy(policy));

    dbgln("FlowInspector: Persisted trusted relationship to PolicyGraph");
    return {};
}

ErrorOr<void> FlowInspector::load_trusted_relationships()
{
    if (!m_policy_graph)
        return Error::from_string_literal("PolicyGraph not available");

    // Query all FormActionMismatch policies from database
    auto policies = TRY(m_policy_graph->list_policies());

    for (auto const& policy : policies) {
        if (policy.match_type != PolicyGraph::PolicyMatchType::FormActionMismatch)
            continue;
        if (policy.action != PolicyGraph::PolicyAction::Allow)
            continue;

        // Parse the enforcement_action to extract action_origin
        // Format: "allow_form_post_to:<action_origin>"
        if (!policy.enforcement_action.starts_with_bytes("allow_form_post_to:"sv))
            continue;

        auto action_origin = MUST(policy.enforcement_action.substring_from_byte_offset(19)); // Skip "allow_form_post_to:"

        TrustedFormRelationship relationship;
        relationship.form_origin = policy.url_pattern.value_or(String {});
        relationship.action_origin = action_origin;
        relationship.learned_at = policy.created_at;
        relationship.submission_count = static_cast<u64>(policy.hit_count);
        // Calculate confidence from submission count
        relationship.confidence_score = min(1.0, static_cast<double>(relationship.submission_count) / 10.0);

        // Store in memory
        if (!m_trusted_relationships.contains(relationship.form_origin)) {
            m_trusted_relationships.set(relationship.form_origin, Vector<TrustedFormRelationship> {});
        }
        m_trusted_relationships.get(relationship.form_origin).value().append(relationship);
    }

    dbgln("FlowInspector: Loaded {} trusted relationships from PolicyGraph", m_trusted_relationships.size());
    return {};
}

bool FlowInspector::should_auto_trust(String const& form_origin, String const& action_origin) const
{
    auto confidence = get_relationship_confidence(form_origin, action_origin);
    return confidence >= 0.8; // Auto-trust if confidence >= 80%
}

double FlowInspector::get_relationship_confidence(String const& form_origin, String const& action_origin) const
{
    auto it = m_trusted_relationships.find(form_origin);
    if (it == m_trusted_relationships.end())
        return 0.0;

    auto const& relationships = it->value;
    for (auto const& rel : relationships) {
        if (rel.action_origin == action_origin)
            return rel.confidence_score;
    }

    return 0.0;
}

}
