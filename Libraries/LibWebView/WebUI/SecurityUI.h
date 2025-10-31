/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <LibWebView/Forward.h>
#include <LibWebView/WebUI.h>
#include <Services/Sentinel/PolicyGraph.h>

namespace WebView {

class WEBVIEW_API SecurityUI : public WebUI {
    WEB_UI(SecurityUI);

private:
    virtual void register_interfaces() override;

    // System status
    void get_system_status();
    void handle_sentinel_status(bool connected, bool scanning_enabled);

    // Statistics
    void load_statistics();

    // Policies
    void load_policies();
    void get_policy(JsonValue const&);
    void create_policy(JsonValue const&);
    void update_policy(JsonValue const&);
    void delete_policy(JsonValue const&);

    // Threat history
    void load_threat_history(JsonValue const&);

    // Policy templates
    void get_policy_templates();
    void create_policy_from_template(JsonValue const&);

    // Quarantine manager (Phase 4 Day 24)
    void open_quarantine_manager();

    // Metrics (Phase 5 Day 34)
    void get_metrics();

    // Credential Protection (Phase 6 Day 40)
    void get_credential_protection_data();
    void revoke_trusted_form(JsonValue const&);

    // Credential Education Preference (Phase 6 Day 41)
    void set_credential_education_shown(JsonValue const&);

    // Import/Export (Milestone 0.3 Phase 4)
    void export_credential_relationships();
    void import_credential_relationships(JsonValue const&);

    // Policy Templates (Milestone 0.3 Phase 5) - additional methods
    void apply_policy_template(JsonValue const&);
    void export_policy_templates();
    void import_policy_templates(JsonValue const&);

    // PolicyGraph instance for security policy management
    Optional<ErrorOr<NonnullOwnPtr<Sentinel::PolicyGraph>>> m_policy_graph;
};

}
