/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/HashMap.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibCore/File.h>
#include <LibCore/MappedFile.h>
#include <LibThreading/Mutex.h>

namespace Sentinel {

// Audit event types for security-relevant actions
enum class AuditEventType {
    ScanInitiated,          // File scan started
    ScanCompleted,          // File scan completed (clean)
    ThreatDetected,         // Malware/threat detected
    FileQuarantined,        // File moved to quarantine
    FileRestored,           // File restored from quarantine
    FileDeleted,            // Quarantined file deleted
    PolicyCreated,          // New policy created
    PolicyUpdated,          // Policy modified
    PolicyDeleted,          // Policy removed
    AccessDenied,           // Access denied by policy
    ConfigurationChanged,   // Sentinel configuration changed
    DatabaseIntegrityCheck, // Database integrity verified
    CacheInvalidated,       // Policy cache cleared
};

// Single audit event record
struct AuditEvent {
    AuditEventType type;
    UnixDateTime timestamp;
    String user;                       // Process/user performing action (e.g., "sentinel", "user", "webcontent")
    String resource;                   // File path, policy ID, quarantine ID, etc.
    String action;                     // "scan", "quarantine", "block", "restore", "delete"
    String result;                     // "success", "failure", "allowed", "blocked", "quarantined"
    String reason;                     // Why decision was made (rule names, policy match, etc.)
    HashMap<String, String> metadata;  // Additional context (file_size, sha256, etc.)
};

// Audit logger for structured security event logging
class AuditLogger {
public:
    // Create audit logger with specified log file path
    static ErrorOr<NonnullOwnPtr<AuditLogger>> create(String log_path);

    ~AuditLogger();

    // Log a complete audit event
    ErrorOr<void> log_event(AuditEvent const& event);

    // Convenience methods for common audit events

    // Log a file scan event (initiated, completed, or threat detected)
    ErrorOr<void> log_scan(
        StringView file_path,
        StringView result,                    // "clean", "threat_detected", "error"
        Vector<String> const& matched_rules); // Empty if clean

    // Log a quarantine action
    ErrorOr<void> log_quarantine(
        StringView file_path,
        StringView quarantine_id,
        StringView reason,
        HashMap<String, String> const& metadata = {});

    // Log a restore action
    ErrorOr<void> log_restore(
        StringView quarantine_id,
        StringView destination,
        StringView user,
        bool success);

    // Log a delete action
    ErrorOr<void> log_delete(
        StringView quarantine_id,
        StringView user,
        bool success);

    // Log a policy change (create, update, delete)
    ErrorOr<void> log_policy_change(
        StringView operation,  // "create", "update", "delete"
        StringView policy_id,
        StringView details);

    // Log configuration change
    ErrorOr<void> log_config_change(
        StringView setting_name,
        StringView old_value,
        StringView new_value,
        StringView user);

    // Log access denied event
    ErrorOr<void> log_access_denied(
        StringView resource,
        StringView reason,
        HashMap<String, String> const& metadata = {});

    // Force flush buffered events to disk
    ErrorOr<void> flush();

    // Get statistics
    struct Statistics {
        size_t total_events_logged { 0 };
        size_t events_in_buffer { 0 };
        size_t total_flushes { 0 };
        size_t flush_errors { 0 };
        UnixDateTime last_flush_time;
    };
    Statistics get_statistics() const;

    // Set buffer size (number of events before auto-flush)
    void set_buffer_size(size_t size) { m_buffer_size = size; }
    size_t buffer_size() const { return m_buffer_size; }

private:
    AuditLogger(String log_path, NonnullOwnPtr<Core::File> log_file);

    // Convert event type to string
    static StringView event_type_to_string(AuditEventType type);

    // Check if log rotation is needed and perform it
    ErrorOr<void> check_and_rotate_log();

    // Perform log rotation
    ErrorOr<void> rotate_log();

    String m_log_path;
    NonnullOwnPtr<Core::File> m_log_file;
    Vector<AuditEvent> m_buffer;
    size_t m_buffer_size { 100 };  // Flush after 100 events

    // Thread safety
    mutable Threading::Mutex m_mutex;

    // Statistics
    mutable size_t m_total_events_logged { 0 };
    mutable size_t m_total_flushes { 0 };
    mutable size_t m_flush_errors { 0 };
    mutable UnixDateTime m_last_flush_time;

    // Log rotation settings
    size_t m_max_file_size { 100 * 1024 * 1024 };  // 100MB default
    size_t m_max_rotated_files { 10 };             // Keep 10 old logs
};

}
