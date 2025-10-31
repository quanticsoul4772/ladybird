/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Services/Sentinel/PolicyTemplates.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>

namespace Sentinel {

Vector<PolicyTemplateDefinition> PolicyTemplates::get_builtin_templates()
{
    Vector<PolicyTemplateDefinition> templates;

    // Template 1: Block All Cross-Origin Credential Submissions
    {
        JsonObject policy_obj;
        policy_obj.set("rule_name"sv, "Cross-Origin Credential Block"_string);
        policy_obj.set("match_type"sv, "FormActionMismatch"_string);
        policy_obj.set("action"sv, "Block"_string);
        policy_obj.set("enforcement_action"sv, "block_submission"_string);
        policy_obj.set("severity"sv, "high"_string);
        policy_obj.set("description"sv, "Blocks form submissions that send credentials to a different origin"_string);

        JsonArray policies;
        policies.must_append(policy_obj);

        JsonObject template_json;
        template_json.set("policies"sv, policies);

        PolicyTemplateDefinition template_def {
            .name = "Block All Cross-Origin Credentials"_string,
            .description = "Blocks all form submissions that send credentials to a different origin. This provides maximum protection against credential exfiltration attacks."_string,
            .category = "credential_protection"_string,
            .template_json = template_json.serialized(),
            .variables = {}
        };

        templates.append(move(template_def));
    }

    // Template 2: Warn on Insecure Credential Submissions
    {
        JsonObject policy_obj;
        policy_obj.set("rule_name"sv, "Insecure Credential Warning"_string);
        policy_obj.set("match_type"sv, "InsecureCredentialPost"_string);
        policy_obj.set("action"sv, "WarnUser"_string);
        policy_obj.set("enforcement_action"sv, "show_warning"_string);
        policy_obj.set("severity"sv, "medium"_string);
        policy_obj.set("description"sv, "Shows warning when credentials are submitted over unencrypted HTTP"_string);

        JsonArray policies;
        policies.must_append(policy_obj);

        JsonObject template_json;
        template_json.set("policies"sv, policies);

        PolicyTemplateDefinition template_def {
            .name = "Warn on HTTP Password Posts"_string,
            .description = "Shows warning when credentials are submitted over unencrypted HTTP. Helps users avoid exposing passwords on insecure connections."_string,
            .category = "credential_protection"_string,
            .template_json = template_json.serialized(),
            .variables = {}
        };

        templates.append(move(template_def));
    }

    // Template 3: Block Third-Party Tracking Forms
    {
        JsonObject policy_obj;
        policy_obj.set("rule_name"sv, "Third-Party Form Block"_string);
        policy_obj.set("match_type"sv, "ThirdPartyFormPost"_string);
        policy_obj.set("url_pattern"sv, "%(tracking_domain)s"_string);
        policy_obj.set("action"sv, "Block"_string);
        policy_obj.set("enforcement_action"sv, "block_submission"_string);
        policy_obj.set("severity"sv, "medium"_string);
        policy_obj.set("description"sv, "Blocks form submissions to known tracking and analytics domains"_string);

        JsonArray policies;
        policies.must_append(policy_obj);

        JsonObject template_json;
        template_json.set("policies"sv, policies);

        HashMap<String, String> variables;
        variables.set("tracking_domain"_string, "*.analytics.com|*.tracking.net|*.ads.example.com"_string);

        PolicyTemplateDefinition template_def {
            .name = "Block Third-Party Tracking"_string,
            .description = "Blocks form submissions to known tracking and analytics domains. Customize the tracking_domain variable to add your own blocklist."_string,
            .category = "credential_protection"_string,
            .template_json = template_json.serialized(),
            .variables = variables
        };

        templates.append(move(template_def));
    }

    // Template 4: Strict Credential Protection (combines multiple policies)
    {
        JsonArray policies;

        // Policy 1: Block cross-origin
        JsonObject policy1;
        policy1.set("rule_name"sv, "Strict Cross-Origin Block"_string);
        policy1.set("match_type"sv, "FormActionMismatch"_string);
        policy1.set("action"sv, "Block"_string);
        policy1.set("enforcement_action"sv, "block_submission"_string);
        policy1.set("severity"sv, "critical"_string);
        policies.must_append(policy1);

        // Policy 2: Block insecure
        JsonObject policy2;
        policy2.set("rule_name"sv, "Strict Insecure Block"_string);
        policy2.set("match_type"sv, "InsecureCredentialPost"_string);
        policy2.set("action"sv, "Block"_string);
        policy2.set("enforcement_action"sv, "block_submission"_string);
        policy2.set("severity"sv, "high"_string);
        policies.must_append(policy2);

        // Policy 3: Block third-party
        JsonObject policy3;
        policy3.set("rule_name"sv, "Strict Third-Party Block"_string);
        policy3.set("match_type"sv, "ThirdPartyFormPost"_string);
        policy3.set("action"sv, "Block"_string);
        policy3.set("enforcement_action"sv, "block_submission"_string);
        policy3.set("severity"sv, "high"_string);
        policies.must_append(policy3);

        JsonObject template_json;
        template_json.set("policies"sv, policies);

        PolicyTemplateDefinition template_def {
            .name = "Strict Credential Protection"_string,
            .description = "Maximum security template that blocks all cross-origin, insecure, and third-party credential submissions. Recommended for high-security environments."_string,
            .category = "credential_protection"_string,
            .template_json = template_json.serialized(),
            .variables = {}
        };

        templates.append(move(template_def));
    }

    // Template 5: Permissive with Warnings (warn-only mode)
    {
        JsonArray policies;

        // Policy 1: Warn on cross-origin
        JsonObject policy1;
        policy1.set("rule_name"sv, "Cross-Origin Warning"_string);
        policy1.set("match_type"sv, "FormActionMismatch"_string);
        policy1.set("action"sv, "WarnUser"_string);
        policy1.set("enforcement_action"sv, "show_warning"_string);
        policy1.set("severity"sv, "medium"_string);
        policies.must_append(policy1);

        // Policy 2: Warn on insecure
        JsonObject policy2;
        policy2.set("rule_name"sv, "Insecure Warning"_string);
        policy2.set("match_type"sv, "InsecureCredentialPost"_string);
        policy2.set("action"sv, "WarnUser"_string);
        policy2.set("enforcement_action"sv, "show_warning"_string);
        policy2.set("severity"sv, "low"_string);
        policies.must_append(policy2);

        JsonObject template_json;
        template_json.set("policies"sv, policies);

        PolicyTemplateDefinition template_def {
            .name = "Permissive with Warnings"_string,
            .description = "Warns users about suspicious credential submissions but allows them to proceed. Good for learning mode or low-friction environments."_string,
            .category = "credential_protection"_string,
            .template_json = template_json.serialized(),
            .variables = {}
        };

        templates.append(move(template_def));
    }

    return templates;
}

}
