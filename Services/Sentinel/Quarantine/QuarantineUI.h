/*
 * Copyright (c) 2025, Ladybird contributors
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/String.h>
#include <AK/Vector.h>

namespace Sentinel::Quarantine {

// IPC Message Stubs for Future UI Integration
// These messages will be used for communication between Sentinel and the UI process
// TODO: Convert to proper LibIPC definitions when implementing UI

// Message: File has been quarantined (alert user)
struct QuarantineFileNotification {
    i64 quarantine_id;
    String original_path;
    String quarantine_reason;
    float threat_score;
    i32 threat_level; // 0=Clean, 1=Suspicious, 2=Malicious, 3=Critical
    i64 quarantined_at_timestamp;

    // Example JSON format for IPC:
    // {
    //   "type": "quarantine_file_notification",
    //   "quarantine_id": 123,
    //   "original_path": "/home/user/Downloads/malware.exe",
    //   "quarantine_reason": "Threat Level: 2 | Confidence: 95.0% | Behaviors: 3 | Rules: 2",
    //   "threat_score": 0.89,
    //   "threat_level": 2,
    //   "quarantined_at": 1704067200
    // }
};

// Message: User requests list of quarantined files (UI file browser)
struct ListQuarantinedFilesRequest {
    Optional<i32> threat_level_filter; // Optional filter by threat level

    // Example JSON format:
    // {
    //   "type": "list_quarantined_files_request",
    //   "threat_level_filter": 2  // Optional: filter by Malicious (2)
    // }
};

// Message: Response with list of quarantined files
struct ListQuarantinedFilesResponse {
    struct QuarantineEntry {
        i64 id;
        String original_path;
        String quarantine_path;
        String quarantine_reason;
        float threat_score;
        i32 threat_level;
        i64 quarantined_at_timestamp;
        u64 file_size;
        String sha256_hash;
    };

    Vector<QuarantineEntry> files;
    u64 total_count;
    u64 total_size_bytes;

    // Example JSON format:
    // {
    //   "type": "list_quarantined_files_response",
    //   "files": [
    //     {
    //       "id": 123,
    //       "original_path": "/home/user/Downloads/malware.exe",
    //       "quarantine_path": "~/.local/share/ladybird/quarantine/1704067200_a1b2c3d4_malware.exe.quar",
    //       "quarantine_reason": "Malicious dropper detected",
    //       "threat_score": 0.89,
    //       "threat_level": 2,
    //       "quarantined_at": 1704067200,
    //       "file_size": 102400,
    //       "sha256_hash": "a1b2c3d4e5f6..."
    //     }
    //   ],
    //   "total_count": 1,
    //   "total_size_bytes": 102400
    // }
};

// Message: User requests to restore a false positive
struct RestoreFileRequest {
    i64 quarantine_id;
    String target_path; // Where to restore the file

    // Example JSON format:
    // {
    //   "type": "restore_file_request",
    //   "quarantine_id": 123,
    //   "target_path": "/home/user/Downloads/safe_file.exe"
    // }
};

// Message: Response to restore request
struct RestoreFileResponse {
    bool success;
    Optional<String> error_message;

    // Example JSON format (success):
    // {
    //   "type": "restore_file_response",
    //   "success": true
    // }
    //
    // Example JSON format (error):
    // {
    //   "type": "restore_file_response",
    //   "success": false,
    //   "error_message": "Quarantined file not found on disk"
    // }
};

// Message: User requests permanent deletion
struct DeleteFileRequest {
    i64 quarantine_id;
    bool confirm_deletion; // Require explicit confirmation

    // Example JSON format:
    // {
    //   "type": "delete_file_request",
    //   "quarantine_id": 123,
    //   "confirm_deletion": true
    // }
};

// Message: Response to delete request
struct DeleteFileResponse {
    bool success;
    Optional<String> error_message;

    // Example JSON format:
    // {
    //   "type": "delete_file_response",
    //   "success": true
    // }
};

// Message: User requests quarantine statistics
struct GetStatisticsRequest {
    // No parameters needed

    // Example JSON format:
    // {
    //   "type": "get_statistics_request"
    // }
};

// Message: Response with quarantine statistics
struct GetStatisticsResponse {
    u64 total_quarantined;
    u64 total_restored;
    u64 total_deleted;
    u64 total_expired_cleaned;
    u64 current_quarantine_count;
    u64 total_quarantine_size_bytes;

    // Example JSON format:
    // {
    //   "type": "get_statistics_response",
    //   "total_quarantined": 42,
    //   "total_restored": 5,
    //   "total_deleted": 10,
    //   "total_expired_cleaned": 12,
    //   "current_quarantine_count": 15,
    //   "total_quarantine_size_bytes": 1048576
    // }
};

// Message: User requests cleanup of expired files
struct CleanupExpiredRequest {
    i32 retention_days; // How many days to retain files (default: 30)

    // Example JSON format:
    // {
    //   "type": "cleanup_expired_request",
    //   "retention_days": 30
    // }
};

// Message: Response to cleanup request
struct CleanupExpiredResponse {
    u64 files_cleaned;
    u64 bytes_freed;

    // Example JSON format:
    // {
    //   "type": "cleanup_expired_response",
    //   "files_cleaned": 12,
    //   "bytes_freed": 524288
    // }
};

}

// Future Implementation Notes:
//
// 1. Convert to LibIPC:
//    - Create Quarantine.ipc file defining these messages
//    - Use LibIPC code generator to create C++ stubs
//    - Implement handlers in Sentinel service and UI process
//
// 2. UI Integration Points:
//    - Quarantine notification dialog (modal alert when file quarantined)
//    - Quarantine manager window (list view with restore/delete buttons)
//    - Settings panel (configure retention period, auto-cleanup)
//    - System tray notification (lightweight alert for quarantined files)
//
// 3. RequestServer Integration:
//    - Add quarantine check in ConnectionFromClient::did_finish_request()
//    - Send QuarantineFileNotification to UI when file quarantined
//    - Block file delivery if quarantined (security measure)
//
// 4. User Workflow:
//    a) Download completes → Sandbox analysis → File quarantined
//    b) User sees notification: "File quarantined: malware.exe (Malicious)"
//    c) User opens Quarantine Manager from menu/notification
//    d) User reviews threat details and decides:
//       - Restore (if false positive)
//       - Delete permanently (confirm threat)
//       - Leave in quarantine (default: 30-day retention)
//
// 5. Security Considerations:
//    - All IPC messages must be validated (prevent path traversal)
//    - Restore operations require explicit user confirmation
//    - Delete operations should show threat details before confirming
//    - UI should display threat score and explanation prominently
//
// 6. Testing:
//    - Mock IPC messages in unit tests
//    - Integration tests with test UI process
//    - User acceptance testing with real malware samples
//
// Example UI Implementation (Qt):
//
// class QuarantineDialog : public QDialog {
// public:
//     void show_quarantine_notification(QuarantineFileNotification const& notification) {
//         auto message = QString::fromUtf8(notification.quarantine_reason);
//         auto threat_level_str = threat_level_to_string(notification.threat_level);
//
//         QMessageBox::warning(this, "File Quarantined",
//             QString("The downloaded file has been quarantined:\n\n"
//                     "File: %1\n"
//                     "Threat: %2\n"
//                     "Score: %3\n\n"
//                     "You can review and manage quarantined files from "
//                     "Settings > Security > Quarantine Manager.")
//                 .arg(notification.original_path)
//                 .arg(threat_level_str)
//                 .arg(notification.threat_score, 0, 'f', 2));
//     }
// };
