# Quarantine System

## Overview

The Quarantine System provides automated file isolation and user rollback capabilities for malicious files detected by Sentinel's malware detection pipeline.

## Architecture

### Components

1. **FileEncryption**: AES-256 encryption utilities for secure file storage
2. **QuarantineManager**: Main quarantine management class
3. **PolicyGraph Integration**: Database storage for quarantine records
4. **DatabaseMigrations**: Schema v7→v8 adds quarantine table

### File Structure

```
Services/Sentinel/Quarantine/
├── FileEncryption.h/.cpp      # AES-256 encryption/decryption
├── QuarantineManager.h/.cpp   # Quarantine management
└── QuarantineUI.h             # IPC message definitions (future UI)
```

## Database Schema

```sql
CREATE TABLE quarantine (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    original_path TEXT NOT NULL,
    quarantine_path TEXT NOT NULL UNIQUE,
    quarantine_reason TEXT NOT NULL,
    threat_score REAL NOT NULL,
    threat_level INTEGER NOT NULL,        -- 0=Clean, 1=Suspicious, 2=Malicious, 3=Critical
    quarantined_at INTEGER NOT NULL,
    file_size INTEGER NOT NULL,
    sha256_hash TEXT NOT NULL
);
CREATE INDEX idx_quarantine_timestamp ON quarantine(quarantined_at);
CREATE INDEX idx_quarantine_hash ON quarantine(sha256_hash);
```

## Integration Points

### RequestServer Integration (Primary Use Case)

The QuarantineManager is designed to be used by RequestServer when handling downloads:

```cpp
// In RequestServer::ConnectionFromClient::handle_download_complete()

// 1. Sandbox analysis performed by Orchestrator
auto analysis_result = TRY(m_orchestrator->analyze_file(file_data, filename));

// 2. If malicious, quarantine the file
if (analysis_result.threat_level >= SandboxResult::ThreatLevel::Malicious) {
    // Write file to disk first (temporary location)
    auto temp_path = write_to_temp_location(file_data, filename);

    // Quarantine the file
    auto record = TRY(m_quarantine_manager->quarantine_file(temp_path, analysis_result));

    // Notify user via IPC
    send_download_quarantined_notification(record);
}
```

### Orchestrator Design Note

The Orchestrator works with file data in memory (ByteBuffer) for performance and security:
- Avoids writing suspicious files to disk unnecessarily
- Enables analysis before filesystem exposure
- Supports streaming analysis for large files

QuarantineManager works with file paths on disk because:
- Files must be persistently stored in quarantine directory
- Encryption requires file I/O operations
- User restoration requires original file path context

**Therefore**: Quarantine operations happen **after** Orchestrator analysis, at the RequestServer level where file download paths are known.

## Usage Examples

### Quarantine a Malicious File

```cpp
auto quarantine_dir = "~/.local/share/ladybird/quarantine"_string;
auto db_path = "~/.cache/ladybird/sentinel"_string;

auto mgr = TRY(QuarantineManager::create(quarantine_dir, db_path));

// After Orchestrator analysis identifies a threat
SandboxResult analysis = /* ... */;
auto record = TRY(mgr->quarantine_file("/path/to/malicious.exe", analysis));

// File is now encrypted and moved to quarantine directory
// Original file deleted from download location
```

### List Quarantined Files

```cpp
// All files
auto all_files = TRY(mgr->list_quarantined_files());

// Filter by threat level
auto critical_files = TRY(mgr->list_quarantined_files(
    SandboxResult::ThreatLevel::Critical));

for (auto const& record : critical_files) {
    println("ID: {}, Path: {}, Reason: {}, Score: {:.2f}",
        record.id, record.original_path, record.quarantine_reason, record.threat_score);
}
```

### Restore a False Positive

```cpp
// User confirms file is safe (via UI)
TRY(mgr->restore_file(record_id, "/home/user/Downloads/safe_file.exe"));

// File is decrypted and moved back to target location
// Quarantine record removed from database
```

### Permanently Delete

```cpp
TRY(mgr->delete_file(record_id));

// Encrypted file deleted from quarantine directory
// Database record removed
```

### Cleanup Expired Files

```cpp
// Clean up files older than 30 days (default)
auto count = TRY(mgr->cleanup_expired());
println("Cleaned up {} expired files", count);

// Custom retention period
auto count = TRY(mgr->cleanup_expired(Duration::from_days(7)));
```

## Security Features

### Encryption

- **Algorithm**: AES-256-CBC
- **Key Storage**: `~/.local/share/ladybird/quarantine/encryption.key`
- **Key Permissions**: 0600 (owner read/write only)
- **IV Generation**: Cryptographically secure random (getrandom)
- **Padding**: PKCS#7 (standard AES padding)

### File Isolation

- **Quarantine Directory**: `~/.local/share/ladybird/quarantine/`
- **Directory Permissions**: 0700 (owner only)
- **File Format**: `<timestamp>_<hash_prefix>_<filename>.quar`
- **Encrypted Format**: `[16-byte IV][encrypted data]`

### Duplicate Detection

- Files are hashed (SHA256) before quarantine
- Duplicate quarantine attempts blocked by hash lookup
- Index on `sha256_hash` for fast duplicate checks

## Statistics Tracking

```cpp
auto stats = mgr->get_statistics();

println("Total Quarantined: {}", stats.total_quarantined);
println("Total Restored: {}", stats.total_restored);
println("Total Deleted: {}", stats.total_deleted);
println("Expired Cleaned: {}", stats.total_expired_cleaned);
println("Current Count: {}", stats.current_quarantine_count);
println("Total Size: {} bytes", stats.total_quarantine_size_bytes);
```

## Testing

Unit tests in `TestQuarantineManager.cpp`:

1. **Test File Quarantine**: Encrypt and move file to quarantine
2. **Test File Restoration**: Decrypt and restore to original location
3. **Test File Deletion**: Permanently remove quarantined file
4. **Test Cleanup Expired**: Remove files past retention period
5. **Test Statistics**: Track quarantine operations
6. **Test Duplicate Detection**: Prevent duplicate quarantines
7. **Test Encryption/Decryption**: Verify AES-256 correctness

Run tests:
```bash
./Build/release/bin/TestQuarantineManager
```

## Future Enhancements

### UI Integration (QuarantineUI.h)

Define IPC messages for UI process:
- `QuarantineFileNotification`: Alert user of quarantined file
- `ListQuarantinedFilesRequest/Response`: UI file browser
- `RestoreFileRequest`: User restores false positive
- `DeleteFileRequest`: User confirms permanent deletion

### RequestServer Integration

Add to `Services/RequestServer/ConnectionFromClient.cpp`:
```cpp
void ConnectionFromClient::did_finish_request(/* ... */)
{
    // Existing download complete logic...

    // NEW: Sandbox analysis
    if (should_scan_download(mime_type)) {
        auto analysis = perform_sandbox_analysis(file_data, filename);

        if (analysis.is_malicious()) {
            quarantine_and_notify(file_path, analysis);
            return; // Don't deliver file to user
        }
    }

    // Normal file delivery...
}
```

## Configuration

Environment variables:
- `QUARANTINE_DIR`: Override default quarantine directory
- `QUARANTINE_RETENTION_DAYS`: Default retention period (default: 30)

Config file: `~/.config/ladybird/sentinel.conf`
```ini
[quarantine]
directory = ~/.local/share/ladybird/quarantine
retention_days = 30
auto_cleanup_on_startup = true
```

## Troubleshooting

### "Invalid encryption key size"
- Encryption key file may be corrupted
- Delete `~/.local/share/ladybird/quarantine/encryption.key` to regenerate

### "Quarantine directory permissions error"
- Ensure directory has 0700 permissions
- `chmod 700 ~/.local/share/ladybird/quarantine`

### "File already quarantined"
- Duplicate file detected by hash
- Check with `is_file_quarantined(sha256_hash)`

### "Failed to decrypt file"
- Encryption key may have changed
- File may be corrupted
- Check key permissions (should be 0600)

## References

- Database Schema: `Services/Sentinel/DatabaseMigrations.cpp` (migrate_v7_to_v8)
- PolicyGraph Integration: `Services/Sentinel/PolicyGraph.cpp` (quarantine methods)
- Orchestrator Design: `Services/Sentinel/Sandbox/Orchestrator.h`
- AES-256 Implementation: `LibCrypto/Cipher/AES.h`
