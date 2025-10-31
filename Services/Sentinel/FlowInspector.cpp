/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FlowInspector.h"
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>

namespace Sentinel {

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

Optional<FlowInspector::CredentialAlert> FlowInspector::generate_alert(FormSubmissionEvent const& event) const
{
    // Only alert if there are password or email fields
    if (!event.has_password_field && !event.has_email_field)
        return {};

    CredentialAlert alert;
    alert.timestamp = UnixDateTime::now();
    alert.form_origin = event.form_origin;
    alert.action_origin = event.action_origin;
    alert.uses_https = event.uses_https;
    alert.has_password_field = event.has_password_field;
    alert.is_cross_origin = event.is_cross_origin;

    // Determine alert type
    alert.alert_type = determine_alert_type(event);

    // Determine severity
    alert.severity = determine_severity(event);

    // Generate description
    alert.description = generate_description(alert);

    return alert;
}

FlowInspector::AlertType FlowInspector::determine_alert_type(FormSubmissionEvent const& event) const
{
    // Priority order: Insecure credential > Credential exfiltration > Third-party > Form mismatch

    if (event.has_password_field && !event.uses_https)
        return AlertType::InsecureCredentialPost;

    if (event.is_cross_origin && event.has_password_field)
        return AlertType::CredentialExfiltration;

    if (event.is_cross_origin && event.has_email_field)
        return AlertType::ThirdPartyFormPost;

    if (event.is_cross_origin)
        return AlertType::FormActionMismatch;

    // Default (should not reach here due to earlier checks)
    return AlertType::FormActionMismatch;
}

FlowInspector::AlertSeverity FlowInspector::determine_severity(FormSubmissionEvent const& event) const
{
    // Critical: Password sent over HTTP
    if (event.has_password_field && !event.uses_https)
        return AlertSeverity::Critical;

    // High: Password sent cross-origin
    if (event.is_cross_origin && event.has_password_field)
        return AlertSeverity::High;

    // Medium: Email or other credentials sent cross-origin
    if (event.is_cross_origin && event.has_email_field)
        return AlertSeverity::Medium;

    // Low: Cross-origin form submission without sensitive fields
    return AlertSeverity::Low;
}

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
            found = true;
            break;
        }
    }

    if (!found) {
        relationships.append(relationship);
    }

    dbgln("FlowInspector: Learned trusted relationship: {} -> {}", form_origin, action_origin);

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

}
