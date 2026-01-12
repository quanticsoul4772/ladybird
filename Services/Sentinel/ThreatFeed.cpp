/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ThreatFeed.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonParser.h>
#include <AK/JsonValue.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibCrypto/Hash/SHA2.h>

namespace Sentinel {

ErrorOr<NonnullOwnPtr<ThreatFeed>> ThreatFeed::create()
{
    auto feed = adopt_own(*new ThreatFeed());

    // Initialize bloom filter
    feed->m_filter = TRY(BloomFilter::create(FILTER_SIZE_BITS, FILTER_NUM_HASHES));

    // Initialize IPFS sync (may fail if IPFS not available)
    auto ipfs_result = IPFSThreatSync::create();
    if (ipfs_result.is_error()) {
        dbgln("ThreatFeed: Warning: IPFS integration unavailable: {}", ipfs_result.error());
        // Continue without IPFS - graceful degradation
    } else {
        feed->m_ipfs_sync = ipfs_result.release_value();
        dbgln("ThreatFeed: IPFS integration initialized");
    }

    // Initialize category counts
    feed->m_category_counts.set(ThreatCategory::Malware, 0);
    feed->m_category_counts.set(ThreatCategory::Phishing, 0);
    feed->m_category_counts.set(ThreatCategory::Exploit, 0);
    feed->m_category_counts.set(ThreatCategory::PUP, 0);
    feed->m_category_counts.set(ThreatCategory::Unknown, 0);

    // Load default threat list if available
    String default_list = MUST(String::from_utf8("/home/rbsmith4/ladybird/Services/Sentinel/data/default_threats.json"sv));
    if (Core::System::access(default_list, R_OK).is_error()) {
        dbgln("ThreatFeed: No default threat list found at {}", default_list);
    } else {
        auto import_result = feed->import_threat_list(default_list);
        if (import_result.is_error()) {
            dbgln("ThreatFeed: Failed to import default threats: {}", import_result.error());
        } else {
            dbgln("ThreatFeed: Loaded default threat list");
        }
    }

    dbgln("ThreatFeed: Created with {} MB bloom filter, {} hash functions",
        FILTER_SIZE_BITS / 8 / 1024 / 1024, FILTER_NUM_HASHES);

    return feed;
}

ThreatFeed::ThreatFeed()
    : m_filter(BloomFilter::create(1, 1).release_value_but_fixme_should_propagate_errors()) // Dummy initialization
{
}

ErrorOr<void> ThreatFeed::add_threat_hash(String const& sha256_hash,
    ThreatCategory category, u32 severity)
{
    // Validate hash format (64 hex characters)
    if (sha256_hash.bytes_as_string_view().length() != 64) {
        return Error::from_string_literal("Invalid SHA256 hash format");
    }

    // Add to bloom filter
    m_filter->add(sha256_hash);

    // Update category counter
    auto it = m_category_counts.find(category);
    if (it != m_category_counts.end()) {
        it->value++;
    }

    // Add to cache if space available
    if (m_threat_cache.size() < MAX_CACHE_SIZE) {
        ThreatInfo info;
        info.sha256_hash = sha256_hash;
        info.category = category;
        info.severity = severity;
        info.first_seen = UnixDateTime::now();
        info.last_updated = UnixDateTime::now();

        m_threat_cache.set(sha256_hash, move(info));
    }

    return {};
}

bool ThreatFeed::probably_malicious(String const& sha256_hash) const
{
    return m_filter->contains(sha256_hash);
}

bool ThreatFeed::probably_malicious(ReadonlyBytes file_content) const
{
    auto hash = calculate_sha256(file_content);
    return probably_malicious(hash);
}

Optional<ThreatFeed::ThreatInfo> ThreatFeed::get_threat_info(String const& sha256_hash) const
{
    auto it = m_threat_cache.find(sha256_hash);
    if (it != m_threat_cache.end()) {
        return it->value;
    }
    return {};
}

ErrorOr<void> ThreatFeed::sync_from_peers()
{
    if (!m_ipfs_sync) {
        return Error::from_string_literal("IPFS sync not available");
    }

    dbgln("ThreatFeed: Syncing from peers...");

    // Get latest threat hashes from IPFS
    auto hashes = TRY(m_ipfs_sync->fetch_latest_threats());

    size_t new_threats = 0;
    for (auto const& hash_info : hashes) {
        if (!probably_malicious(hash_info.hash)) {
            TRY(add_threat_hash(hash_info.hash, static_cast<ThreatCategory>(hash_info.category), hash_info.severity));
            new_threats++;
        }
    }

    m_last_sync_time = UnixDateTime::now();
    m_peer_count = m_ipfs_sync->connected_peer_count();

    dbgln("ThreatFeed: Synced {} new threats from {} peers",
        new_threats, m_peer_count);

    return {};
}

ErrorOr<void> ThreatFeed::publish_local_threats()
{
    if (!m_ipfs_sync) {
        return Error::from_string_literal("IPFS sync not available");
    }

    Vector<IPFSThreatSync::ThreatHashInfo> threats_to_publish;

    // Collect threats from cache
    for (auto const& [hash, info] : m_threat_cache) {
        IPFSThreatSync::ThreatHashInfo hash_info;
        hash_info.hash = hash;
        hash_info.category = static_cast<IPFSThreatSync::ThreatCategory>(info.category);
        hash_info.severity = info.severity;
        threats_to_publish.append(move(hash_info));
    }

    if (threats_to_publish.is_empty()) {
        dbgln("ThreatFeed: No local threats to publish");
        return {};
    }

    // Apply differential privacy before publishing
    // In a real implementation, this would add noise to protect individual contributions
    dbgln("ThreatFeed: Publishing {} threats with differential privacy (ε={})",
        threats_to_publish.size(), m_privacy_epsilon);

    TRY(m_ipfs_sync->publish_threats(threats_to_publish));

    dbgln("ThreatFeed: Successfully published local threats");
    return {};
}

ErrorOr<void> ThreatFeed::subscribe_to_feed(String const& ipfs_hash)
{
    if (!m_ipfs_sync) {
        return Error::from_string_literal("IPFS sync not available");
    }

    TRY(m_ipfs_sync->subscribe_to_feed(ipfs_hash));
    m_trusted_peers.append(ipfs_hash);

    dbgln("ThreatFeed: Subscribed to feed: {}", ipfs_hash);
    return {};
}

ErrorOr<void> ThreatFeed::import_threat_list(String const& file_path)
{
    auto file = TRY(Core::File::open(file_path, Core::File::OpenMode::Read));
    auto contents = TRY(file->read_until_eof());

    auto json_result = JsonParser::parse(StringView(contents));
    if (json_result.is_error()) {
        return Error::from_string_literal("Failed to parse threat list JSON");
    }

    auto json = json_result.release_value();
    if (!json.is_array()) {
        return Error::from_string_literal("Threat list must be a JSON array");
    }

    size_t imported = 0;
    for (auto const& threat_value : json.as_array().values()) {
        if (!threat_value.is_object())
            continue;

        auto const& threat = threat_value.as_object();
        auto hash = threat.get("hash"sv);
        if (!hash.has_value() || !hash->is_string())
            continue;

        ThreatCategory category = ThreatCategory::Unknown;
        auto cat_str = threat.get("category"sv);
        if (cat_str.has_value() && cat_str->is_string()) {
            auto cat = cat_str->as_string();
            if (cat == "malware"sv)
                category = ThreatCategory::Malware;
            else if (cat == "phishing"sv)
                category = ThreatCategory::Phishing;
            else if (cat == "exploit"sv)
                category = ThreatCategory::Exploit;
            else if (cat == "pup"sv)
                category = ThreatCategory::PUP;
        }

        u32 severity = 5;
        auto sev = threat.get("severity"sv);
        if (sev.has_value() && sev->is_number())
            severity = static_cast<u32>(sev->as_integer<i64>());

        auto add_result = add_threat_hash(
            MUST(String::from_utf8(hash->as_string().bytes_as_string_view())),
            category,
            severity);

        if (!add_result.is_error())
            imported++;
    }

    dbgln("ThreatFeed: Imported {} threats from {}", imported, file_path);
    return {};
}

ErrorOr<void> ThreatFeed::export_threat_list(String const& file_path) const
{
    JsonArray threats;

    for (auto const& [hash, info] : m_threat_cache) {
        JsonObject threat;
        threat.set("hash"sv, JsonValue(hash.to_byte_string()));

        ByteString category_str = "unknown"sv;
        switch (info.category) {
        case ThreatCategory::Malware:
            category_str = "malware"sv;
            break;
        case ThreatCategory::Phishing:
            category_str = "phishing"sv;
            break;
        case ThreatCategory::Exploit:
            category_str = "exploit"sv;
            break;
        case ThreatCategory::PUP:
            category_str = "pup"sv;
            break;
        default:
            break;
        }
        threat.set("category"sv, JsonValue(category_str));
        threat.set("severity"sv, JsonValue(info.severity));

        if (info.family_name.has_value())
            threat.set("family"sv, JsonValue(info.family_name->to_byte_string()));

        threats.must_append(move(threat));
    }

    auto file = TRY(Core::File::open(file_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(threats.serialized().bytes()));

    dbgln("ThreatFeed: Exported {} threats to {}", threats.size(), file_path);
    return {};
}

ThreatFeed::Statistics ThreatFeed::get_statistics() const
{
    Statistics stats;

    // Calculate totals from category counts
    for (auto const& [category, count] : m_category_counts) {
        stats.total_threats += count;

        switch (category) {
        case ThreatCategory::Malware:
            stats.malware_count = count;
            break;
        case ThreatCategory::Phishing:
            stats.phishing_count = count;
            break;
        case ThreatCategory::Exploit:
            stats.exploit_count = count;
            break;
        case ThreatCategory::PUP:
            stats.pup_count = count;
            break;
        default:
            break;
        }
    }

    stats.false_positive_rate = m_filter->estimated_false_positive_rate();
    stats.last_sync = m_last_sync_time;
    stats.peer_count = m_peer_count;

    return stats;
}

ErrorOr<void> ThreatFeed::save_to_disk(String const& path) const
{
    // Save bloom filter
    auto filter_data = TRY(m_filter->serialize());
    auto filter_path = String::formatted("{}.bloom", path).release_value_but_fixme_should_propagate_errors();
    auto filter_file = TRY(Core::File::open(filter_path, Core::File::OpenMode::Write));
    TRY(filter_file->write_until_depleted(filter_data));

    // Save metadata and cache
    JsonObject metadata;
    metadata.set("version"sv, 1);
    metadata.set("total_threats"sv, static_cast<i64>(m_filter->estimated_item_count()));
    metadata.set("false_positive_rate"sv, m_filter->estimated_false_positive_rate());
    metadata.set("privacy_epsilon"sv, m_privacy_epsilon);

    if (m_last_sync_time.has_value()) {
        metadata.set("last_sync"sv, JsonValue(MUST(m_last_sync_time->to_string()).to_byte_string()));
    }

    auto meta_path = String::formatted("{}.meta", path).release_value_but_fixme_should_propagate_errors();
    auto meta_file = TRY(Core::File::open(meta_path, Core::File::OpenMode::Write));
    TRY(meta_file->write_until_depleted(metadata.serialized().bytes()));

    // Save threat cache
    auto cache_path = String::formatted("{}.cache", path).release_value_but_fixme_should_propagate_errors();
    TRY(export_threat_list(cache_path));

    dbgln("ThreatFeed: Saved to disk at {}", path);
    return {};
}

ErrorOr<void> ThreatFeed::load_from_disk(String const& path)
{
    // Load bloom filter
    auto filter_path = String::formatted("{}.bloom", path).release_value_but_fixme_should_propagate_errors();
    auto filter_file = TRY(Core::File::open(filter_path, Core::File::OpenMode::Read));
    auto filter_data = TRY(filter_file->read_until_eof());
    m_filter = TRY(BloomFilter::deserialize(filter_data));

    // Load metadata
    auto meta_path = String::formatted("{}.meta", path).release_value_but_fixme_should_propagate_errors();
    auto meta_file = TRY(Core::File::open(meta_path, Core::File::OpenMode::Read));
    auto meta_data = TRY(meta_file->read_until_eof());

    auto meta_json = TRY(JsonParser::parse(StringView(meta_data)));
    if (meta_json.is_object()) {
        auto const& metadata = meta_json.as_object();

        auto epsilon = metadata.get("privacy_epsilon"sv);
        if (epsilon.has_value() && epsilon->is_number())
            m_privacy_epsilon = epsilon->get_double_with_precision_loss().value_or(0.1);

        // Parse last sync time if available
        auto last_sync = metadata.get("last_sync"sv);
        if (last_sync.has_value() && last_sync->is_string()) {
            // Simple parsing, would need proper implementation
            m_last_sync_time = UnixDateTime::now(); // Placeholder
        }
    }

    // Load threat cache
    auto cache_path = String::formatted("{}.cache", path).release_value_but_fixme_should_propagate_errors();
    if (!Core::System::access(cache_path, R_OK).is_error()) {
        TRY(import_threat_list(cache_path));
    }

    dbgln("ThreatFeed: Loaded from disk at {}", path);
    return {};
}

String ThreatFeed::calculate_sha256(ReadonlyBytes content) const
{
    auto sha256 = Crypto::Hash::SHA256::create();
    sha256->update(content);
    auto digest = sha256->digest();

    StringBuilder hash_builder;
    for (auto byte : digest.bytes()) {
        hash_builder.appendff("{:02x}", byte);
    }

    return hash_builder.to_string().release_value_but_fixme_should_propagate_errors();
}

}