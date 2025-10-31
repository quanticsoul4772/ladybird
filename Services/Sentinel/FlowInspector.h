/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>

namespace Sentinel {

// FlowInspector analyzes form submission patterns to detect credential exfiltration
class FlowInspector {
public:
    enum class AlertSeverity {
        Low,
        Medium,
        High,
        Critical
    };

    enum class AlertType {
        CredentialExfiltration,
        FormActionMismatch,
        InsecureCredentialPost,
        ThirdPartyFormPost
    };

    struct FormSubmissionEvent {
        String form_origin;
        String action_origin;
        bool has_password_field { false };
        bool has_email_field { false };
        bool uses_https { false };
        bool is_cross_origin { false };
        UnixDateTime timestamp;
    };

    struct CredentialAlert {
        AlertType alert_type;
        AlertSeverity severity;
        String form_origin;
        String action_origin;
        bool uses_https { false };
        bool has_password_field { false };
        bool is_cross_origin { false };
        UnixDateTime timestamp;
        String description;

        String to_json() const;
    };

    // Helper functions for conversion
    static String alert_type_to_string(AlertType type);
    static String severity_to_string(AlertSeverity severity);

    struct TrustedFormRelationship {
        String form_origin;
        String action_origin;
        UnixDateTime learned_at;
        u64 submission_count { 0 };
    };

    FlowInspector() = default;

    // Analyze form submission event and generate alert if suspicious
    ErrorOr<Optional<CredentialAlert>> analyze_form_submission(FormSubmissionEvent const& event);

    // Learn trusted form relationships (for future whitelist implementation)
    ErrorOr<void> learn_trusted_relationship(String const& form_origin, String const& action_origin);

    // Check if form relationship is trusted
    bool is_trusted_relationship(String const& form_origin, String const& action_origin) const;

    // Get all alerts generated in a time period
    Vector<CredentialAlert> const& get_recent_alerts(Optional<UnixDateTime> since = {}) const;

    // Clear old alerts from memory
    void cleanup_old_alerts(u64 hours_to_keep = 24);

private:
    // Generate alert based on event characteristics
    Optional<CredentialAlert> generate_alert(FormSubmissionEvent const& event) const;

    // Determine severity based on threat characteristics
    AlertSeverity determine_severity(FormSubmissionEvent const& event) const;

    // Determine alert type based on event
    AlertType determine_alert_type(FormSubmissionEvent const& event) const;

    // Generate human-readable description
    String generate_description(CredentialAlert const& alert) const;

    // Store alerts in memory (will be persisted to PolicyGraph in full implementation)
    Vector<CredentialAlert> m_alerts;

    // Store trusted relationships (whitelist)
    HashMap<String, Vector<TrustedFormRelationship>> m_trusted_relationships;
};

}
