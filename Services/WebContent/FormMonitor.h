/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/HashTable.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibURL/URL.h>

namespace WebContent {

// FormMonitor tracks form submissions and detects potential credential exfiltration
class FormMonitor {
public:
    enum class FieldType {
        Text,
        Password,
        Email,
        Hidden,
        Other
    };

    struct FormField {
        String name;
        FieldType type;
        bool has_value { false };
    };

    struct FormSubmitEvent {
        URL::URL document_url;
        URL::URL action_url;
        String method;
        Vector<FormField> fields;
        bool has_password_field { false };
        bool has_email_field { false };
        UnixDateTime timestamp;
    };

    struct CredentialAlert {
        String alert_type;
        String severity;
        String form_origin;
        String action_origin;
        bool uses_https { false };
        bool has_password_field { false };
        bool is_cross_origin { false };
        UnixDateTime timestamp;

        String to_json() const;
    };

    FormMonitor() = default;

    // Called when a form is submitted
    void on_form_submit(FormSubmitEvent const& event);

    // Check if form submission is suspicious
    bool is_suspicious_submission(FormSubmitEvent const& event) const;

    // Generate alert for suspicious form submission
    Optional<CredentialAlert> analyze_submission(FormSubmitEvent const& event) const;

    // Learn trusted form relationship (user clicked "Trust" in alert dialog)
    void learn_trusted_relationship(String const& form_origin, String const& action_origin);

    // Check if relationship is trusted
    bool is_trusted_relationship(String const& form_origin, String const& action_origin) const;

    // Block form submission (user clicked "Block" in alert dialog)
    void block_submission(String const& form_origin, String const& action_origin);

    // Check if submission is blocked
    bool is_blocked_submission(String const& form_origin, String const& action_origin) const;

    // Extract origin from URL (scheme + host + port) - public for PageClient autofill check
    String extract_origin(URL::URL const& url) const;

    // Check if submission crosses origin boundaries - public for PageClient autofill check
    bool is_cross_origin_submission(URL::URL const& form_url, URL::URL const& action_url) const;

    // Autofill override mechanism (one-time permission for blocked autofill)
    void grant_autofill_override(String const& form_origin, String const& action_origin);
    bool has_autofill_override(String const& form_origin, String const& action_origin) const;
    void consume_autofill_override(String const& form_origin, String const& action_origin);

private:

    // Check if submission uses insecure transport for credentials
    bool is_insecure_credential_submission(FormSubmitEvent const& event) const;

    // Check if action URL points to known third-party tracking domains
    bool is_third_party_submission(URL::URL const& form_url, URL::URL const& action_url) const;

    // Store trusted form relationships (form_origin -> action_origin)
    HashMap<String, HashTable<String>> m_trusted_relationships;

    // Store blocked form submissions (form_origin -> action_origin)
    HashMap<String, HashTable<String>> m_blocked_submissions;

    // Store one-time autofill permissions (form_origin -> action_origin)
    // Cleared after first use to prevent persistent bypass of security checks
    HashMap<String, HashTable<String>> m_autofill_overrides;
};

}
