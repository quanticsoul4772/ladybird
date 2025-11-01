/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/Directory.h>
#include <LibCore/File.h>
#include <LibCore/StandardPaths.h>
#include <LibFileSystem/FileSystem.h>
#include <LibWebView/SentinelConfig.h>
#include <Services/Sentinel/InputValidator.h>

namespace WebView {

ErrorOr<String> SentinelConfig::default_yara_rules_path()
{
    auto config_dir = Core::StandardPaths::config_directory();
    StringBuilder path_builder;
    path_builder.append(config_dir);
    path_builder.append("/ladybird/sentinel/rules"sv);
    return path_builder.to_string();
}

ErrorOr<String> SentinelConfig::default_quarantine_path()
{
    auto data_dir = Core::StandardPaths::user_data_directory();
    StringBuilder path_builder;
    path_builder.append(data_dir);
    path_builder.append("/Ladybird/Quarantine"sv);
    return path_builder.to_string();
}

ErrorOr<String> SentinelConfig::default_database_path()
{
    auto data_dir = Core::StandardPaths::user_data_directory();
    StringBuilder path_builder;
    path_builder.append(data_dir);
    path_builder.append("/Ladybird/PolicyGraph/policies.db"sv);
    return path_builder.to_string();
}

ErrorOr<String> SentinelConfig::default_audit_log_path()
{
    auto data_dir = Core::StandardPaths::user_data_directory();
    StringBuilder path_builder;
    path_builder.append(data_dir);
    path_builder.append("/Ladybird/audit.log"sv);
    return path_builder.to_string();
}

ErrorOr<String> SentinelConfig::default_config_path()
{
    auto config_dir = Core::StandardPaths::config_directory();
    StringBuilder path_builder;
    path_builder.append(config_dir);
    path_builder.append("/ladybird/sentinel/config.json"sv);
    return path_builder.to_string();
}

SentinelConfig SentinelConfig::create_default()
{
    SentinelConfig config;
    config.enabled = true;
    config.yara_rules_path = MUST(default_yara_rules_path());
    config.quarantine_path = MUST(default_quarantine_path());
    config.database_path = MUST(default_database_path());
    config.policy_cache_size = 1000;
    config.threat_retention_days = 30;
    config.rate_limit = RateLimitConfig { .policies_per_minute = 5, .rate_window_seconds = 60 };
    config.notifications = NotificationConfig { .enabled = true, .auto_dismiss_seconds = 5, .max_queue_size = 10 };
    config.scan_size = ScanSizeConfig {
        .small_file_threshold = 10 * 1024 * 1024,
        .medium_file_threshold = 100 * 1024 * 1024,
        .max_scan_size = 200 * 1024 * 1024,
        .chunk_size = 1 * 1024 * 1024,
        .scan_large_files_partially = true,
        .large_file_scan_bytes = 10 * 1024 * 1024
    };
    config.audit_log = AuditLogConfig {
        .enabled = true,
        .log_path = MUST(default_audit_log_path()),
        .max_file_size = 100 * 1024 * 1024,
        .max_files = 10,
        .log_clean_scans = false,
        .buffer_size = 100
    };
    config.network_monitoring = NetworkMonitoringConfig {
        .enabled = true,
        .dga_threshold = 0.6f,
        .beaconing_threshold = 0.7f,
        .exfiltration_threshold = 0.6f
    };
    return config;
}

String SentinelConfig::to_json() const
{
    JsonObject json;
    json.set("enabled"sv, enabled);
    json.set("yara_rules_path"sv, yara_rules_path);
    json.set("quarantine_path"sv, quarantine_path);
    json.set("database_path"sv, database_path);
    json.set("policy_cache_size"sv, static_cast<u64>(policy_cache_size));
    json.set("threat_retention_days"sv, static_cast<u64>(threat_retention_days));

    // Rate limit config
    JsonObject rate_limit_json;
    rate_limit_json.set("policies_per_minute"sv, static_cast<u64>(rate_limit.policies_per_minute));
    rate_limit_json.set("rate_window_seconds"sv, static_cast<u64>(rate_limit.rate_window_seconds));
    json.set("rate_limit"sv, rate_limit_json);

    // Notification config
    JsonObject notifications_json;
    notifications_json.set("enabled"sv, notifications.enabled);
    notifications_json.set("auto_dismiss_seconds"sv, static_cast<u64>(notifications.auto_dismiss_seconds));
    notifications_json.set("max_queue_size"sv, static_cast<u64>(notifications.max_queue_size));
    json.set("notifications"sv, notifications_json);

    // Scan size config
    JsonObject scan_size_json;
    scan_size_json.set("small_file_threshold"sv, static_cast<u64>(scan_size.small_file_threshold));
    scan_size_json.set("medium_file_threshold"sv, static_cast<u64>(scan_size.medium_file_threshold));
    scan_size_json.set("max_scan_size"sv, static_cast<u64>(scan_size.max_scan_size));
    scan_size_json.set("chunk_size"sv, static_cast<u64>(scan_size.chunk_size));
    scan_size_json.set("scan_large_files_partially"sv, scan_size.scan_large_files_partially);
    scan_size_json.set("large_file_scan_bytes"sv, static_cast<u64>(scan_size.large_file_scan_bytes));
    json.set("scan_size"sv, scan_size_json);

    // Audit log config
    JsonObject audit_log_json;
    audit_log_json.set("enabled"sv, audit_log.enabled);
    audit_log_json.set("log_path"sv, audit_log.log_path);
    audit_log_json.set("max_file_size"sv, static_cast<u64>(audit_log.max_file_size));
    audit_log_json.set("max_files"sv, static_cast<u64>(audit_log.max_files));
    audit_log_json.set("log_clean_scans"sv, audit_log.log_clean_scans);
    audit_log_json.set("buffer_size"sv, static_cast<u64>(audit_log.buffer_size));
    json.set("audit_log"sv, audit_log_json);

    // Network monitoring config
    JsonObject network_monitoring_json;
    network_monitoring_json.set("enabled"sv, network_monitoring.enabled);
    network_monitoring_json.set("dga_threshold"sv, static_cast<double>(network_monitoring.dga_threshold));
    network_monitoring_json.set("beaconing_threshold"sv, static_cast<double>(network_monitoring.beaconing_threshold));
    network_monitoring_json.set("exfiltration_threshold"sv, static_cast<double>(network_monitoring.exfiltration_threshold));
    json.set("network_monitoring"sv, network_monitoring_json);

    return json.serialized();
}

ErrorOr<SentinelConfig> SentinelConfig::from_json(String const& json_string)
{
    auto json = TRY(JsonValue::from_string(json_string));
    if (!json.is_object())
        return Error::from_string_literal("Config must be a JSON object");

    auto const& obj = json.as_object();

    SentinelConfig config;

    // Load main settings
    config.enabled = obj.get_bool("enabled"sv).value_or(true);

    auto yara_path = obj.get_string("yara_rules_path"sv);
    config.yara_rules_path = yara_path.has_value() ? yara_path.value() : TRY(default_yara_rules_path());

    auto quarantine_path = obj.get_string("quarantine_path"sv);
    config.quarantine_path = quarantine_path.has_value() ? quarantine_path.value() : TRY(default_quarantine_path());

    auto db_path = obj.get_string("database_path"sv);
    config.database_path = db_path.has_value() ? db_path.value() : TRY(default_database_path());

    // Validate and load policy_cache_size
    auto cache_size_value = obj.get_u64("policy_cache_size"sv).value_or(1000);
    auto cache_size_validation = Sentinel::InputValidator::validate_config_value("policy_cache_size"sv, JsonValue(cache_size_value));
    if (!cache_size_validation.is_valid)
        return Error::from_string_literal("Invalid policy_cache_size value");
    config.policy_cache_size = static_cast<size_t>(cache_size_value);

    // Validate and load threat_retention_days
    auto retention_days_value = obj.get_u64("threat_retention_days"sv).value_or(30);
    auto retention_days_validation = Sentinel::InputValidator::validate_config_value("threat_retention_days"sv, JsonValue(retention_days_value));
    if (!retention_days_validation.is_valid)
        return Error::from_string_literal("Invalid threat_retention_days value");
    config.threat_retention_days = static_cast<size_t>(retention_days_value);

    // Load rate limit config with validation
    auto rate_limit_opt = obj.get_object("rate_limit"sv);
    if (rate_limit_opt.has_value()) {
        auto const& rl = rate_limit_opt.value();

        auto policies_per_min = rl.get_u64("policies_per_minute"sv).value_or(5);
        auto policies_validation = Sentinel::InputValidator::validate_config_value("policies_per_minute"sv, JsonValue(policies_per_min));
        if (!policies_validation.is_valid)
            return Error::from_string_literal("Invalid policies_per_minute value");
        config.rate_limit.policies_per_minute = static_cast<size_t>(policies_per_min);

        auto rate_window = rl.get_u64("rate_window_seconds"sv).value_or(60);
        auto window_validation = Sentinel::InputValidator::validate_config_value("rate_window_seconds"sv, JsonValue(rate_window));
        if (!window_validation.is_valid)
            return Error::from_string_literal("Invalid rate_window_seconds value");
        config.rate_limit.rate_window_seconds = static_cast<size_t>(rate_window);
    }

    // Load notification config
    auto notif_opt = obj.get_object("notifications"sv);
    if (notif_opt.has_value()) {
        auto const& notif = notif_opt.value();
        config.notifications.enabled = notif.get_bool("enabled"sv).value_or(true);
        config.notifications.auto_dismiss_seconds = static_cast<size_t>(notif.get_u64("auto_dismiss_seconds"sv).value_or(5));
        config.notifications.max_queue_size = static_cast<size_t>(notif.get_u64("max_queue_size"sv).value_or(10));
    }

    // Load scan size config
    auto scan_size_opt = obj.get_object("scan_size"sv);
    if (scan_size_opt.has_value()) {
        auto const& ss = scan_size_opt.value();
        config.scan_size.small_file_threshold = static_cast<size_t>(ss.get_u64("small_file_threshold"sv).value_or(10 * 1024 * 1024));
        config.scan_size.medium_file_threshold = static_cast<size_t>(ss.get_u64("medium_file_threshold"sv).value_or(100 * 1024 * 1024));
        config.scan_size.max_scan_size = static_cast<size_t>(ss.get_u64("max_scan_size"sv).value_or(200 * 1024 * 1024));
        config.scan_size.chunk_size = static_cast<size_t>(ss.get_u64("chunk_size"sv).value_or(1 * 1024 * 1024));
        config.scan_size.scan_large_files_partially = ss.get_bool("scan_large_files_partially"sv).value_or(true);
        config.scan_size.large_file_scan_bytes = static_cast<size_t>(ss.get_u64("large_file_scan_bytes"sv).value_or(10 * 1024 * 1024));
    }

    // Load audit log config
    auto audit_log_opt = obj.get_object("audit_log"sv);
    if (audit_log_opt.has_value()) {
        auto const& al = audit_log_opt.value();
        config.audit_log.enabled = al.get_bool("enabled"sv).value_or(true);
        auto log_path = al.get_string("log_path"sv);
        config.audit_log.log_path = log_path.has_value() ? log_path.value() : TRY(default_audit_log_path());
        config.audit_log.max_file_size = static_cast<size_t>(al.get_u64("max_file_size"sv).value_or(100 * 1024 * 1024));
        config.audit_log.max_files = static_cast<size_t>(al.get_u64("max_files"sv).value_or(10));
        config.audit_log.log_clean_scans = al.get_bool("log_clean_scans"sv).value_or(false);
        config.audit_log.buffer_size = static_cast<size_t>(al.get_u64("buffer_size"sv).value_or(100));
    } else {
        // Use defaults if not present
        config.audit_log.enabled = true;
        config.audit_log.log_path = TRY(default_audit_log_path());
        config.audit_log.max_file_size = 100 * 1024 * 1024;
        config.audit_log.max_files = 10;
        config.audit_log.log_clean_scans = false;
        config.audit_log.buffer_size = 100;
    }

    // Load network monitoring config
    auto network_monitoring_opt = obj.get_object("network_monitoring"sv);
    if (network_monitoring_opt.has_value()) {
        auto const& nm = network_monitoring_opt.value();
        config.network_monitoring.enabled = nm.get_bool("enabled"sv).value_or(true);

        auto dga_threshold = nm.get_double_with_precision_loss("dga_threshold"sv).value_or(0.6);
        config.network_monitoring.dga_threshold = static_cast<float>(dga_threshold);

        auto beaconing_threshold = nm.get_double_with_precision_loss("beaconing_threshold"sv).value_or(0.7);
        config.network_monitoring.beaconing_threshold = static_cast<float>(beaconing_threshold);

        auto exfiltration_threshold = nm.get_double_with_precision_loss("exfiltration_threshold"sv).value_or(0.6);
        config.network_monitoring.exfiltration_threshold = static_cast<float>(exfiltration_threshold);
    } else {
        // Use defaults if not present
        config.network_monitoring.enabled = true;
        config.network_monitoring.dga_threshold = 0.6f;
        config.network_monitoring.beaconing_threshold = 0.7f;
        config.network_monitoring.exfiltration_threshold = 0.6f;
    }

    return config;
}

ErrorOr<SentinelConfig> SentinelConfig::load_from_file(String const& path)
{
    // Check if file exists
    if (!FileSystem::exists(path)) {
        dbgln("SentinelConfig: Config file not found at {}, using defaults", path);
        return create_default();
    }

    // Read file
    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Read));
    auto contents = TRY(file->read_until_eof());
    auto json_string = TRY(String::from_utf8(contents));

    // Parse config
    return from_json(json_string);
}

ErrorOr<SentinelConfig> SentinelConfig::load_default()
{
    auto config_path = TRY(default_config_path());
    return load_from_file(config_path);
}

ErrorOr<void> SentinelConfig::save_to_file(String const& path) const
{
    // Ensure parent directory exists
    auto path_parts = TRY(path.split('/'));
    if (path_parts.size() > 1) {
        StringBuilder parent_builder;
        for (size_t i = 0; i < path_parts.size() - 1; i++) {
            if (i > 0)
                parent_builder.append('/');
            parent_builder.append(path_parts[i]);
        }
        auto parent_path = TRY(parent_builder.to_string());
        TRY(Core::Directory::create(ByteString(parent_path), Core::Directory::CreateDirectories::Yes));
    }

    // Write config
    auto json = to_json();
    auto file = TRY(Core::File::open(path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(json.bytes()));

    dbgln("SentinelConfig: Saved config to {}", path);
    return {};
}

}
