/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "AuditLog.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibCore/System.h>
#include <LibFileSystem/FileSystem.h>

namespace Sentinel {

StringView AuditLogger::event_type_to_string(AuditEventType type)
{
    switch (type) {
    case AuditEventType::ScanInitiated:
        return "scan_initiated"sv;
    case AuditEventType::ScanCompleted:
        return "scan_completed"sv;
    case AuditEventType::ThreatDetected:
        return "threat_detected"sv;
    case AuditEventType::FileQuarantined:
        return "file_quarantined"sv;
    case AuditEventType::FileRestored:
        return "file_restored"sv;
    case AuditEventType::FileDeleted:
        return "file_deleted"sv;
    case AuditEventType::PolicyCreated:
        return "policy_created"sv;
    case AuditEventType::PolicyUpdated:
        return "policy_updated"sv;
    case AuditEventType::PolicyDeleted:
        return "policy_deleted"sv;
    case AuditEventType::AccessDenied:
        return "access_denied"sv;
    case AuditEventType::ConfigurationChanged:
        return "configuration_changed"sv;
    case AuditEventType::DatabaseIntegrityCheck:
        return "database_integrity_check"sv;
    case AuditEventType::CacheInvalidated:
        return "cache_invalidated"sv;
    }
    VERIFY_NOT_REACHED();
}

ErrorOr<NonnullOwnPtr<AuditLogger>> AuditLogger::create(String log_path)
{
    // Open log file in append mode
    auto file = TRY(Core::File::open(log_path, Core::File::OpenMode::Append | Core::File::OpenMode::Write));

    auto logger = adopt_own(*new AuditLogger(move(log_path), move(file)));
    logger->m_last_flush_time = UnixDateTime::now();

    dbgln("AuditLogger: Initialized with log path: {}", logger->m_log_path);
    return logger;
}

AuditLogger::AuditLogger(String log_path, NonnullOwnPtr<Core::File> log_file)
    : m_log_path(move(log_path))
    , m_log_file(move(log_file))
{
}

AuditLogger::~AuditLogger()
{
    // Flush any remaining buffered events on destruction
    auto flush_result = flush();
    if (flush_result.is_error()) {
        dbgln("AuditLogger: Failed to flush on destruction: {}", flush_result.error());
    }
}

ErrorOr<void> AuditLogger::log_event(AuditEvent const& event)
{
    Threading::MutexLocker locker(m_mutex);

    // Build JSON object for this event (JSON Lines format)
    JsonObject json;
    json.set("timestamp"sv, JsonValue(event.timestamp.seconds_since_epoch()));
    json.set("type"sv, JsonValue(event_type_to_string(event.type)));
    json.set("user"sv, JsonValue(event.user));
    json.set("resource"sv, JsonValue(event.resource));
    json.set("action"sv, JsonValue(event.action));
    json.set("result"sv, JsonValue(event.result));
    json.set("reason"sv, JsonValue(event.reason));

    // Add metadata object
    JsonObject metadata_json;
    for (auto const& [key, value] : event.metadata) {
        metadata_json.set(key, JsonValue(value));
    }
    json.set("metadata"sv, move(metadata_json));

    // Serialize to JSON string (single line)
    auto json_string = json.serialized();

    // Write to log file (JSON Lines: one JSON object per line)
    TRY(m_log_file->write_until_depleted(json_string.bytes()));
    TRY(m_log_file->write_until_depleted("\n"sv.bytes()));

    // Add to buffer for potential flush
    m_buffer.append(event);
    m_total_events_logged++;

    // Auto-flush if buffer reaches threshold
    if (m_buffer.size() >= m_buffer_size) {
        TRY(flush());
    }

    // Check if log rotation is needed (every 1000 events to avoid excessive checks)
    if (m_total_events_logged % 1000 == 0) {
        TRY(check_and_rotate_log());
    }

    return {};
}

ErrorOr<void> AuditLogger::flush()
{
    // Note: Caller must hold mutex

    if (m_buffer.is_empty()) {
        return {};  // Nothing to flush
    }

    // Sync file to disk
    if (fsync(m_log_file->fd()) < 0) {
        m_flush_errors++;
        auto error = Error::from_syscall("fsync"sv, -errno);
        dbgln("AuditLogger: Failed to sync log file: {}", error);
        return error;
    }

    // Clear buffer after successful flush
    m_buffer.clear();
    m_total_flushes++;
    m_last_flush_time = UnixDateTime::now();

    return {};
}

ErrorOr<void> AuditLogger::log_scan(
    StringView file_path,
    StringView result,
    Vector<String> const& matched_rules)
{
    AuditEventType event_type;
    if (result == "clean"sv) {
        event_type = AuditEventType::ScanCompleted;
    } else if (result == "threat_detected"sv) {
        event_type = AuditEventType::ThreatDetected;
    } else {
        event_type = AuditEventType::ScanInitiated;
    }

    HashMap<String, String> metadata;
    if (!matched_rules.is_empty()) {
        // Join rule names with commas
        StringBuilder rules_builder;
        for (size_t i = 0; i < matched_rules.size(); i++) {
            if (i > 0)
                rules_builder.append(", "sv);
            rules_builder.append(matched_rules[i]);
        }
        auto rules_string = TRY(rules_builder.to_string());
        metadata.set(TRY(String::from_utf8("matched_rules"sv)), rules_string);
    }

    AuditEvent event {
        .type = event_type,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8("sentinel"sv)),
        .resource = TRY(String::from_utf8(file_path)),
        .action = TRY(String::from_utf8("scan"sv)),
        .result = TRY(String::from_utf8(result)),
        .reason = matched_rules.is_empty()
            ? TRY(String::from_utf8("No threats detected"sv))
            : TRY(String::formatted("Matched {} YARA rule(s)", matched_rules.size())),
        .metadata = move(metadata)
    };

    return log_event(event);
}

ErrorOr<void> AuditLogger::log_quarantine(
    StringView file_path,
    StringView quarantine_id,
    StringView reason,
    HashMap<String, String> const& metadata)
{
    HashMap<String, String> enriched_metadata = metadata;
    enriched_metadata.set(TRY(String::from_utf8("quarantine_id"sv)), TRY(String::from_utf8(quarantine_id)));

    AuditEvent event {
        .type = AuditEventType::FileQuarantined,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8("sentinel"sv)),
        .resource = TRY(String::from_utf8(file_path)),
        .action = TRY(String::from_utf8("quarantine"sv)),
        .result = TRY(String::from_utf8("success"sv)),
        .reason = TRY(String::from_utf8(reason)),
        .metadata = move(enriched_metadata)
    };

    return log_event(event);
}

ErrorOr<void> AuditLogger::log_restore(
    StringView quarantine_id,
    StringView destination,
    StringView user,
    bool success)
{
    HashMap<String, String> metadata;
    metadata.set(TRY(String::from_utf8("destination"sv)), TRY(String::from_utf8(destination)));

    AuditEvent event {
        .type = AuditEventType::FileRestored,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8(user)),
        .resource = TRY(String::from_utf8(quarantine_id)),
        .action = TRY(String::from_utf8("restore"sv)),
        .result = success ? TRY(String::from_utf8("success"sv)) : TRY(String::from_utf8("failure"sv)),
        .reason = success ? TRY(String::from_utf8("User requested restoration"sv))
                         : TRY(String::from_utf8("Restoration failed"sv)),
        .metadata = move(metadata)
    };

    return log_event(event);
}

ErrorOr<void> AuditLogger::log_delete(
    StringView quarantine_id,
    StringView user,
    bool success)
{
    AuditEvent event {
        .type = AuditEventType::FileDeleted,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8(user)),
        .resource = TRY(String::from_utf8(quarantine_id)),
        .action = TRY(String::from_utf8("delete"sv)),
        .result = success ? TRY(String::from_utf8("success"sv)) : TRY(String::from_utf8("failure"sv)),
        .reason = success ? TRY(String::from_utf8("User requested deletion"sv))
                         : TRY(String::from_utf8("Deletion failed"sv)),
        .metadata = {}
    };

    return log_event(event);
}

ErrorOr<void> AuditLogger::log_policy_change(
    StringView operation,
    StringView policy_id,
    StringView details)
{
    AuditEventType event_type;
    if (operation == "create"sv) {
        event_type = AuditEventType::PolicyCreated;
    } else if (operation == "update"sv) {
        event_type = AuditEventType::PolicyUpdated;
    } else if (operation == "delete"sv) {
        event_type = AuditEventType::PolicyDeleted;
    } else {
        return Error::from_string_literal("Invalid policy operation");
    }

    AuditEvent event {
        .type = event_type,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8("sentinel"sv)),
        .resource = TRY(String::formatted("policy:{}", policy_id)),
        .action = TRY(String::from_utf8(operation)),
        .result = TRY(String::from_utf8("success"sv)),
        .reason = TRY(String::from_utf8(details)),
        .metadata = {}
    };

    return log_event(event);
}

ErrorOr<void> AuditLogger::log_config_change(
    StringView setting_name,
    StringView old_value,
    StringView new_value,
    StringView user)
{
    HashMap<String, String> metadata;
    metadata.set(TRY(String::from_utf8("old_value"sv)), TRY(String::from_utf8(old_value)));
    metadata.set(TRY(String::from_utf8("new_value"sv)), TRY(String::from_utf8(new_value)));

    AuditEvent event {
        .type = AuditEventType::ConfigurationChanged,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8(user)),
        .resource = TRY(String::from_utf8(setting_name)),
        .action = TRY(String::from_utf8("config_change"sv)),
        .result = TRY(String::from_utf8("success"sv)),
        .reason = TRY(String::formatted("Changed {} from '{}' to '{}'", setting_name, old_value, new_value)),
        .metadata = move(metadata)
    };

    return log_event(event);
}

ErrorOr<void> AuditLogger::log_access_denied(
    StringView resource,
    StringView reason,
    HashMap<String, String> const& metadata)
{
    AuditEvent event {
        .type = AuditEventType::AccessDenied,
        .timestamp = UnixDateTime::now(),
        .user = TRY(String::from_utf8("sentinel"sv)),
        .resource = TRY(String::from_utf8(resource)),
        .action = TRY(String::from_utf8("access"sv)),
        .result = TRY(String::from_utf8("denied"sv)),
        .reason = TRY(String::from_utf8(reason)),
        .metadata = metadata
    };

    return log_event(event);
}

AuditLogger::Statistics AuditLogger::get_statistics() const
{
    Threading::MutexLocker locker(m_mutex);

    return Statistics {
        .total_events_logged = m_total_events_logged,
        .events_in_buffer = m_buffer.size(),
        .total_flushes = m_total_flushes,
        .flush_errors = m_flush_errors,
        .last_flush_time = m_last_flush_time
    };
}

ErrorOr<void> AuditLogger::check_and_rotate_log()
{
    // Check current log file size
    auto stat_result = Core::System::fstat(m_log_file->fd());
    if (stat_result.is_error()) {
        dbgln("AuditLogger: Failed to stat log file: {}", stat_result.error());
        return {};  // Don't fail on stat error, just skip rotation
    }

    auto file_stat = stat_result.release_value();
    auto file_size = static_cast<size_t>(file_stat.st_size);

    // Rotate if file exceeds maximum size
    if (file_size >= m_max_file_size) {
        dbgln("AuditLogger: Log file size ({} bytes) exceeds maximum ({}), rotating",
            file_size, m_max_file_size);
        return rotate_log();
    }

    return {};
}

ErrorOr<void> AuditLogger::rotate_log()
{
    Threading::MutexLocker locker(m_mutex);

    // Flush any pending events first
    TRY(flush());

    // Note: We keep the log file open during rotation. It will be closed automatically
    // when we reassign m_log_file at the end.

    // Rotate existing log files: audit.log.9 -> delete, audit.log.8 -> audit.log.9, etc.
    for (int i = m_max_rotated_files - 1; i >= 1; i--) {
        auto old_name = ByteString::formatted("{}.{}", m_log_path, i);
        auto new_name = ByteString::formatted("{}.{}", m_log_path, i + 1);

        if (FileSystem::exists(old_name)) {
            if (i == static_cast<int>(m_max_rotated_files - 1)) {
                // Delete oldest log file
                auto remove_result = FileSystem::remove(String::from_byte_string(old_name).release_value(),
                    FileSystem::RecursionMode::Disallowed);
                if (remove_result.is_error()) {
                    dbgln("AuditLogger: Failed to delete old log {}: {}", old_name, remove_result.error());
                }
            } else {
                // Rename to next number
                auto rename_result = Core::System::rename(old_name, new_name);
                if (rename_result.is_error()) {
                    dbgln("AuditLogger: Failed to rotate {} to {}: {}", old_name, new_name, rename_result.error());
                }
            }
        }
    }

    // Rename current log to .1
    auto rotated_name = ByteString::formatted("{}.1", m_log_path);
    if (FileSystem::exists(m_log_path)) {
        auto rename_result = Core::System::rename(ByteString(m_log_path), rotated_name);
        if (rename_result.is_error()) {
            dbgln("AuditLogger: Failed to rotate current log: {}", rename_result.error());
            // Continue anyway and create new log file
        }
    }

    // Open new log file
    auto new_file = TRY(Core::File::open(m_log_path, Core::File::OpenMode::Append | Core::File::OpenMode::Write));
    m_log_file = move(new_file);

    dbgln("AuditLogger: Log rotation complete");
    return {};
}

}
