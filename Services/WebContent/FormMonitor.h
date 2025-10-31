/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

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

private:
    // Extract origin from URL (scheme + host + port)
    String extract_origin(URL::URL const& url) const;

    // Check if submission crosses origin boundaries
    bool is_cross_origin_submission(URL::URL const& form_url, URL::URL const& action_url) const;

    // Check if submission uses insecure transport for credentials
    bool is_insecure_credential_submission(FormSubmitEvent const& event) const;

    // Check if action URL points to known third-party tracking domains
    bool is_third_party_submission(URL::URL const& form_url, URL::URL const& action_url) const;
};

}
