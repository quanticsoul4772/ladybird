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

namespace WebContent {

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
}

bool FormMonitor::is_suspicious_submission(FormSubmitEvent const& event) const
{
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

}
