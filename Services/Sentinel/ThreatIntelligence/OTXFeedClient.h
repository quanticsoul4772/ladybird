/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <Services/Sentinel/PolicyGraph.h>

namespace Sentinel::ThreatIntelligence {

// AlienVault Open Threat Exchange (OTX) feed client
// Fetches IOCs (Indicators of Compromise) from OTX pulses
// Stores IOCs in PolicyGraph database for malware detection
class OTXFeedClient {
public:
    struct Statistics {
        u64 pulses_fetched { 0 };
        u64 iocs_processed { 0 };
        u64 iocs_stored { 0 };
        u64 yara_rules_generated { 0 };
        UnixDateTime last_update;
        String last_error;
    };

    static ErrorOr<NonnullOwnPtr<OTXFeedClient>> create(String const& api_key, String const& db_directory);

    // Fetch latest pulses from OTX and store IOCs
    ErrorOr<void> fetch_latest_pulses();

    // Get new IOCs since a specific timestamp
    ErrorOr<Vector<PolicyGraph::IOC>> get_new_iocs_since(UnixDateTime since);

    // Generate YARA rules from stored IOCs
    ErrorOr<void> generate_yara_rules(String const& output_directory);

    // Get statistics about feed operations
    Statistics const& get_statistics() const { return m_stats; }

    // Reset statistics counters
    void reset_statistics();

    // Set custom base URL for testing
    void set_base_url(String const& url) { m_base_url = url; }

private:
    OTXFeedClient(String api_key, NonnullOwnPtr<PolicyGraph> policy_graph);

    // Fetch single page of pulses (OTX returns 20 pulses per page)
    ErrorOr<void> fetch_pulse_page(String const& url);

    // Parse OTX pulse JSON and extract IOCs
    ErrorOr<Vector<PolicyGraph::IOC>> parse_pulse_json(String const& json);

    // Extract IOCs from a single pulse
    ErrorOr<Vector<PolicyGraph::IOC>> extract_iocs_from_pulse(JsonObject const& pulse);

    // HTTP GET request helper
    ErrorOr<String> http_get(String const& url);

    String m_api_key;
    String m_base_url { "https://otx.alienvault.com/api/v1"_string };
    NonnullOwnPtr<PolicyGraph> m_policy_graph;
    Statistics m_stats;
};

}
