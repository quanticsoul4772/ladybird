/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "FlowInspector.h"
#include <LibMain/Main.h>
#include <stdio.h>

using namespace Sentinel;

static void test_insecure_credential_post(FlowInspector& inspector)
{
    printf("\n=== Test: Insecure Credential Post Detection ===\n");

    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "http://example.com"_string;
    event.action_origin = "http://example.com"_string;
    event.has_password_field = true;
    event.has_email_field = true;
    event.uses_https = false;  // Insecure!
    event.is_cross_origin = false;
    event.timestamp = UnixDateTime::now();

    auto result = inspector.analyze_form_submission(event);
    if (result.is_error()) {
        printf("❌ FAILED: analyze_form_submission returned error: %s\n", result.error().string_literal().characters_without_null_termination());
        return;
    }

    if (!result.value().has_value()) {
        printf("❌ FAILED: Expected alert but got none\n");
        return;
    }

    auto const& alert = result.value().value();
    if (alert.alert_type != FlowInspector::AlertType::InsecureCredentialPost) {
        printf("❌ FAILED: Expected InsecureCredentialPost alert\n");
        return;
    }

    if (alert.severity != FlowInspector::AlertSeverity::Critical) {
        printf("❌ FAILED: Expected Critical severity\n");
        return;
    }

    printf("✅ PASSED: Detected insecure credential post with Critical severity\n");
}

static void test_cross_origin_exfiltration(FlowInspector& inspector)
{
    printf("\n=== Test: Cross-Origin Credential Exfiltration ===\n");

    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "https://example.com"_string;
    event.action_origin = "https://evil.com"_string;
    event.has_password_field = true;
    event.has_email_field = true;
    event.uses_https = true;
    event.is_cross_origin = true;  // Cross-origin exfiltration!
    event.timestamp = UnixDateTime::now();

    auto result = inspector.analyze_form_submission(event);
    if (result.is_error()) {
        printf("❌ FAILED: analyze_form_submission returned error\n");
        return;
    }

    if (!result.value().has_value()) {
        printf("❌ FAILED: Expected alert but got none\n");
        return;
    }

    auto const& alert = result.value().value();
    if (alert.alert_type != FlowInspector::AlertType::CredentialExfiltration) {
        printf("❌ FAILED: Expected CredentialExfiltration alert\n");
        return;
    }

    if (alert.severity != FlowInspector::AlertSeverity::High) {
        printf("❌ FAILED: Expected High severity\n");
        return;
    }

    printf("✅ PASSED: Detected cross-origin credential exfiltration with High severity\n");
}

static void test_trusted_relationship_learning(FlowInspector& inspector)
{
    printf("\n=== Test: Trusted Relationship Learning ===\n");

    auto form_origin = "https://example.com"_string;
    auto action_origin = "https://payment.example.com"_string;

    // Initially not trusted
    if (inspector.is_trusted_relationship(form_origin, action_origin)) {
        printf("❌ FAILED: Relationship should not be trusted initially\n");
        return;
    }

    // Learn the relationship
    auto result = inspector.learn_trusted_relationship(form_origin, action_origin);
    if (result.is_error()) {
        printf("❌ FAILED: Failed to learn relationship\n");
        return;
    }

    // Now it should be trusted
    if (!inspector.is_trusted_relationship(form_origin, action_origin)) {
        printf("❌ FAILED: Relationship should be trusted after learning\n");
        return;
    }

    // Verify confidence scoring
    auto confidence = inspector.get_relationship_confidence(form_origin, action_origin);
    if (confidence != 0.1) {
        printf("❌ FAILED: Initial confidence should be 0.1, got %f\n", confidence);
        return;
    }

    // Learn again multiple times to increase confidence
    for (int i = 0; i < 9; i++) {
        auto learn_result = inspector.learn_trusted_relationship(form_origin, action_origin);
        if (learn_result.is_error()) {
            printf("❌ FAILED: Failed to learn relationship on iteration %d\n", i);
            return;
        }
    }

    // After 10 submissions, confidence should be 1.0 (100%)
    confidence = inspector.get_relationship_confidence(form_origin, action_origin);
    if (confidence != 1.0) {
        printf("❌ FAILED: Confidence should be 1.0 after 10 submissions, got %f\n", confidence);
        return;
    }

    // Should auto-trust at 80%+ confidence
    if (!inspector.should_auto_trust(form_origin, action_origin)) {
        printf("❌ FAILED: Should auto-trust at 100%% confidence\n");
        return;
    }

    printf("✅ PASSED: Trusted relationship learning and confidence scoring working correctly\n");
}

static void test_no_alert_for_safe_submission(FlowInspector& inspector)
{
    printf("\n=== Test: No Alert for Safe Submission ===\n");

    // Same-origin HTTPS form with no sensitive fields - should NOT alert
    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "https://example.com"_string;
    event.action_origin = "https://example.com"_string;
    event.has_password_field = false;
    event.has_email_field = false;
    event.uses_https = true;
    event.is_cross_origin = false;
    event.timestamp = UnixDateTime::now();

    auto result = inspector.analyze_form_submission(event);
    if (result.is_error()) {
        printf("❌ FAILED: analyze_form_submission returned error\n");
        return;
    }

    if (result.value().has_value()) {
        printf("❌ FAILED: Should not generate alert for safe submission\n");
        return;
    }

    printf("✅ PASSED: No alert generated for safe same-origin submission\n");
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    printf("\n====================================\n");
    printf("  FlowInspector Test Suite\n");
    printf("====================================\n");

    FlowInspector inspector;

    test_insecure_credential_post(inspector);
    test_cross_origin_exfiltration(inspector);
    test_trusted_relationship_learning(inspector);
    test_no_alert_for_safe_submission(inspector);

    printf("\n====================================\n");
    printf("  All Tests Complete!\n");
    printf("====================================\n\n");

    return 0;
}
