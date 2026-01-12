/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <Services/Sentinel/FlowInspector.h>
#include <Services/Sentinel/PolicyGraph.h>
#include <Services/WebContent/FormMonitor.h>

using namespace WebContent;
using namespace Sentinel;

TEST_CASE(test_legitimate_login_no_alert)
{
    FormMonitor monitor;

    // Create a legitimate login form submission (same origin)
    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::create_with_url_or_path("https://example.com/login"sv).value();
    event.action_url = URL::create_with_url_or_path("https://example.com/auth"sv).value();
    event.method = "POST"_string;
    event.has_password_field = true;
    event.has_email_field = true;
    event.timestamp = UnixDateTime::now();

    // This should not be flagged as suspicious
    EXPECT(!monitor.is_suspicious_submission(event));

    // No alert should be generated
    auto alert = monitor.analyze_submission(event);
    EXPECT(!alert.has_value());
}

TEST_CASE(test_cross_origin_form_submission_alert)
{
    FormMonitor monitor;

    // Create a cross-origin form submission
    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::create_with_url_or_path("https://example.com/login"sv).value();
    event.action_url = URL::create_with_url_or_path("https://malicious-site.ru/collect"sv).value();
    event.method = "POST"_string;
    event.has_password_field = true;
    event.has_email_field = true;
    event.timestamp = UnixDateTime::now();

    // This should be flagged as suspicious
    EXPECT(monitor.is_suspicious_submission(event));

    // Alert should be generated
    auto alert = monitor.analyze_submission(event);
    EXPECT(alert.has_value());
    EXPECT_EQ(alert->alert_type, "credential_exfiltration"_string);
    EXPECT_EQ(alert->severity, "high"_string);
    EXPECT(alert->is_cross_origin);
}

TEST_CASE(test_insecure_password_submission_alert)
{
    FormMonitor monitor;

    // Create a form submission over HTTP (not HTTPS)
    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::create_with_url_or_path("http://example.com/login"sv).value();
    event.action_url = URL::create_with_url_or_path("http://example.com/auth"sv).value();
    event.method = "POST"_string;
    event.has_password_field = true;
    event.has_email_field = false;
    event.timestamp = UnixDateTime::now();

    // This should be flagged as suspicious
    EXPECT(monitor.is_suspicious_submission(event));

    // Alert should be generated with critical severity
    auto alert = monitor.analyze_submission(event);
    EXPECT(alert.has_value());
    EXPECT_EQ(alert->alert_type, "insecure_credential_post"_string);
    EXPECT_EQ(alert->severity, "critical"_string);
    EXPECT(!alert->uses_https);
}

TEST_CASE(test_third_party_form_submission)
{
    FormMonitor monitor;

    // Create a third-party form submission
    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::create_with_url_or_path("https://example.com/page"sv).value();
    event.action_url = URL::create_with_url_or_path("https://tracking-service.com/collect"sv).value();
    event.method = "POST"_string;
    event.has_password_field = false;
    event.has_email_field = true;
    event.timestamp = UnixDateTime::now();

    // This should be flagged as suspicious
    EXPECT(monitor.is_suspicious_submission(event));

    // Alert should be generated
    auto alert = monitor.analyze_submission(event);
    EXPECT(alert.has_value());
    EXPECT_EQ(alert->alert_type, "third_party_form_post"_string);
    EXPECT_EQ(alert->severity, "medium"_string);
}

TEST_CASE(test_flow_inspector_analysis)
{
    FlowInspector inspector;

    // Create a suspicious form submission event
    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "https://example.com"_string;
    event.action_origin = "https://malicious-site.ru"_string;
    event.has_password_field = true;
    event.has_email_field = true;
    event.uses_https = true;
    event.is_cross_origin = true;
    event.timestamp = UnixDateTime::now();

    // Analyze the submission
    auto result = inspector.analyze_form_submission(event);
    EXPECT(!result.is_error());

    auto alert = result.value();
    EXPECT(alert.has_value());
    EXPECT_EQ(inspector.severity_to_string(alert->severity), "high"_string);
    EXPECT_EQ(inspector.alert_type_to_string(alert->alert_type), "credential_exfiltration"_string);
}

TEST_CASE(test_flow_inspector_trusted_relationship)
{
    FlowInspector inspector;

    // Learn a trusted relationship
    auto learn_result = inspector.learn_trusted_relationship(
        "https://example.com"_string,
        "https://auth.example.com"_string
    );
    EXPECT(!learn_result.is_error());

    // Verify it's trusted
    EXPECT(inspector.is_trusted_relationship(
        "https://example.com"_string,
        "https://auth.example.com"_string
    ));

    // Create an event for the trusted relationship
    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "https://example.com"_string;
    event.action_origin = "https://auth.example.com"_string;
    event.has_password_field = true;
    event.has_email_field = true;
    event.uses_https = true;
    event.is_cross_origin = true;
    event.timestamp = UnixDateTime::now();

    // No alert should be generated for trusted relationship
    auto result = inspector.analyze_form_submission(event);
    EXPECT(!result.is_error());
    EXPECT(!result.value().has_value());
}

TEST_CASE(test_alert_json_serialization)
{
    FlowInspector::CredentialAlert alert;
    alert.alert_type = FlowInspector::AlertType::CredentialExfiltration;
    alert.severity = FlowInspector::AlertSeverity::High;
    alert.form_origin = "https://example.com"_string;
    alert.action_origin = "https://malicious-site.ru"_string;
    alert.uses_https = true;
    alert.has_password_field = true;
    alert.is_cross_origin = true;
    alert.timestamp = UnixDateTime::now();
    alert.description = "Test alert"_string;

    // Serialize to JSON
    auto json = alert.to_json();

    // Verify JSON contains expected fields
    EXPECT(json.contains("alert_type"sv));
    EXPECT(json.contains("severity"sv));
    EXPECT(json.contains("form_origin"sv));
    EXPECT(json.contains("action_origin"sv));
    EXPECT(json.contains("uses_https"sv));
    EXPECT(json.contains("has_password_field"sv));
    EXPECT(json.contains("is_cross_origin"sv));
    EXPECT(json.contains("timestamp"sv));
    EXPECT(json.contains("description"sv));
}

TEST_CASE(test_policy_match_type_conversion)
{
    using namespace Sentinel;

    // Test all new policy match types
    auto form_mismatch = PolicyGraph::string_to_match_type("form_mismatch"_string);
    EXPECT(form_mismatch == PolicyGraph::PolicyMatchType::FormActionMismatch);

    auto insecure_cred = PolicyGraph::string_to_match_type("insecure_cred"_string);
    EXPECT(insecure_cred == PolicyGraph::PolicyMatchType::InsecureCredentialPost);

    auto third_party = PolicyGraph::string_to_match_type("third_party_form"_string);
    EXPECT(third_party == PolicyGraph::PolicyMatchType::ThirdPartyFormPost);

    // Test round-trip conversion
    auto mismatch_str = PolicyGraph::match_type_to_string(PolicyGraph::PolicyMatchType::FormActionMismatch);
    EXPECT(PolicyGraph::string_to_match_type(mismatch_str) == PolicyGraph::PolicyMatchType::FormActionMismatch);
}

TEST_CASE(test_policy_action_conversion)
{
    using namespace Sentinel;

    // Test new policy actions
    auto block_autofill = PolicyGraph::string_to_action("block_autofill"_string);
    EXPECT(block_autofill == PolicyGraph::PolicyAction::BlockAutofill);

    auto warn_user = PolicyGraph::string_to_action("warn_user"_string);
    EXPECT(warn_user == PolicyGraph::PolicyAction::WarnUser);

    // Test round-trip conversion
    auto autofill_str = PolicyGraph::action_to_string(PolicyGraph::PolicyAction::BlockAutofill);
    EXPECT_EQ(autofill_str, "block_autofill"_string);

    auto warn_str = PolicyGraph::action_to_string(PolicyGraph::PolicyAction::WarnUser);
    EXPECT_EQ(warn_str, "warn_user"_string);
}

TEST_CASE(test_alert_cleanup)
{
    FlowInspector inspector;

    // Add several alerts
    for (int i = 0; i < 5; i++) {
        FlowInspector::FormSubmissionEvent event;
        event.form_origin = "https://example.com"_string;
        event.action_origin = MUST(String::formatted("https://site{}.com", i));
        event.has_password_field = true;
        event.uses_https = true;
        event.is_cross_origin = true;
        event.timestamp = UnixDateTime::now();

        auto result = inspector.analyze_form_submission(event);
        EXPECT(!result.is_error());
    }

    // Verify we have alerts
    auto& alerts = inspector.get_recent_alerts();
    EXPECT(alerts.size() >= 5);

    // Cleanup old alerts (0 hours = cleanup all)
    inspector.cleanup_old_alerts(0);

    // All alerts should be removed
    auto& alerts_after = inspector.get_recent_alerts();
    EXPECT_EQ(alerts_after.size(), 0u);
}
