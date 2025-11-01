/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <LibCore/Resource.h>
#include <LibCore/StandardPaths.h>
#include <LibWebView/Application.h>
#include <LibWebView/WebUI/SecurityUI.h>

namespace WebView {

void SecurityUI::register_interfaces()
{
    // Initialize PolicyGraph with Ladybird data directory
    auto data_directory = ByteString::formatted("{}/Ladybird/PolicyGraph", Core::StandardPaths::user_data_directory());
    auto pg_result = Sentinel::PolicyGraph::create(data_directory);

    if (pg_result.is_error()) {
        dbgln("SecurityUI: Failed to initialize PolicyGraph: {}", pg_result.error());
        dbgln("SecurityUI: The about:security page will have limited functionality");
        // Send error notification to UI
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Database initialization failed: {}", pg_result.error()) });
        error.set("message"sv, JsonValue { "The security policy database could not be loaded. Some features may not work."sv });
        async_send_message("databaseError"sv, error);
    } else {
        m_policy_graph = move(pg_result);
        dbgln("SecurityUI: PolicyGraph initialized successfully");
    }

    // Register interfaces
    register_interface("getSystemStatus"sv, [this](auto const&) {
        get_system_status();
    });

    register_interface("loadStatistics"sv, [this](auto const&) {
        load_statistics();
    });

    register_interface("loadPolicies"sv, [this](auto const&) {
        load_policies();
    });

    register_interface("getPolicy"sv, [this](auto const& data) {
        get_policy(data);
    });

    register_interface("createPolicy"sv, [this](auto const& data) {
        create_policy(data);
    });

    register_interface("updatePolicy"sv, [this](auto const& data) {
        update_policy(data);
    });

    register_interface("deletePolicy"sv, [this](auto const& data) {
        delete_policy(data);
    });

    register_interface("loadThreatHistory"sv, [this](auto const& data) {
        load_threat_history(data);
    });

    register_interface("getTemplates"sv, [this](auto const&) {
        get_policy_templates();
    });

    register_interface("createFromTemplate"sv, [this](auto const& data) {
        create_policy_from_template(data);
    });

    register_interface("openQuarantineManager"sv, [this](auto const&) {
        open_quarantine_manager();
    });

    register_interface("getMetrics"sv, [this](auto const&) {
        get_metrics();
    });

    register_interface("getCredentialProtectionData"sv, [this](auto const&) {
        get_credential_protection_data();
    });

    register_interface("revokeTrustedForm"sv, [this](auto const& data) {
        revoke_trusted_form(data);
    });

    // Sentinel Phase 6 Day 41: Credential education preference
    register_interface("setCredentialEducationShown"sv, [this](auto const& data) {
        set_credential_education_shown(data);
    });

    // Milestone 0.3 Phase 4: Import/Export credential relationships
    register_interface("exportCredentialRelationships"sv, [this](auto const&) {
        export_credential_relationships();
    });

    register_interface("importCredentialRelationships"sv, [this](auto const& data) {
        import_credential_relationships(data);
    });

    // Milestone 0.3 Phase 5: Policy template management
    register_interface("getPolicyTemplates"sv, [this](auto const&) {
        get_policy_templates();
    });

    register_interface("applyPolicyTemplate"sv, [this](auto const& data) {
        apply_policy_template(data);
    });

    register_interface("exportPolicyTemplates"sv, [this](auto const&) {
        export_policy_templates();
    });

    register_interface("importPolicyTemplates"sv, [this](auto const& data) {
        import_policy_templates(data);
    });

    // Milestone 0.4 Phase 6: Network Monitoring
    register_interface("getNetworkMonitoringStats"sv, [this](auto const&) {
        get_network_monitoring_stats();
    });

    register_interface("getTrafficAlerts"sv, [this](auto const&) {
        get_traffic_alerts();
    });

    register_interface("getNetworkBehaviorPolicies"sv, [this](auto const&) {
        get_network_behavior_policies();
    });

    register_interface("deleteNetworkBehaviorPolicy"sv, [this](auto const& data) {
        delete_network_behavior_policy(data);
    });

    register_interface("clearTrafficAlerts"sv, [this](auto const&) {
        clear_traffic_alerts();
    });

    register_interface("exportNetworkBehaviorPolicies"sv, [this](auto const&) {
        export_network_behavior_policies();
    });

    register_interface("importNetworkBehaviorPolicies"sv, [this](auto const& data) {
        import_network_behavior_policies(data);
    });
}

void SecurityUI::get_system_status()
{
    // Query RequestServer for real-time SentinelServer status via IPC
    // This replaces the old heuristic-based approach with actual status from RequestServer
    auto& request_client = Application::request_server_client();

    // Make synchronous IPC call to get sentinel status
    // The RequestServer checks if g_security_tap (connection to SentinelServer) is initialized
    auto response = request_client.get_sentinel_status();
    handle_sentinel_status(response.connected(), response.scanning_enabled());
}

void SecurityUI::handle_sentinel_status(bool connected, bool scanning_enabled)
{
    JsonObject status;
    status.set("connected"sv, JsonValue { connected });
    status.set("scanning_enabled"sv, JsonValue { scanning_enabled });

    // Get last scan time from threat history
    i64 last_scan_timestamp = 0;
    if (m_policy_graph.has_value()) {
        // Get the most recent threat from history
        auto threats_result = m_policy_graph->value()->get_threat_history({}); // All threats (empty Optional)
        if (!threats_result.is_error()) {
            auto threats = threats_result.release_value();
            if (!threats.is_empty()) {
                // Find most recent threat
                for (auto const& threat : threats) {
                    auto timestamp = threat.detected_at.milliseconds_since_epoch();
                    if (timestamp > last_scan_timestamp) {
                        last_scan_timestamp = timestamp;
                    }
                }
            }
        }
    }

    status.set("last_scan"sv, JsonValue { last_scan_timestamp });

    async_send_message("systemStatusLoaded"sv, status);
}

void SecurityUI::load_statistics()
{
    JsonObject stats;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized, returning zeros");
        stats.set("totalPolicies"sv, JsonValue { 0 });
        stats.set("threatsBlocked"sv, JsonValue { 0 });
        stats.set("threatsQuarantined"sv, JsonValue { 0 });
        stats.set("threatsToday"sv, JsonValue { 0 });
        async_send_message("statisticsLoaded"sv, stats);
        return;
    }

    // Query PolicyGraph for statistics
    auto policy_count_result = m_policy_graph->value()->get_policy_count();
    auto threat_count_result = m_policy_graph->value()->get_threat_count();

    if (policy_count_result.is_error()) {
        dbgln("SecurityUI: Failed to get policy count: {}", policy_count_result.error());
        stats.set("totalPolicies"sv, JsonValue { 0 });
    } else {
        stats.set("totalPolicies"sv, JsonValue { static_cast<i64>(policy_count_result.value()) });
    }

    if (threat_count_result.is_error()) {
        dbgln("SecurityUI: Failed to get threat count: {}", threat_count_result.error());
        stats.set("threatsBlocked"sv, JsonValue { 0 });
        stats.set("threatsQuarantined"sv, JsonValue { 0 });
        stats.set("threatsToday"sv, JsonValue { 0 });
    } else {
        // Get all threats to analyze by action_taken
        auto threats_result = m_policy_graph->value()->get_threat_history({});

        if (threats_result.is_error()) {
            stats.set("threatsBlocked"sv, JsonValue { 0 });
            stats.set("threatsQuarantined"sv, JsonValue { 0 });
            stats.set("threatsToday"sv, JsonValue { 0 });
        } else {
            auto threats = threats_result.release_value();

            // Count threats by action_taken
            size_t blocked_count = 0;
            size_t quarantined_count = 0;
            size_t threats_today_count = 0;

            // Calculate yesterday's timestamp for "today" filtering
            auto now = UnixDateTime::now();
            auto yesterday = UnixDateTime::from_seconds_since_epoch(now.seconds_since_epoch() - 86400);

            for (auto const& threat : threats) {
                // Count by action
                if (threat.action_taken == "block"sv) {
                    blocked_count++;
                } else if (threat.action_taken == "quarantine"sv) {
                    quarantined_count++;
                }

                // Count threats from last 24 hours
                if (threat.detected_at >= yesterday) {
                    threats_today_count++;
                }
            }

            stats.set("threatsBlocked"sv, JsonValue { static_cast<i64>(blocked_count) });
            stats.set("threatsQuarantined"sv, JsonValue { static_cast<i64>(quarantined_count) });
            stats.set("threatsToday"sv, JsonValue { static_cast<i64>(threats_today_count) });
        }
    }

    async_send_message("statisticsLoaded"sv, stats);
}

void SecurityUI::load_policies()
{
    JsonArray policies_array;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized, returning empty policies");
        JsonObject response;
        response.set("policies"sv, JsonValue { policies_array });
        async_send_message("policiesLoaded"sv, response);
        return;
    }

    // Query PolicyGraph for all policies
    auto policies_result = m_policy_graph->value()->list_policies();

    if (policies_result.is_error()) {
        dbgln("SecurityUI: Failed to list policies: {}", policies_result.error());
        JsonObject response;
        response.set("policies"sv, JsonValue { policies_array });
        async_send_message("policiesLoaded"sv, response);
        return;
    }

    // Convert policies to JSON
    auto policies = policies_result.release_value();
    for (auto const& policy : policies) {
        JsonObject policy_obj;
        policy_obj.set("id"sv, JsonValue { policy.id });
        policy_obj.set("ruleName"sv, JsonValue { policy.rule_name });

        if (policy.url_pattern.has_value()) {
            policy_obj.set("urlPattern"sv, JsonValue { *policy.url_pattern });
        }

        if (policy.file_hash.has_value()) {
            policy_obj.set("fileHash"sv, JsonValue { *policy.file_hash });
        }

        if (policy.mime_type.has_value()) {
            policy_obj.set("mimeType"sv, JsonValue { *policy.mime_type });
        }

        // Convert PolicyAction enum to string
        StringView action_str;
        switch (policy.action) {
        case Sentinel::PolicyGraph::PolicyAction::Allow:
            action_str = "Allow"sv;
            break;
        case Sentinel::PolicyGraph::PolicyAction::Block:
            action_str = "Block"sv;
            break;
        case Sentinel::PolicyGraph::PolicyAction::Quarantine:
            action_str = "Quarantine"sv;
            break;
        case Sentinel::PolicyGraph::PolicyAction::BlockAutofill:
            action_str = "BlockAutofill"sv;
            break;
        case Sentinel::PolicyGraph::PolicyAction::WarnUser:
            action_str = "WarnUser"sv;
            break;
        }
        policy_obj.set("action"sv, JsonValue { action_str });

        policy_obj.set("createdAt"sv, JsonValue { policy.created_at.milliseconds_since_epoch() });
        policy_obj.set("createdBy"sv, JsonValue { policy.created_by });

        if (policy.expires_at.has_value()) {
            policy_obj.set("expiresAt"sv, JsonValue { policy.expires_at->milliseconds_since_epoch() });
        }

        policy_obj.set("hitCount"sv, JsonValue { static_cast<i64>(policy.hit_count) });

        if (policy.last_hit.has_value()) {
            policy_obj.set("lastHit"sv, JsonValue { policy.last_hit->milliseconds_since_epoch() });
        }

        policies_array.must_append(policy_obj);
    }

    JsonObject response;
    response.set("policies"sv, JsonValue { policies_array });

    async_send_message("policiesLoaded"sv, response);
}

void SecurityUI::get_policy(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid request: expected object with policyId"sv });
        async_send_message("policyLoaded"sv, error);
        return;
    }

    if (!m_policy_graph.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "PolicyGraph not initialized"sv });
        async_send_message("policyLoaded"sv, error);
        return;
    }

    auto const& data_obj = data.as_object();
    auto policy_id_value = data_obj.get_integer<i64>("policyId"sv);

    if (!policy_id_value.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing or invalid policyId"sv });
        async_send_message("policyLoaded"sv, error);
        return;
    }

    // Retrieve policy from PolicyGraph
    auto policy_result = m_policy_graph->value()->get_policy(policy_id_value.value());

    if (policy_result.is_error()) {
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Failed to get policy: {}", policy_result.error()) });
        async_send_message("policyLoaded"sv, error);
        return;
    }

    // Convert policy to JSON
    auto const& policy = policy_result.value();
    JsonObject policy_obj;
    policy_obj.set("id"sv, JsonValue { policy.id });
    policy_obj.set("ruleName"sv, JsonValue { policy.rule_name });

    if (policy.url_pattern.has_value()) {
        policy_obj.set("urlPattern"sv, JsonValue { *policy.url_pattern });
    }

    if (policy.file_hash.has_value()) {
        policy_obj.set("fileHash"sv, JsonValue { *policy.file_hash });
    }

    if (policy.mime_type.has_value()) {
        policy_obj.set("mimeType"sv, JsonValue { *policy.mime_type });
    }

    // Convert PolicyAction enum to string
    StringView action_str;
    switch (policy.action) {
    case Sentinel::PolicyGraph::PolicyAction::Allow:
        action_str = "Allow"sv;
        break;
    case Sentinel::PolicyGraph::PolicyAction::Block:
        action_str = "Block"sv;
        break;
    case Sentinel::PolicyGraph::PolicyAction::Quarantine:
        action_str = "Quarantine"sv;
        break;
    case Sentinel::PolicyGraph::PolicyAction::BlockAutofill:
        action_str = "BlockAutofill"sv;
        break;
    case Sentinel::PolicyGraph::PolicyAction::WarnUser:
        action_str = "WarnUser"sv;
        break;
    }
    policy_obj.set("action"sv, JsonValue { action_str });

    policy_obj.set("createdAt"sv, JsonValue { policy.created_at.milliseconds_since_epoch() });
    policy_obj.set("createdBy"sv, JsonValue { policy.created_by });

    if (policy.expires_at.has_value()) {
        policy_obj.set("expiresAt"sv, JsonValue { policy.expires_at->milliseconds_since_epoch() });
    }

    policy_obj.set("hitCount"sv, JsonValue { static_cast<i64>(policy.hit_count) });

    if (policy.last_hit.has_value()) {
        policy_obj.set("lastHit"sv, JsonValue { policy.last_hit->milliseconds_since_epoch() });
    }

    async_send_message("policyLoaded"sv, policy_obj);
}

void SecurityUI::create_policy(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid request: expected policy object"sv });
        async_send_message("policyCreated"sv, error);
        return;
    }

    if (!m_policy_graph.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "PolicyGraph not initialized"sv });
        async_send_message("policyCreated"sv, error);
        return;
    }

    auto const& data_obj = data.as_object();

    // Parse required fields
    auto rule_name = data_obj.get_string("ruleName"sv);
    auto action_str = data_obj.get_string("action"sv);

    if (!rule_name.has_value() || !action_str.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing required fields: ruleName and action"sv });
        async_send_message("policyCreated"sv, error);
        return;
    }

    // Parse action
    Sentinel::PolicyGraph::PolicyAction action;
    if (action_str.value() == "Allow"sv) {
        action = Sentinel::PolicyGraph::PolicyAction::Allow;
    } else if (action_str.value() == "Block"sv) {
        action = Sentinel::PolicyGraph::PolicyAction::Block;
    } else if (action_str.value() == "Quarantine"sv) {
        action = Sentinel::PolicyGraph::PolicyAction::Quarantine;
    } else {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid action: must be Allow, Block, or Quarantine"sv });
        async_send_message("policyCreated"sv, error);
        return;
    }

    // Parse optional fields
    Optional<String> url_pattern;
    if (auto url_val = data_obj.get_string("urlPattern"sv); url_val.has_value()) {
        url_pattern = url_val.value();
    }

    Optional<String> file_hash;
    if (auto hash_val = data_obj.get_string("fileHash"sv); hash_val.has_value()) {
        file_hash = hash_val.value();
    }

    Optional<String> mime_type;
    if (auto mime_val = data_obj.get_string("mimeType"sv); mime_val.has_value()) {
        mime_type = mime_val.value();
    }

    // Create policy struct
    Sentinel::PolicyGraph::Policy policy {
        .id = -1, // Will be assigned by database
        .rule_name = rule_name.value(),
        .url_pattern = move(url_pattern),
        .file_hash = move(file_hash),
        .mime_type = move(mime_type),
        .action = action,
        .match_type = Sentinel::PolicyGraph::PolicyMatchType::DownloadOriginFileType,
        .enforcement_action = ""_string,
        .created_at = UnixDateTime::now(),
        .created_by = "UI"_string,
        .expires_at = {},
        .hit_count = 0,
        .last_hit = {}
    };

    // Create policy in PolicyGraph
    auto policy_id_result = m_policy_graph->value()->create_policy(policy);

    if (policy_id_result.is_error()) {
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Failed to create policy: {}", policy_id_result.error()) });
        async_send_message("policyCreated"sv, error);
        return;
    }

    JsonObject response;
    response.set("success"sv, JsonValue { true });
    response.set("policyId"sv, JsonValue { policy_id_result.value() });
    response.set("message"sv, JsonValue { "Policy created successfully"sv });

    async_send_message("policyCreated"sv, response);
}

void SecurityUI::update_policy(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid request: expected policy object"sv });
        async_send_message("policyUpdated"sv, error);
        return;
    }

    if (!m_policy_graph.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "PolicyGraph not initialized"sv });
        async_send_message("policyUpdated"sv, error);
        return;
    }

    auto const& data_obj = data.as_object();

    // Parse policy ID
    auto policy_id_value = data_obj.get_integer<i64>("id"sv);
    if (!policy_id_value.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing or invalid policy id"sv });
        async_send_message("policyUpdated"sv, error);
        return;
    }

    // Parse required fields
    auto rule_name = data_obj.get_string("ruleName"sv);
    auto action_str = data_obj.get_string("action"sv);

    if (!rule_name.has_value() || !action_str.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing required fields: ruleName and action"sv });
        async_send_message("policyUpdated"sv, error);
        return;
    }

    // Parse action
    Sentinel::PolicyGraph::PolicyAction action;
    if (action_str.value() == "Allow"sv) {
        action = Sentinel::PolicyGraph::PolicyAction::Allow;
    } else if (action_str.value() == "Block"sv) {
        action = Sentinel::PolicyGraph::PolicyAction::Block;
    } else if (action_str.value() == "Quarantine"sv) {
        action = Sentinel::PolicyGraph::PolicyAction::Quarantine;
    } else {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid action: must be Allow, Block, or Quarantine"sv });
        async_send_message("policyUpdated"sv, error);
        return;
    }

    // Parse optional fields
    Optional<String> url_pattern;
    if (auto url_val = data_obj.get_string("urlPattern"sv); url_val.has_value()) {
        url_pattern = url_val.value();
    }

    Optional<String> file_hash;
    if (auto hash_val = data_obj.get_string("fileHash"sv); hash_val.has_value()) {
        file_hash = hash_val.value();
    }

    Optional<String> mime_type;
    if (auto mime_val = data_obj.get_string("mimeType"sv); mime_val.has_value()) {
        mime_type = mime_val.value();
    }

    // Create updated policy struct
    Sentinel::PolicyGraph::Policy policy {
        .id = policy_id_value.value(),
        .rule_name = rule_name.value(),
        .url_pattern = move(url_pattern),
        .file_hash = move(file_hash),
        .mime_type = move(mime_type),
        .action = action,
        .match_type = Sentinel::PolicyGraph::PolicyMatchType::DownloadOriginFileType,
        .enforcement_action = ""_string,
        .created_at = UnixDateTime::now(), // This will be ignored by update
        .created_by = "UI"_string,
        .expires_at = {},
        .hit_count = 0,
        .last_hit = {}
    };

    // Update policy in PolicyGraph
    auto update_result = m_policy_graph->value()->update_policy(policy_id_value.value(), policy);

    if (update_result.is_error()) {
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Failed to update policy: {}", update_result.error()) });
        async_send_message("policyUpdated"sv, error);
        return;
    }

    JsonObject response;
    response.set("success"sv, JsonValue { true });
    response.set("message"sv, JsonValue { "Policy updated successfully"sv });

    async_send_message("policyUpdated"sv, response);
}

void SecurityUI::delete_policy(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid request: expected object with policyId"sv });
        async_send_message("policyDeleted"sv, error);
        return;
    }

    if (!m_policy_graph.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "PolicyGraph not initialized"sv });
        async_send_message("policyDeleted"sv, error);
        return;
    }

    auto const& data_obj = data.as_object();
    auto policy_id_value = data_obj.get_integer<i64>("policyId"sv);

    if (!policy_id_value.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing or invalid policyId"sv });
        async_send_message("policyDeleted"sv, error);
        return;
    }

    // Delete policy from PolicyGraph
    auto delete_result = m_policy_graph->value()->delete_policy(policy_id_value.value());

    if (delete_result.is_error()) {
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Failed to delete policy: {}", delete_result.error()) });
        async_send_message("policyDeleted"sv, error);
        return;
    }

    JsonObject response;
    response.set("success"sv, JsonValue { true });
    response.set("message"sv, JsonValue { "Policy deleted successfully"sv });

    async_send_message("policyDeleted"sv, response);
}

void SecurityUI::load_threat_history(JsonValue const& data)
{
    JsonArray threats_array;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized, returning empty threat history");
        JsonObject response;
        response.set("threats"sv, JsonValue { threats_array });
        async_send_message("threatHistoryLoaded"sv, response);
        return;
    }

    // Parse optional 'since' parameter (timestamp in milliseconds)
    Optional<UnixDateTime> since;
    if (data.is_object()) {
        auto const& data_obj = data.as_object();
        auto since_value = data_obj.get_integer<i64>("since"sv);
        if (since_value.has_value()) {
            since = UnixDateTime::from_milliseconds_since_epoch(since_value.value());
        }
    }

    // Query PolicyGraph for threat history
    auto threats_result = m_policy_graph->value()->get_threat_history(since);

    if (threats_result.is_error()) {
        dbgln("SecurityUI: Failed to get threat history: {}", threats_result.error());
        JsonObject response;
        response.set("threats"sv, JsonValue { threats_array });
        async_send_message("threatHistoryLoaded"sv, response);
        return;
    }

    // Convert threats to JSON
    auto threats = threats_result.release_value();
    for (auto const& threat : threats) {
        JsonObject threat_obj;
        threat_obj.set("id"sv, JsonValue { threat.id });
        threat_obj.set("detectedAt"sv, JsonValue { threat.detected_at.milliseconds_since_epoch() });
        threat_obj.set("url"sv, JsonValue { threat.url });
        threat_obj.set("filename"sv, JsonValue { threat.filename });
        threat_obj.set("fileHash"sv, JsonValue { threat.file_hash });
        threat_obj.set("mimeType"sv, JsonValue { threat.mime_type });
        threat_obj.set("fileSize"sv, JsonValue { static_cast<i64>(threat.file_size) });
        threat_obj.set("ruleName"sv, JsonValue { threat.rule_name });
        threat_obj.set("severity"sv, JsonValue { threat.severity });
        threat_obj.set("actionTaken"sv, JsonValue { threat.action_taken });

        if (threat.policy_id.has_value()) {
            threat_obj.set("policyId"sv, JsonValue { threat.policy_id.value() });
        }

        threat_obj.set("alertJson"sv, JsonValue { threat.alert_json });

        threats_array.must_append(threat_obj);
    }

    JsonObject response;
    response.set("threats"sv, JsonValue { threats_array });

    async_send_message("threatHistoryLoaded"sv, response);
}

void SecurityUI::get_policy_templates()
{
    // Milestone 0.3 Phase 5: Load policy templates from PolicyGraph database

    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for get templates");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("policyTemplates"sv, response);
        return;
    }

    auto list_result = m_policy_graph->value()->list_templates({});
    if (list_result.is_error()) {
        dbgln("SecurityUI: Failed to list templates: {}", list_result.error());
        response.set("error"sv, JsonValue { "Failed to list templates"_string });
    } else {
        // Convert templates to JSON array
        JsonArray templates_array;
        for (auto const& tmpl : list_result.value()) {
            JsonObject tmpl_obj;
            tmpl_obj.set("id"sv, tmpl.id);
            tmpl_obj.set("name"sv, tmpl.name);
            tmpl_obj.set("description"sv, tmpl.description);
            tmpl_obj.set("category"sv, tmpl.category);
            tmpl_obj.set("is_builtin"sv, tmpl.is_builtin);
            templates_array.must_append(tmpl_obj);
        }

        dbgln("SecurityUI: Retrieved {} policy templates", templates_array.size());
        response.set("templates"sv, JsonValue(templates_array).serialized());
    }

    async_send_message("policyTemplates"sv, response);
}

void SecurityUI::create_policy_from_template(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid request: expected object with templateId and variables"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    if (!m_policy_graph.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "PolicyGraph not initialized"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    auto const& data_obj = data.as_object();
    auto template_id = data_obj.get_string("templateId"sv);

    if (!template_id.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing templateId"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    // Load the template
    auto template_filename = ByteString::formatted("{}.json", template_id.value());
    auto template_resource_result = Core::Resource::load_from_uri(
        ByteString::formatted("resource://ladybird/policy-templates/{}", template_filename)
    );

    if (template_resource_result.is_error()) {
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Failed to load template: {}", template_resource_result.error()) });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    auto template_resource = template_resource_result.release_value();
    auto json_data = ByteString(reinterpret_cast<char const*>(template_resource->data().data()), template_resource->data().size());
    auto json_result = JsonValue::from_string(json_data);

    if (json_result.is_error()) {
        JsonObject error;
        error.set("error"sv, JsonValue { ByteString::formatted("Failed to parse template: {}", json_result.error()) });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    auto template_json = json_result.release_value();
    if (!template_json.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Template is not a JSON object"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    auto const& template_obj = template_json.as_object();

    // Get variables from request
    auto variables_json = data_obj.get_object("variables"sv);
    if (!variables_json.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing variables"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    auto const& variables = variables_json.value();

    // Get policies array from template
    auto policies_json = template_obj.get_array("policies"sv);
    if (!policies_json.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Template missing policies array"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    // Create policies from template
    Vector<i64> created_policy_ids;
    for (auto const& policy_value : policies_json.value().values()) {
        if (!policy_value.is_object()) {
            continue;
        }

        auto const& policy_template = policy_value.as_object();

        // Get rule name and substitute variables
        auto rule_name_template = policy_template.get_string("ruleName"sv);
        if (!rule_name_template.has_value()) {
            continue;
        }

        String rule_name = rule_name_template.value();

        // Substitute variables in rule name
        variables.for_each_member([&](auto const& var_name, auto const& var_value) {
            if (var_value.is_string()) {
                auto placeholder = ByteString::formatted("${{{}}}", var_name);
                rule_name = MUST(rule_name.replace(placeholder, var_value.as_string(), ReplaceMode::All));
            }
        });

        // Get action
        auto action_str = policy_template.get_string("action"sv);
        if (!action_str.has_value()) {
            continue;
        }

        Sentinel::PolicyGraph::PolicyAction action;
        if (action_str.value() == "Allow"sv) {
            action = Sentinel::PolicyGraph::PolicyAction::Allow;
        } else if (action_str.value() == "Block"sv) {
            action = Sentinel::PolicyGraph::PolicyAction::Block;
        } else if (action_str.value() == "Quarantine"sv) {
            action = Sentinel::PolicyGraph::PolicyAction::Quarantine;
        } else {
            continue;
        }

        // Get match pattern
        auto match_pattern_json = policy_template.get_object("match_pattern"sv);
        if (!match_pattern_json.has_value()) {
            continue;
        }

        auto const& match_pattern = match_pattern_json.value();

        // Extract and substitute URL pattern
        Optional<String> url_pattern;
        if (auto url_val = match_pattern.get_string("url_pattern"sv); url_val.has_value() && !url_val.value().is_empty()) {
            String url = url_val.value();
            variables.for_each_member([&](auto const& var_name, auto const& var_value) {
                if (var_value.is_string()) {
                    auto placeholder = ByteString::formatted("${{{}}}", var_name);
                    url = MUST(url.replace(placeholder, var_value.as_string(), ReplaceMode::All));
                }
            });
            url_pattern = url;
        }

        // Extract and substitute file hash
        Optional<String> file_hash;
        if (auto hash_val = match_pattern.get_string("file_hash"sv); hash_val.has_value() && !hash_val.value().is_empty()) {
            String hash = hash_val.value();
            variables.for_each_member([&](auto const& var_name, auto const& var_value) {
                if (var_value.is_string()) {
                    auto placeholder = ByteString::formatted("${{{}}}", var_name);
                    hash = MUST(hash.replace(placeholder, var_value.as_string(), ReplaceMode::All));
                }
            });
            file_hash = hash;
        }

        // Extract MIME type (usually not templated, but handle it anyway)
        Optional<String> mime_type;
        if (auto mime_val = match_pattern.get_string("mime_type"sv); mime_val.has_value() && !mime_val.value().is_empty()) {
            mime_type = mime_val.value();
        }

        // Create the policy
        Sentinel::PolicyGraph::Policy policy {
            .id = -1,
            .rule_name = rule_name,
            .url_pattern = move(url_pattern),
            .file_hash = move(file_hash),
            .mime_type = move(mime_type),
            .action = action,
            .match_type = Sentinel::PolicyGraph::PolicyMatchType::DownloadOriginFileType,
            .enforcement_action = ""_string,
            .created_at = UnixDateTime::now(),
            .created_by = "Template"_string,
            .expires_at = {},
            .hit_count = 0,
            .last_hit = {}
        };

        auto policy_id_result = m_policy_graph->value()->create_policy(policy);
        if (!policy_id_result.is_error()) {
            created_policy_ids.append(policy_id_result.value());
        } else {
            dbgln("SecurityUI: Failed to create policy from template: {}", policy_id_result.error());
        }
    }

    // Send response
    if (created_policy_ids.is_empty()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Failed to create any policies from template"sv });
        async_send_message("policyFromTemplateCreated"sv, error);
        return;
    }

    JsonArray ids_array;
    for (auto id : created_policy_ids) {
        ids_array.must_append(JsonValue { id });
    }

    JsonObject response;
    response.set("success"sv, JsonValue { true });
    response.set("policyIds"sv, JsonValue { ids_array });
    response.set("message"sv, JsonValue { ByteString::formatted("Created {} policies from template", created_policy_ids.size()) });

    async_send_message("policyFromTemplateCreated"sv, response);
}

void SecurityUI::open_quarantine_manager()
{
    // This method is called when the user clicks "Manage Quarantine" in about:security
    // The actual quarantine dialog is shown by the Qt UI layer
    // We notify the application that the quarantine manager should be opened
    dbgln("SecurityUI: Quarantine manager requested");

    // Send a message to the application to open the quarantine dialog
    // The Qt/application layer will handle creating and showing the dialog
    Application::the().on_quarantine_manager_requested();
}

void SecurityUI::get_metrics()
{
    JsonObject metrics;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized, returning empty metrics");
        metrics.set("error"sv, JsonValue { "PolicyGraph not initialized"sv });
        async_send_message("metricsLoaded"sv, metrics);
        return;
    }

    // Get policy count
    auto policy_count_result = m_policy_graph->value()->get_policy_count();
    if (!policy_count_result.is_error()) {
        metrics.set("totalPolicies"sv, JsonValue { static_cast<i64>(policy_count_result.value()) });
    } else {
        metrics.set("totalPolicies"sv, JsonValue { 0 });
    }

    // Get threat count
    auto threat_count_result = m_policy_graph->value()->get_threat_count();
    if (!threat_count_result.is_error()) {
        metrics.set("totalThreats"sv, JsonValue { static_cast<i64>(threat_count_result.value()) });
    } else {
        metrics.set("totalThreats"sv, JsonValue { 0 });
    }

    // Get detailed threat breakdown
    auto threats_result = m_policy_graph->value()->get_threat_history({});
    if (!threats_result.is_error()) {
        auto threats = threats_result.release_value();

        size_t blocked_count = 0;
        size_t quarantined_count = 0;
        size_t allowed_count = 0;

        for (auto const& threat : threats) {
            if (threat.action_taken == "block"sv) {
                blocked_count++;
            } else if (threat.action_taken == "quarantine"sv) {
                quarantined_count++;
            } else if (threat.action_taken == "allow"sv) {
                allowed_count++;
            }
        }

        metrics.set("threatsBlocked"sv, JsonValue { static_cast<i64>(blocked_count) });
        metrics.set("threatsQuarantined"sv, JsonValue { static_cast<i64>(quarantined_count) });
        metrics.set("threatsAllowed"sv, JsonValue { static_cast<i64>(allowed_count) });

        // Get last threat timestamp
        i64 last_threat_timestamp = 0;
        for (auto const& threat : threats) {
            auto timestamp = threat.detected_at.milliseconds_since_epoch();
            if (timestamp > last_threat_timestamp) {
                last_threat_timestamp = timestamp;
            }
        }
        metrics.set("lastThreatTime"sv, JsonValue { last_threat_timestamp });
    } else {
        metrics.set("threatsBlocked"sv, JsonValue { 0 });
        metrics.set("threatsQuarantined"sv, JsonValue { 0 });
        metrics.set("threatsAllowed"sv, JsonValue { 0 });
        metrics.set("lastThreatTime"sv, JsonValue { 0 });
    }

    // Note: For full metrics including cache stats and performance data,
    // we would need to integrate with SentinelMetrics from the Sentinel daemon
    // This provides database-level metrics as a starting point
    metrics.set("metricsVersion"sv, JsonValue { 1 });

    async_send_message("metricsLoaded"sv, metrics);
}

void SecurityUI::get_credential_protection_data()
{
    JsonObject data;

    // NOTE: This is a stub implementation for Phase 6 Day 40
    // The actual credential protection data lives in WebContent's PageClient/FormMonitor
    // and is not directly accessible from the UI process yet.
    // Future implementation will need to query WebContent via IPC for real-time data.

    // For now, we return zero stats to demonstrate the UI integration
    data.set("formsMonitored"sv, JsonValue { 0 });
    data.set("threatsBlocked"sv, JsonValue { 0 });
    data.set("trustedForms"sv, JsonValue { 0 });

    // Empty alerts array
    JsonArray alerts_array;
    data.set("alerts"sv, JsonValue { alerts_array });

    // Empty trusted relationships array
    JsonArray trusted_array;
    data.set("trustedRelationships"sv, JsonValue { trusted_array });

    // TODO (Phase 6 Day 41+): Implement real data fetching:
    // 1. Add IPC message to WebContent to query FormMonitor state
    // 2. Aggregate data from all WebContent processes
    // 3. Store credential alerts in PolicyGraph for persistence
    // 4. Query PolicyGraph for trusted relationships and alert history

    async_send_message("credentialProtectionDataLoaded"sv, data);
}

void SecurityUI::revoke_trusted_form(JsonValue const& data)
{
    if (!data.is_object()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Invalid request: expected object with formOrigin and actionOrigin"sv });
        async_send_message("trustedFormRevoked"sv, error);
        return;
    }

    auto const& data_obj = data.as_object();
    auto form_origin = data_obj.get_string("formOrigin"sv);
    auto action_origin = data_obj.get_string("actionOrigin"sv);

    if (!form_origin.has_value() || !action_origin.has_value()) {
        JsonObject error;
        error.set("error"sv, JsonValue { "Missing formOrigin or actionOrigin"sv });
        async_send_message("trustedFormRevoked"sv, error);
        return;
    }

    dbgln("SecurityUI: Revoking trusted form relationship: {} -> {}", form_origin.value(), action_origin.value());

    // NOTE: This is a stub implementation for Phase 6 Day 40
    // The actual trusted relationships are stored in WebContent's FormMonitor
    // and are not persisted to PolicyGraph yet.

    // TODO (Phase 6 Day 41+): Implement real trust revocation:
    // 1. Delete the trusted relationship from PolicyGraph
    // 2. Send IPC message to all WebContent processes to update their FormMonitors
    // 3. Ensure future forms from this origin will trigger alerts

    // For now, just send success response
    JsonObject response;
    response.set("success"sv, JsonValue { true });
    response.set("message"sv, JsonValue { ByteString::formatted("Trust revoked for {} -> {}", form_origin.value(), action_origin.value()) });

    async_send_message("trustedFormRevoked"sv, response);
}

void SecurityUI::set_credential_education_shown(JsonValue const& data)
{
    // Sentinel Phase 6 Day 41: Save user preference for credential education modal

    if (!data.is_object()) {
        dbgln("SecurityUI: Invalid data for setCredentialEducationShown");
        return;
    }

    auto const& data_obj = data.as_object();
    auto dont_show_again = data_obj.get_bool("dontShowAgain"sv).value_or(false);

    dbgln("SecurityUI: Setting credential education shown preference: {}", dont_show_again);

    // Save preference to a file in user data directory
    auto preference_path = ByteString::formatted("{}/Ladybird/credential_education_shown", Core::StandardPaths::user_data_directory());

    // Create the directory if it doesn't exist
    auto dir_path = ByteString::formatted("{}/Ladybird", Core::StandardPaths::user_data_directory());
    auto dir_result = Core::System::mkdir(dir_path, 0700);
    if (dir_result.is_error() && dir_result.error().code() != EEXIST) {
        dbgln("SecurityUI: Failed to create Ladybird directory: {}", dir_result.error());
        return;
    }

    // Write the preference file (just create an empty file as a flag)
    if (dont_show_again) {
        auto file_result = Core::File::open(preference_path, Core::File::OpenMode::Write);
        if (file_result.is_error()) {
            dbgln("SecurityUI: Failed to create preference file: {}", file_result.error());
            return;
        }

        auto& file = file_result.value();
        auto write_result = file->write_until_depleted("1"sv.bytes());
        if (write_result.is_error()) {
            dbgln("SecurityUI: Failed to write preference file: {}", write_result.error());
        } else {
            dbgln("SecurityUI: Credential education preference saved successfully");
        }
    }
}

void SecurityUI::export_credential_relationships()
{
    // Milestone 0.3 Phase 4: Export credential relationships to JSON

    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for export");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("credentialExported"sv, response);
        return;
    }

    auto export_result = m_policy_graph->value()->export_relationships_json();
    if (export_result.is_error()) {
        dbgln("SecurityUI: Failed to export relationships: {}", export_result.error());
        response.set("error"sv, JsonValue { "Failed to export relationships"_string });
    } else {
        dbgln("SecurityUI: Successfully exported credential relationships");
        response.set("json"sv, JsonValue { export_result.value() });
    }

    async_send_message("credentialExported"sv, response);
}

void SecurityUI::import_credential_relationships(JsonValue const& data)
{
    // Milestone 0.3 Phase 4: Import credential relationships from JSON

    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for import");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("credentialImported"sv, response);
        return;
    }

    if (!data.is_object()) {
        dbgln("SecurityUI: Invalid data for import");
        response.set("error"sv, JsonValue { "Invalid request data"_string });
        async_send_message("credentialImported"sv, response);
        return;
    }

    auto const& data_obj = data.as_object();
    auto json_str = data_obj.get_string("json"sv);
    if (!json_str.has_value()) {
        dbgln("SecurityUI: Missing JSON data for import");
        response.set("error"sv, JsonValue { "Missing JSON data"_string });
        async_send_message("credentialImported"sv, response);
        return;
    }

    // Import relationships
    auto import_result = m_policy_graph->value()->import_relationships_json(json_str.value());
    if (import_result.is_error()) {
        dbgln("SecurityUI: Failed to import relationships: {}", import_result.error());
        response.set("error"sv, JsonValue { "Failed to import relationships"_string });
    } else {
        // Count how many relationships were imported by querying the database
        auto list_result = m_policy_graph->value()->list_relationships({});
        size_t count = 0;
        if (!list_result.is_error()) {
            count = list_result.value().size();
        }

        dbgln("SecurityUI: Successfully imported credential relationships (total: {})", count);
        response.set("count"sv, JsonValue { static_cast<i64>(count) });
    }

    async_send_message("credentialImported"sv, response);
}

// Milestone 0.3 Phase 5: Policy Template Management (Additional Methods)

void SecurityUI::apply_policy_template(JsonValue const& data)
{
    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for apply template");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("templateApplied"sv, response);
        return;
    }

    if (!data.is_object()) {
        dbgln("SecurityUI: Invalid data for apply template");
        response.set("error"sv, JsonValue { "Invalid request data"_string });
        async_send_message("templateApplied"sv, response);
        return;
    }

    auto const& data_obj = data.as_object();
    auto template_id_opt = data_obj.get_integer<i64>("template_id"sv);
    if (!template_id_opt.has_value()) {
        dbgln("SecurityUI: Missing template_id for apply");
        response.set("error"sv, JsonValue { "Missing template_id"_string });
        async_send_message("templateApplied"sv, response);
        return;
    }

    auto template_id = template_id_opt.value();

    // Instantiate the template (for now with no variables)
    HashMap<String, String> variables;
    auto policy_result = m_policy_graph->value()->instantiate_template(template_id, variables);
    if (policy_result.is_error()) {
        dbgln("SecurityUI: Failed to instantiate template {}: {}", template_id, policy_result.error());
        response.set("error"sv, JsonValue { "Failed to instantiate template"_string });
    } else {
        // Create the policy
        auto create_result = m_policy_graph->value()->create_policy(policy_result.value());
        if (create_result.is_error()) {
            dbgln("SecurityUI: Failed to create policy from template: {}", create_result.error());
            response.set("error"sv, JsonValue { "Failed to create policy"_string });
        } else {
            dbgln("SecurityUI: Successfully applied template {} as policy {}", template_id, create_result.value());
            response.set("policies_created"sv, JsonValue { 1 });
        }
    }

    async_send_message("templateApplied"sv, response);
}

void SecurityUI::export_policy_templates()
{
    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for export templates");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("templateExported"sv, response);
        return;
    }

    auto export_result = m_policy_graph->value()->export_templates_json();
    if (export_result.is_error()) {
        dbgln("SecurityUI: Failed to export templates: {}", export_result.error());
        response.set("error"sv, JsonValue { "Failed to export templates"_string });
    } else {
        dbgln("SecurityUI: Successfully exported policy templates");
        response.set("json"sv, JsonValue { export_result.value() });
    }

    async_send_message("templateExported"sv, response);
}

void SecurityUI::import_policy_templates(JsonValue const& data)
{
    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for import templates");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("templateImported"sv, response);
        return;
    }

    if (!data.is_object()) {
        dbgln("SecurityUI: Invalid data for import templates");
        response.set("error"sv, JsonValue { "Invalid request data"_string });
        async_send_message("templateImported"sv, response);
        return;
    }

    auto const& data_obj = data.as_object();
    auto json_str = data_obj.get_string("json"sv);
    if (!json_str.has_value()) {
        dbgln("SecurityUI: Missing JSON data for import templates");
        response.set("error"sv, JsonValue { "Missing JSON data"_string });
        async_send_message("templateImported"sv, response);
        return;
    }

    auto import_result = m_policy_graph->value()->import_templates_json(json_str.value());
    if (import_result.is_error()) {
        dbgln("SecurityUI: Failed to import templates: {}", import_result.error());
        response.set("error"sv, JsonValue { "Failed to import templates"_string });
    } else {
        // Count how many templates were imported
        auto list_result = m_policy_graph->value()->list_templates({});
        size_t count = 0;
        if (!list_result.is_error()) {
            count = list_result.value().size();
        }

        dbgln("SecurityUI: Successfully imported policy templates (total: {})", count);
        response.set("count"sv, JsonValue { static_cast<i64>(count) });
    }

    async_send_message("templateImported"sv, response);
}

// Milestone 0.4 Phase 6: Network Monitoring Implementation

void SecurityUI::get_network_monitoring_stats()
{
    JsonObject stats;

    // NOTE: This is a stub implementation for Phase 6
    // The actual network monitoring data lives in RequestServer's TrafficMonitor
    // and is not directly accessible from the UI process yet.
    // Future implementation will need to query RequestServer via IPC for real-time data.

    // For now, we return zero stats to demonstrate the UI integration
    stats.set("requestsMonitored"sv, JsonValue { 0 });
    stats.set("alertsGenerated"sv, JsonValue { 0 });
    stats.set("domainsAnalyzed"sv, JsonValue { 0 });

    // TODO (Phase 6+): Implement real data fetching:
    // 1. Add IPC message to RequestServer to query TrafficMonitor state
    // 2. Aggregate data from all RequestServer processes
    // 3. Return real statistics

    async_send_message("networkMonitoringStatsLoaded"sv, stats);
}

void SecurityUI::get_traffic_alerts()
{
    JsonObject response;
    JsonArray alerts_array;

    // NOTE: This is a stub implementation for Phase 6
    // Traffic alerts from TrafficMonitor are not yet persisted to PolicyGraph
    // Future implementation will store alerts in PolicyGraph for history

    // For now, return empty alerts array
    response.set("alerts"sv, JsonValue { alerts_array });

    // TODO (Phase 6+): Implement real alert history:
    // 1. Store TrafficMonitor alerts in PolicyGraph when generated
    // 2. Query PolicyGraph for alert history
    // 3. Return last 100 alerts sorted by timestamp descending

    async_send_message("trafficAlertsLoaded"sv, response);
}

void SecurityUI::get_network_behavior_policies()
{
    JsonObject response;
    JsonArray policies_array;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for network behavior policies");
        response.set("policies"sv, JsonValue { policies_array });
        async_send_message("networkBehaviorPoliciesLoaded"sv, response);
        return;
    }

    // Query PolicyGraph for network behavior policies
    auto policies_result = m_policy_graph->value()->get_all_network_behavior_policies();

    if (policies_result.is_error()) {
        dbgln("SecurityUI: Failed to get network behavior policies: {}", policies_result.error());
        response.set("policies"sv, JsonValue { policies_array });
        async_send_message("networkBehaviorPoliciesLoaded"sv, response);
        return;
    }

    // Convert policies to JSON
    auto policies = policies_result.release_value();
    for (auto const& policy : policies) {
        JsonObject policy_obj;
        policy_obj.set("id"sv, JsonValue { policy.id });
        policy_obj.set("domain"sv, JsonValue { policy.domain });
        policy_obj.set("policy"sv, JsonValue { policy.policy });
        policy_obj.set("threatType"sv, JsonValue { policy.threat_type });
        policy_obj.set("confidence"sv, JsonValue { policy.confidence });
        policy_obj.set("createdAt"sv, JsonValue { policy.created_at.milliseconds_since_epoch() });
        policy_obj.set("updatedAt"sv, JsonValue { policy.updated_at.milliseconds_since_epoch() });
        policy_obj.set("notes"sv, JsonValue { policy.notes });

        policies_array.must_append(policy_obj);
    }

    response.set("policies"sv, JsonValue { policies_array });
    async_send_message("networkBehaviorPoliciesLoaded"sv, response);
}

void SecurityUI::delete_network_behavior_policy(JsonValue const& data)
{
    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for delete network behavior policy");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("networkBehaviorPolicyDeleted"sv, response);
        return;
    }

    if (!data.is_object()) {
        dbgln("SecurityUI: Invalid data for delete network behavior policy");
        response.set("error"sv, JsonValue { "Invalid request data"_string });
        async_send_message("networkBehaviorPolicyDeleted"sv, response);
        return;
    }

    auto const& data_obj = data.as_object();
    auto policy_id_opt = data_obj.get_integer<i64>("policyId"sv);
    if (!policy_id_opt.has_value()) {
        dbgln("SecurityUI: Missing policyId for delete");
        response.set("error"sv, JsonValue { "Missing policyId"_string });
        async_send_message("networkBehaviorPolicyDeleted"sv, response);
        return;
    }

    auto policy_id = policy_id_opt.value();

    // Delete the policy
    auto delete_result = m_policy_graph->value()->delete_network_behavior_policy(policy_id);
    if (delete_result.is_error()) {
        dbgln("SecurityUI: Failed to delete network behavior policy {}: {}", policy_id, delete_result.error());
        response.set("error"sv, JsonValue { "Failed to delete policy"_string });
    } else {
        dbgln("SecurityUI: Successfully deleted network behavior policy {}", policy_id);
        response.set("success"sv, JsonValue { true });
    }

    async_send_message("networkBehaviorPolicyDeleted"sv, response);
}

void SecurityUI::clear_traffic_alerts()
{
    JsonObject response;

    // NOTE: This is a stub implementation for Phase 6
    // Traffic alerts are not yet persisted, so there's nothing to clear

    // For now, just return success
    response.set("success"sv, JsonValue { true });

    // TODO (Phase 6+): Implement real alert clearing:
    // 1. Add a method to PolicyGraph to delete all traffic alerts
    // 2. Call that method here

    async_send_message("trafficAlertsCleared"sv, response);
}

void SecurityUI::export_network_behavior_policies()
{
    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for export network behavior policies");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("networkPoliciesExported"sv, response);
        return;
    }

    // Get all network behavior policies
    auto policies_result = m_policy_graph->value()->get_all_network_behavior_policies();
    if (policies_result.is_error()) {
        dbgln("SecurityUI: Failed to get network behavior policies for export: {}", policies_result.error());
        response.set("error"sv, JsonValue { "Failed to export policies"_string });
        async_send_message("networkPoliciesExported"sv, response);
        return;
    }

    auto policies = policies_result.release_value();

    // Build JSON export structure
    JsonObject export_obj;
    export_obj.set("version"sv, JsonValue { 1 });
    export_obj.set("exported_at"sv, JsonValue { UnixDateTime::now().milliseconds_since_epoch() });

    JsonArray policies_array;
    for (auto const& policy : policies) {
        JsonObject policy_obj;
        policy_obj.set("domain"sv, JsonValue { policy.domain });
        policy_obj.set("policy"sv, JsonValue { policy.policy });
        policy_obj.set("threat_type"sv, JsonValue { policy.threat_type });
        policy_obj.set("confidence"sv, JsonValue { policy.confidence });
        policy_obj.set("notes"sv, JsonValue { policy.notes });

        policies_array.must_append(policy_obj);
    }

    export_obj.set("policies"sv, JsonValue { policies_array });

    dbgln("SecurityUI: Successfully exported {} network behavior policies", policies.size());
    response.set("json"sv, JsonValue { export_obj.serialized() });

    async_send_message("networkPoliciesExported"sv, response);
}

void SecurityUI::import_network_behavior_policies(JsonValue const& data)
{
    JsonObject response;

    if (!m_policy_graph.has_value()) {
        dbgln("SecurityUI: PolicyGraph not initialized for import network behavior policies");
        response.set("error"sv, JsonValue { "PolicyGraph not initialized"_string });
        async_send_message("networkPoliciesImported"sv, response);
        return;
    }

    if (!data.is_object()) {
        dbgln("SecurityUI: Invalid data for import network behavior policies");
        response.set("error"sv, JsonValue { "Invalid request data"_string });
        async_send_message("networkPoliciesImported"sv, response);
        return;
    }

    auto const& data_obj = data.as_object();
    auto json_str = data_obj.get_string("json"sv);
    if (!json_str.has_value()) {
        dbgln("SecurityUI: Missing JSON data for import");
        response.set("error"sv, JsonValue { "Missing JSON data"_string });
        async_send_message("networkPoliciesImported"sv, response);
        return;
    }

    // Parse the JSON
    auto json_result = JsonValue::from_string(json_str.value());
    if (json_result.is_error()) {
        dbgln("SecurityUI: Failed to parse import JSON: {}", json_result.error());
        response.set("error"sv, JsonValue { "Failed to parse JSON"_string });
        async_send_message("networkPoliciesImported"sv, response);
        return;
    }

    auto import_json = json_result.release_value();
    if (!import_json.is_object()) {
        dbgln("SecurityUI: Import JSON is not an object");
        response.set("error"sv, JsonValue { "Invalid JSON structure"_string });
        async_send_message("networkPoliciesImported"sv, response);
        return;
    }

    auto const& import_obj = import_json.as_object();
    auto policies_json = import_obj.get_array("policies"sv);
    if (!policies_json.has_value()) {
        dbgln("SecurityUI: Missing policies array in import JSON");
        response.set("error"sv, JsonValue { "Missing policies array"_string });
        async_send_message("networkPoliciesImported"sv, response);
        return;
    }

    // Import each policy
    size_t imported_count = 0;
    for (auto const& policy_value : policies_json.value().values()) {
        if (!policy_value.is_object()) {
            continue;
        }

        auto const& policy_obj = policy_value.as_object();

        auto domain = policy_obj.get_string("domain"sv);
        auto policy = policy_obj.get_string("policy"sv);
        auto threat_type = policy_obj.get_string("threat_type"sv);
        auto confidence = policy_obj.get_integer<i32>("confidence"sv);
        auto notes = policy_obj.get_string("notes"sv);

        if (!domain.has_value() || !policy.has_value() || !threat_type.has_value() || !confidence.has_value()) {
            dbgln("SecurityUI: Skipping policy with missing fields");
            continue;
        }

        // Create the policy
        auto create_result = m_policy_graph->value()->create_network_behavior_policy(
            domain.value(),
            policy.value(),
            threat_type.value(),
            confidence.value(),
            notes.value_or(""_string)
        );

        if (!create_result.is_error()) {
            imported_count++;
        } else {
            dbgln("SecurityUI: Failed to import network behavior policy for {}: {}", domain.value(), create_result.error());
        }
    }

    dbgln("SecurityUI: Successfully imported {} network behavior policies", imported_count);
    response.set("count"sv, JsonValue { static_cast<i64>(imported_count) });

    async_send_message("networkPoliciesImported"sv, response);
}

}
