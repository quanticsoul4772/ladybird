/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "OTXFeedClient.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/File.h>
#include <LibCore/System.h>

namespace Sentinel::ThreatIntelligence {

ErrorOr<NonnullOwnPtr<OTXFeedClient>> OTXFeedClient::create(String const& api_key, String const& db_directory)
{
    if (api_key.is_empty())
        return Error::from_string_literal("OTX API key cannot be empty");

    auto policy_graph = TRY(PolicyGraph::create(db_directory.to_byte_string()));

    return adopt_nonnull_own_or_enomem(new (nothrow) OTXFeedClient(api_key, move(policy_graph)));
}

OTXFeedClient::OTXFeedClient(String api_key, NonnullOwnPtr<PolicyGraph> policy_graph)
    : m_api_key(move(api_key))
    , m_policy_graph(move(policy_graph))
{
    m_stats.last_update = UnixDateTime::now();
}

ErrorOr<void> OTXFeedClient::fetch_latest_pulses()
{
    // Start with subscribed pulses endpoint
    auto url = TRY(String::formatted("{}/pulses/subscribed", m_base_url));

    dbgln("OTXFeedClient: Fetching pulses from {}", url);

    TRY(fetch_pulse_page(url));

    m_stats.last_update = UnixDateTime::now();
    dbgln("OTXFeedClient: Fetch complete - {} pulses, {} IOCs stored",
        m_stats.pulses_fetched, m_stats.iocs_stored);

    return {};
}

ErrorOr<void> OTXFeedClient::fetch_pulse_page(String const& url)
{
    // Fetch JSON from OTX API
    auto json_str = TRY(http_get(url));

    // Parse JSON response
    auto iocs = TRY(parse_pulse_json(json_str));

    // Store IOCs in PolicyGraph
    for (auto const& ioc : iocs) {
        auto store_result = m_policy_graph->store_ioc(ioc);
        if (store_result.is_error()) {
            dbgln("OTXFeedClient: Warning - failed to store IOC {}: {}",
                ioc.indicator, store_result.error());
            continue;
        }
        m_stats.iocs_stored++;
    }

    m_stats.iocs_processed += iocs.size();

    return {};
}

ErrorOr<Vector<PolicyGraph::IOC>> OTXFeedClient::parse_pulse_json(String const& json_str)
{
    Vector<PolicyGraph::IOC> iocs;

    auto json_result = JsonParser::parse(json_str);
    if (json_result.is_error())
        return Error::from_string_literal("Failed to parse OTX JSON response");

    auto json = json_result.release_value();
    if (!json.is_object())
        return Error::from_string_literal("OTX response is not a JSON object");

    auto const& root = json.as_object();

    // Extract "results" array containing pulses
    auto results_value = root.get("results"sv);
    if (!results_value.has_value() || !results_value->is_array())
        return Error::from_string_literal("OTX response missing 'results' array");

    auto const& results = results_value->as_array();

    for (auto const& pulse_value : results.values()) {
        if (!pulse_value.is_object())
            continue;

        auto const& pulse = pulse_value.as_object();
        m_stats.pulses_fetched++;

        // Extract IOCs from this pulse
        auto pulse_iocs_result = extract_iocs_from_pulse(pulse);
        if (pulse_iocs_result.is_error()) {
            dbgln("OTXFeedClient: Warning - failed to extract IOCs from pulse: {}",
                pulse_iocs_result.error());
            continue;
        }

        auto pulse_iocs = pulse_iocs_result.release_value();
        for (auto& ioc : pulse_iocs) {
            iocs.append(move(ioc));
        }
    }

    return iocs;
}

ErrorOr<Vector<PolicyGraph::IOC>> OTXFeedClient::extract_iocs_from_pulse(JsonObject const& pulse)
{
    Vector<PolicyGraph::IOC> iocs;

    // Extract pulse metadata
    auto name_value = pulse.get("name"sv);
    auto description_value = pulse.get("description"sv);

    String pulse_description;
    if (description_value.has_value() && description_value->is_string())
        pulse_description = description_value->as_string();
    else if (name_value.has_value() && name_value->is_string())
        pulse_description = name_value->as_string();

    // Extract tags
    Vector<String> tags;
    auto tags_value = pulse.get("tags"sv);
    if (tags_value.has_value() && tags_value->is_array()) {
        auto const& tags_array = tags_value->as_array();
        for (auto const& tag_value : tags_array.values()) {
            if (tag_value.is_string())
                tags.append(tag_value.as_string());
        }
    }

    // Extract indicators array
    auto indicators_value = pulse.get("indicators"sv);
    if (!indicators_value.has_value() || !indicators_value->is_array())
        return iocs; // No indicators in this pulse

    auto const& indicators = indicators_value->as_array();

    for (auto const& indicator_value : indicators.values()) {
        if (!indicator_value.is_object())
            continue;

        auto const& indicator_obj = indicator_value.as_object();

        // Extract indicator fields
        auto type_value = indicator_obj.get("type"sv);
        auto indicator_str_value = indicator_obj.get("indicator"sv);
        auto desc_value = indicator_obj.get("description"sv);

        if (!type_value.has_value() || !type_value->is_string())
            continue;
        if (!indicator_str_value.has_value() || !indicator_str_value->is_string())
            continue;

        auto type_str = type_value->as_string();
        auto indicator_str = indicator_str_value->as_string();

        // Map OTX indicator type to PolicyGraph IOC type
        PolicyGraph::IOC::Type ioc_type;
        if (type_str == "FileHash-SHA256"sv || type_str == "FileHash-SHA1"sv || type_str == "FileHash-MD5"sv) {
            ioc_type = PolicyGraph::IOC::Type::FileHash;
        } else if (type_str == "domain"sv || type_str == "hostname"sv) {
            ioc_type = PolicyGraph::IOC::Type::Domain;
        } else if (type_str == "IPv4"sv || type_str == "IPv6"sv) {
            ioc_type = PolicyGraph::IOC::Type::IP;
        } else if (type_str == "URL"sv || type_str == "URI"sv) {
            ioc_type = PolicyGraph::IOC::Type::URL;
        } else {
            // Skip unsupported indicator types
            continue;
        }

        // Create IOC
        PolicyGraph::IOC ioc;
        ioc.type = ioc_type;
        ioc.indicator = indicator_str;

        // Use indicator description if available, otherwise pulse description
        if (desc_value.has_value() && desc_value->is_string() && !desc_value->as_string().is_empty())
            ioc.description = desc_value->as_string();
        else if (!pulse_description.is_empty())
            ioc.description = pulse_description;

        ioc.tags = tags;
        ioc.created_at = UnixDateTime::now();
        ioc.source = "otx"_string;

        iocs.append(move(ioc));
    }

    return iocs;
}

ErrorOr<Vector<PolicyGraph::IOC>> OTXFeedClient::get_new_iocs_since(UnixDateTime since)
{
    Vector<PolicyGraph::IOC> new_iocs;

    // Search all IOCs from OTX source
    auto all_iocs = TRY(m_policy_graph->search_iocs({}, "otx"_string));

    // Filter by timestamp
    for (auto const& ioc : all_iocs) {
        if (ioc.created_at >= since)
            new_iocs.append(ioc);
    }

    return new_iocs;
}

ErrorOr<void> OTXFeedClient::generate_yara_rules(String const& output_directory)
{
    dbgln("OTXFeedClient: Generating YARA rules in {}", output_directory);

    // Get all file hash IOCs from OTX
    auto iocs = TRY(m_policy_graph->search_iocs(PolicyGraph::IOC::Type::FileHash, "otx"_string));

    dbgln("OTXFeedClient: Found {} file hash IOCs", iocs.size());

    // Create output directory if it doesn't exist
    auto dir_result = Core::System::mkdir(output_directory.to_byte_string(), 0755);
    if (dir_result.is_error() && dir_result.error().code() != EEXIST) {
        dbgln("OTXFeedClient: Warning - failed to create directory: {}", dir_result.error());
    }

    // Generate one YARA rule file for all OTX hashes
    StringBuilder yara_content;
    yara_content.append("// Auto-generated YARA rules from AlienVault OTX feed\n"sv);
    yara_content.append("// Generated at: "sv);
    yara_content.append(TRY(String::formatted("{}", UnixDateTime::now().seconds_since_epoch())));
    yara_content.append("\n\n"sv);

    u64 rules_generated = 0;

    for (auto const& ioc : iocs) {
        // Generate rule name from hash (truncate to 32 chars for readability)
        auto hash_prefix = ioc.indicator.bytes_as_string_view().substring_view(0, min(32ul, ioc.indicator.bytes_as_string_view().length()));

        yara_content.append("rule otx_hash_"sv);
        yara_content.append(hash_prefix);
        yara_content.append(" {\n"sv);

        // Meta section
        yara_content.append("    meta:\n"sv);
        yara_content.append("        description = \""sv);
        if (ioc.description.has_value())
            yara_content.append(ioc.description.value());
        else
            yara_content.append("OTX file hash indicator"sv);
        yara_content.append("\"\n"sv);

        yara_content.append("        source = \"AlienVault OTX\"\n"sv);
        yara_content.append("        date = \""sv);
        yara_content.append(TRY(String::formatted("{}", ioc.created_at.seconds_since_epoch())));
        yara_content.append("\"\n"sv);

        // Add tags if present
        if (!ioc.tags.is_empty()) {
            yara_content.append("        tags = \""sv);
            for (size_t i = 0; i < ioc.tags.size(); ++i) {
                if (i > 0)
                    yara_content.append(", "sv);
                yara_content.append(ioc.tags[i]);
            }
            yara_content.append("\"\n"sv);
        }

        // Condition section - check file hash
        yara_content.append("\n    condition:\n"sv);

        // Determine hash type from length
        auto hash_len = ioc.indicator.bytes_as_string_view().length();
        if (hash_len == 64) {
            // SHA256
            yara_content.append("        hash.sha256(0, filesize) == \""sv);
        } else if (hash_len == 40) {
            // SHA1
            yara_content.append("        hash.sha1(0, filesize) == \""sv);
        } else if (hash_len == 32) {
            // MD5
            yara_content.append("        hash.md5(0, filesize) == \""sv);
        } else {
            // Unknown hash type, skip
            continue;
        }

        yara_content.append(ioc.indicator);
        yara_content.append("\"\n"sv);
        yara_content.append("}\n\n"sv);

        rules_generated++;
    }

    // Write to file
    auto output_path = TRY(String::formatted("{}/otx_hashes.yara", output_directory));
    auto file = TRY(Core::File::open(output_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(TRY(yara_content.to_string()).bytes()));

    m_stats.yara_rules_generated = rules_generated;
    dbgln("OTXFeedClient: Generated {} YARA rules in {}", rules_generated, output_path);

    return {};
}

void OTXFeedClient::reset_statistics()
{
    m_stats.pulses_fetched = 0;
    m_stats.iocs_processed = 0;
    m_stats.iocs_stored = 0;
    m_stats.yara_rules_generated = 0;
    m_stats.last_error = ""_string;
}

ErrorOr<String> OTXFeedClient::http_get(String const& url)
{
    // Mock HTTP GET for testing
    // In production, this would use LibHTTP or curl

    dbgln("OTXFeedClient: HTTP GET {}", url);

    // Mock response for testing
    if (url.contains("mock"sv) || m_base_url.contains("mock"sv)) {
        // Return mock OTX pulse JSON
        return R"({
            "results": [
                {
                    "name": "Test Malware Campaign",
                    "description": "Test malware indicators",
                    "tags": ["malware", "test"],
                    "indicators": [
                        {
                            "type": "FileHash-SHA256",
                            "indicator": "ed01ebfbc9eb5bbea545af4d01bf5f1071661840480439c6e5babe8e080e41aa",
                            "description": "Test malware hash"
                        },
                        {
                            "type": "domain",
                            "indicator": "malicious-test-domain.com",
                            "description": "C2 server domain"
                        }
                    ]
                }
            ]
        })"_string;
    }

    // For real OTX API, we would use LibHTTP here
    // For now, return error indicating HTTP client not implemented
    return Error::from_string_literal("HTTP client not implemented - use mock mode for testing");
}

}
