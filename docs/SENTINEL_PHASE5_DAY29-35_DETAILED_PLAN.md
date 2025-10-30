# Sentinel Phase 5: Detailed Implementation Plan (Days 29-35)

**Status**: Day 29 Complete, Days 30-35 Ready to Execute
**Based On**: Technical Debt Analysis Report (SENTINEL_PHASE1-4_TECHNICAL_DEBT.md)
**Tracking**: Known Issues Document (SENTINEL_DAY29_KNOWN_ISSUES.md)
**Goal**: Address critical technical debt before Phase 6 (Milestone 0.2)

---

## Quick Reference

**Issue Tracking**: See `docs/SENTINEL_DAY29_KNOWN_ISSUES.md` for comprehensive tracking
**Quick Summary**: See `docs/SENTINEL_ISSUE_SUMMARY.txt` for 1-page overview

**Cross-Reference Note**: All tasks below include issue IDs (e.g., ISSUE-006) that link to the known issues document for full context, attack scenarios, and implementation details.

---

## Overview

This plan addresses the **100+ technical debt items** identified across phases 1-4, prioritizing:
1. **Security vulnerabilities** (18 items, 3 critical)
2. **Test coverage gaps** (0% coverage for core components)
3. **Error handling deficiencies** (50+ gaps)
4. **Performance bottlenecks** (23 optimization opportunities)
5. **Configuration inflexibility** (57+ hardcoded values)

---

## Day 29: Critical Security Fixes + Integration Testing

### Morning Session (4 hours): Critical Security Vulnerabilities

**STATUS**: ✅ COMPLETE - All 4 tasks resolved
**See**: `docs/SENTINEL_DAY29_MORNING_COMPLETE.md` for full details

#### Task 1: Fix SQL Injection in PolicyGraph (CRITICAL) - ✅ COMPLETE
**Issue ID**: ISSUE-001 (See SENTINEL_DAY29_KNOWN_ISSUES.md)
**Time**: 1 hour
**File**: `Services/Sentinel/PolicyGraph.cpp:207-212`
**Status**: ✅ RESOLVED

**Current Code**:
```cpp
statements.match_by_url_pattern = TRY(database->prepare_statement(R"#(
    SELECT * FROM policies
    WHERE url_pattern != ''
      AND ? LIKE url_pattern
      AND (expires_at = -1 OR expires_at > ?)
    LIMIT 1;
)#"sv));
```

**Changes Required**:
1. Add URL pattern validation on policy creation:
```cpp
static bool is_safe_url_pattern(StringView pattern) {
    // Allow only: alphanumeric, /, -, _, ., *, %
    for (auto ch : pattern) {
        if (!isalnum(ch) && ch != '/' && ch != '-' && ch != '_' &&
            ch != '.' && ch != '*' && ch != '%')
            return false;
    }
    // Limit length
    if (pattern.length() > 2048)
        return false;
    return true;
}
```

2. Update `create_policy()` to validate:
```cpp
if (!is_safe_url_pattern(policy.url_pattern))
    return Error::from_string_literal("Invalid URL pattern: contains unsafe characters");
```

3. Add ESCAPE clause to query:
```cpp
AND ? LIKE url_pattern ESCAPE '\\'
```

**Testing**:
- Add test with malicious pattern: `%%%%%%%`
- Add test with path traversal: `../../etc/passwd`
- Verify legitimate patterns still work

---

#### Task 2: Fix Arbitrary File Read in SentinelServer (CRITICAL) - ✅ COMPLETE
**Issue ID**: ISSUE-002 (See SENTINEL_DAY29_KNOWN_ISSUES.md)
**Time**: 1.5 hours
**File**: `Services/Sentinel/SentinelServer.cpp:228-234`
**Status**: ✅ RESOLVED

**Current Code**:
```cpp
ErrorOr<ByteString> SentinelServer::scan_file(ByteString const& file_path)
{
    auto file = TRY(Core::File::open(file_path, Core::File::OpenMode::Read));
    auto content = TRY(file->read_until_eof());
    return scan_content(content.bytes());
}
```

**Changes Required**:
1. Add file path validation:
```cpp
static ErrorOr<String> validate_scan_path(StringView file_path) {
    // Resolve canonical path
    auto canonical = TRY(FileSystem::real_path(file_path));

    // Check for directory traversal
    if (canonical.contains(".."sv))
        return Error::from_string_literal("Path traversal detected");

    // Whitelist allowed directories
    Vector<StringView> allowed_prefixes = {
        "/home"sv,           // User home directories
        "/tmp"sv,            // Temp downloads
        "/var/tmp"sv,        // Temp downloads
    };

    bool allowed = false;
    for (auto& prefix : allowed_prefixes) {
        if (canonical.starts_with(prefix)) {
            allowed = true;
            break;
        }
    }

    if (!allowed)
        return Error::from_string_literal("File path not in allowed directory");

    // Check file is not a symlink
    auto stat_result = TRY(Core::System::lstat(canonical));
    if (S_ISLNK(stat_result.st_mode))
        return Error::from_string_literal("Cannot scan symlinks");

    // Check file is regular file
    if (!S_ISREG(stat_result.st_mode))
        return Error::from_string_literal("Can only scan regular files");

    return canonical;
}
```

2. Update `scan_file()`:
```cpp
ErrorOr<ByteString> SentinelServer::scan_file(ByteString const& file_path)
{
    auto validated_path = TRY(validate_scan_path(file_path));

    // Check file size before reading
    auto stat_result = TRY(Core::System::stat(validated_path));
    constexpr size_t MAX_FILE_SIZE = 200 * 1024 * 1024; // 200MB
    if (static_cast<size_t>(stat_result.st_size) > MAX_FILE_SIZE)
        return Error::from_string_literal("File too large to scan");

    auto file = TRY(Core::File::open(validated_path, Core::File::OpenMode::Read));
    auto content = TRY(file->read_until_eof());
    return scan_content(content.bytes());
}
```

**Testing**:
- Test with path traversal: `/tmp/../etc/passwd`
- Test with symlink attack
- Test with device file: `/dev/zero`
- Test with directory instead of file
- Verify legitimate quarantine files work

---

#### Task 3: Fix Path Traversal in Quarantine restore_file (CRITICAL)
**Time**: 1 hour
**File**: `Services/RequestServer/Quarantine.cpp:331-415`

**Current Code**:
```cpp
StringBuilder dest_path_builder;
dest_path_builder.append(destination_dir);
dest_path_builder.append('/');
dest_path_builder.append(metadata.filename);
```

**Changes Required**:
1. Add destination directory validation:
```cpp
static ErrorOr<String> validate_restore_destination(StringView dest_dir) {
    // Resolve canonical path
    auto canonical = TRY(FileSystem::real_path(dest_dir));

    // Must be absolute path
    if (!canonical.starts_with("/"sv))
        return Error::from_string_literal("Destination must be absolute path");

    // Check directory exists and is writable
    if (!FileSystem::is_directory(canonical))
        return Error::from_string_literal("Destination is not a directory");

    auto access_result = Core::System::access(canonical, W_OK);
    if (access_result.is_error())
        return Error::from_string_literal("Destination directory is not writable");

    return canonical;
}
```

2. Add filename sanitization:
```cpp
static String sanitize_filename(StringView filename) {
    // Use only basename (remove any path components)
    auto basename = filename;
    if (auto last_slash = filename.find_last('/'); last_slash.has_value())
        basename = filename.substring_view(last_slash.value() + 1);

    // Remove dangerous characters
    StringBuilder sanitized;
    for (auto ch : basename) {
        if (ch == '/' || ch == '\\' || ch == '\0' || ch < 32)
            continue;  // Skip dangerous chars
        sanitized.append(ch);
    }

    auto result = sanitized.to_string();
    if (result.is_error() || result.value().is_empty())
        return "quarantined_file"_string;

    return result.release_value();
}
```

3. Update `restore_file()`:
```cpp
auto validated_dest = TRY(validate_restore_destination(destination_dir));
auto safe_filename = sanitize_filename(metadata.filename);

StringBuilder dest_path_builder;
dest_path_builder.append(validated_dest);
dest_path_builder.append('/');
dest_path_builder.append(safe_filename);
```

**Testing**:
- Test with path traversal in dest_dir: `/tmp/../../../../etc/`
- Test with path traversal in filename: `../../.ssh/authorized_keys`
- Test with null bytes in filename
- Verify normal restore works

---

#### Task 4: Add Quarantine ID Validation (HIGH)
**Time**: 30 minutes
**File**: `Services/RequestServer/Quarantine.cpp` (all functions using quarantine_id)

**Changes Required**:
1. Add validation function:
```cpp
static bool is_valid_quarantine_id(StringView id) {
    // Expected format: YYYYMMDD_HHMMSS_XXXXXX (6 hex chars)
    // Example: 20251030_143052_a3f5c2

    if (id.length() != 21)
        return false;

    // Check format: 8 digits, underscore, 6 digits, underscore, 6 hex
    for (size_t i = 0; i < id.length(); ++i) {
        char ch = id[i];
        if (i < 8 || (i >= 9 && i < 15)) {
            // Date and time portions: must be digits
            if (!isdigit(ch))
                return false;
        } else if (i == 8 || i == 15) {
            // Separators: must be underscore
            if (ch != '_')
                return false;
        } else {
            // Random portion: must be hex
            if (!isxdigit(ch))
                return false;
        }
    }

    return true;
}
```

2. Add validation to all functions:
```cpp
ErrorOr<QuarantineMetadata> Quarantine::read_metadata(String const& quarantine_id)
{
    if (!is_valid_quarantine_id(quarantine_id))
        return Error::from_string_literal("Invalid quarantine ID format");

    // ... rest of function
}
```

3. Apply to: `read_metadata()`, `restore_file()`, `delete_file()`

**Testing**:
- Test with path traversal: `../../../etc/passwd`
- Test with invalid format: `malicious_id`
- Test with valid format: `20251030_143052_a3f5c2`

---

### Afternoon Session (4 hours): Integration Testing

#### Task 5: Write Integration Tests for Download Vetting Flow
**Time**: 2 hours
**File**: `Tests/Sentinel/TestDownloadVettingE2E.cpp` (new file)

**Test Cases**:
```cpp
TEST_CASE(test_eicar_detection_end_to_end)
{
    // 1. Initialize Sentinel with EICAR rule
    // 2. Start RequestServer with SecurityTap
    // 3. Simulate download of EICAR test file
    // 4. Verify SecurityTap intercepts
    // 5. Verify YARA detection triggers
    // 6. Verify alert generated
    // 7. Verify file NOT saved (blocked)
}

TEST_CASE(test_clean_file_download)
{
    // 1. Setup same as above
    // 2. Download clean file
    // 3. Verify no alert generated
    // 4. Verify file saved successfully
    // 5. Verify no performance degradation
}

TEST_CASE(test_policy_creation_and_enforcement)
{
    // 1. Download malicious file, create policy to block
    // 2. Attempt to download same file again
    // 3. Verify policy auto-enforces (no scan needed)
    // 4. Verify cache hit recorded
}

TEST_CASE(test_quarantine_workflow)
{
    // 1. Download malicious file, choose quarantine
    // 2. Verify file moved to quarantine directory
    // 3. Verify metadata JSON created
    // 4. List quarantine entries
    // 5. Restore file to temp directory
    // 6. Verify file restored with correct permissions
    // 7. Delete from quarantine
    // 8. Verify cleanup
}
```

**Implementation**: ~200 lines of test code with setup/teardown

---

#### Task 6: Run ASAN/UBSAN Memory Safety Tests
**Time**: 1 hour

**Commands**:
```bash
# Build with sanitizers
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Run all Sentinel tests
cd Build/sanitizers
ctest -R Sentinel -V

# Check for leaks
ASAN_OPTIONS=detect_leaks=1 ./bin/TestPolicyGraph
ASAN_OPTIONS=detect_leaks=1 ./bin/TestDownloadVetting

# Check for undefined behavior
UBSAN_OPTIONS=print_stacktrace=1 ./bin/TestPolicyGraph
```

**Expected Issues**:
- Memory leaks in client handling (already identified)
- Potential use-after-free in async operations
- Uninitialized memory in cache

**Action**: Fix any issues discovered before proceeding

---

#### Task 7: Document Known Issues and Workarounds
**Time**: 1 hour
**File**: `docs/SENTINEL_KNOWN_ISSUES.md` (new file)

**Content**:
```markdown
# Sentinel Known Issues

## Limitations

### Large File Handling
- Files >100MB skip YARA scanning (fail-open)
- **Workaround**: Configure max_scan_size_bytes higher if needed
- **Risk**: Performance degradation

### Platform Support
- macOS Mach IPC not implemented (6 TODO items)
- **Workaround**: Use Unix domain sockets (works on all platforms)

### Database
- No automatic backup system
- **Workaround**: Manually backup ~/.local/share/Ladybird/PolicyGraph

## Performance Considerations

### Cache Size
- Default 1000 entries may be insufficient for heavy users
- **Recommendation**: Increase to 5000+ in SentinelConfig

### Quarantine Directory
- No size limit enforcement
- **Recommendation**: Monitor disk usage periodically
```

---

## Day 30: Error Handling + Test Coverage

### Morning Session (4 hours): Critical Error Handling Fixes

#### Task 1: Fix File Orphaning in Quarantine (CRITICAL)
**Issue ID**: ISSUE-006 (See SENTINEL_DAY29_KNOWN_ISSUES.md for full details)
**Time**: 1.5 hours
**File**: `Services/RequestServer/Quarantine.cpp:86-156`

**Current Problem**: If metadata write fails after file move, file is orphaned

**Solution**: Use move-last transaction pattern

**Changes Required**:
```cpp
ErrorOr<String> Quarantine::quarantine_file(
    String const& source_path,
    QuarantineMetadata const& metadata)
{
    auto quarantine_id = TRY(generate_quarantine_id());

    // 1. Write metadata FIRST (to temp location)
    auto temp_metadata_path = ByteString::formatted("{}/temp_{}.json",
        quarantine_dir, quarantine_id);
    auto metadata_result = write_metadata_to_path(temp_metadata_path, metadata);
    if (metadata_result.is_error()) {
        // Clean up temp file
        FileSystem::remove(temp_metadata_path, FileSystem::RecursionMode::Disallowed);
        return metadata_result.release_error();
    }

    // 2. Move file to quarantine
    auto dest_path = ByteString::formatted("{}/{}", quarantine_dir, quarantine_id);
    auto move_result = FileSystem::move_file(dest_path, source_path,
        FileSystem::PreserveMode::PermissionsOnly);
    if (move_result.is_error()) {
        // Clean up metadata on failure
        FileSystem::remove(temp_metadata_path, FileSystem::RecursionMode::Disallowed);
        return move_result.release_error();
    }

    // 3. Set permissions on quarantined file
    auto chmod_result = Core::System::chmod(dest_path, 0400);
    if (chmod_result.is_error()) {
        dbgln("Quarantine: Warning - failed to set permissions: {}", chmod_result.error());
        // Continue anyway - file is quarantined
    }

    // 4. Rename metadata to final location (atomic on most filesystems)
    auto final_metadata_path = ByteString::formatted("{}/{}.json",
        quarantine_dir, quarantine_id);
    auto rename_result = FileSystem::move_file(final_metadata_path, temp_metadata_path,
        FileSystem::PreserveMode::PermissionsOnly);
    if (rename_result.is_error()) {
        // File is quarantined but metadata missing - recoverable
        dbgln("Quarantine: Warning - metadata rename failed: {}", rename_result.error());
        // TODO: Add to recovery list
    }

    return String::from_byte_string(quarantine_id);
}
```

**Testing**:
- Simulate disk full during metadata write
- Simulate permission denied during file move
- Verify no orphaned files remain
- Verify rollback works correctly

---

#### Task 2: Replace MUST() with TRY() in SentinelServer (CRITICAL)
**Issue ID**: ISSUE-007 (See SENTINEL_DAY29_KNOWN_ISSUES.md for full details)
**Time**: 45 minutes
**File**: `Services/Sentinel/SentinelServer.cpp:148`

**Current Code**:
```cpp
String message = MUST(String::from_utf8(...));
```

**Changes Required**:
```cpp
auto message_result = String::from_utf8(StringView(
    reinterpret_cast<char const*>(bytes_read.data()),
    bytes_read.size()));

if (message_result.is_error()) {
    dbgln("Sentinel: Invalid UTF-8 in client message");

    // Send error response
    JsonObject error_response;
    error_response.set("status"sv, "error"sv);
    error_response.set("error"sv, "Invalid UTF-8 encoding in message"sv);

    StringBuilder response_builder;
    error_response.serialize(response_builder);
    auto response_str = MUST(response_builder.to_string());

    socket.write_until_depleted(response_str.bytes());
    return;
}

auto message = message_result.release_value();
```

**Testing**:
- Send message with invalid UTF-8 bytes
- Verify server doesn't crash
- Verify error response sent to client

---

#### Task 3: Fix Partial IPC Response Reading (CRITICAL)
**Issue ID**: ISSUE-008 (See SENTINEL_DAY29_KNOWN_ISSUES.md for full details)
**Time**: 1 hour
**File**: `Services/RequestServer/SecurityTap.cpp:223-230`

**Current Code**:
```cpp
auto bytes_read_result = m_sentinel_socket->read_some(response_buffer);
// ...
return ByteString(bytes_read);
```

**Changes Required**:
```cpp
ErrorOr<ByteString> SecurityTap::read_json_response()
{
    // Read until newline delimiter
    Vector<u8> accumulated_data;

    while (true) {
        Array<u8, 1024> chunk_buffer;
        auto bytes_read_result = m_sentinel_socket->read_some(chunk_buffer);

        if (bytes_read_result.is_error())
            return bytes_read_result.release_error();

        auto bytes_read = bytes_read_result.value();
        if (bytes_read.is_empty()) {
            m_connection_failed = true;
            return Error::from_string_literal("Sentinel socket closed");
        }

        // Append to accumulated data
        for (auto byte : bytes_read) {
            accumulated_data.append(byte);
            if (byte == '\n')
                goto found_newline;
        }

        // Prevent infinite growth
        if (accumulated_data.size() > 1024 * 1024) { // 1MB limit
            return Error::from_string_literal("Response too large");
        }
    }

found_newline:
    return ByteString(reinterpret_cast<char const*>(accumulated_data.data()),
        accumulated_data.size());
}
```

**Testing**:
- Simulate fragmented response across multiple reads
- Test with response exactly at buffer boundary
- Test with very large response (error case)

---

#### Task 4: Fix PolicyGraph Error Propagation (HIGH)
**Time**: 45 minutes
**File**: `Services/Sentinel/PolicyGraph.cpp:285-313`

**Changes Required**:
1. Check database operations:
```cpp
auto exec_result = m_database->execute_statement(
    m_statements.create_policy,
    {},
    policy.rule_name,
    // ... other parameters
);

if (exec_result.is_error()) {
    dbgln("PolicyGraph: Failed to create policy: {}", exec_result.error());
    return Error::from_string_literal("Failed to insert policy into database");
}
```

2. Verify last_insert_id:
```cpp
i64 last_id = 0;
bool callback_invoked = false;

m_database->execute_statement(
    m_statements.get_last_insert_id,
    [&](auto statement_id) {
        last_id = m_database->result_column<i64>(statement_id, 0);
        callback_invoked = true;
    }
);

if (!callback_invoked || last_id == 0) {
    return Error::from_string_literal("Failed to retrieve policy ID");
}
```

**Testing**:
- Test with database locked
- Test with disk full
- Verify error messages are helpful

---

### Afternoon Session (4 hours): Test Coverage Priority 1

#### Task 5: SecurityTap YARA Integration Tests (CRITICAL)
**Issue ID**: COVERAGE-001 (See SENTINEL_DAY29_KNOWN_ISSUES.md for full details)
**Time**: 2.5 hours
**File**: `Tests/Sentinel/TestSecurityTapYARA.cpp` (new file)

**Test Cases**:
```cpp
// Test 1: EICAR Detection
TEST_CASE(test_eicar_detection)
{
    // Use actual EICAR test string
    auto eicar = "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*"sv;

    auto tap = SecurityTap::create();
    EXPECT(!tap.is_error());

    auto result = tap.value()->inspect_download(eicar.bytes(), "test.txt"sv, "text/plain"sv, "https://example.com"sv);

    EXPECT(!result.is_error());
    EXPECT(result.value().is_threat);
}

// Test 2: Clean File
TEST_CASE(test_clean_file_detection)
{
    auto clean_data = "This is a clean file with no malware"sv;

    auto tap = SecurityTap::create();
    auto result = tap.value()->inspect_download(clean_data.bytes(), "clean.txt"sv, "text/plain"sv, "https://example.com"sv);

    EXPECT(!result.is_error());
    EXPECT(!result.value().is_threat);
}

// Test 3: Large File Handling
TEST_CASE(test_large_file_skip)
{
    // 101MB file (over limit)
    auto large_data = ByteBuffer::create_zeroed(101 * 1024 * 1024);

    auto tap = SecurityTap::create();
    auto result = tap.value()->inspect_download(large_data.value().bytes(), "large.bin"sv, "application/octet-stream"sv, "https://example.com"sv);

    // Should skip scan and allow (fail-open)
    EXPECT(!result.is_error());
    EXPECT(!result.value().is_threat);
}

// Test 4: SHA256 Hash Accuracy
TEST_CASE(test_sha256_computation)
{
    auto test_data = "test data"sv;
    auto expected_hash = "916f0027a575074ce72a331777c3478d6513f786a591bd892da1a577bf2335f9"sv;

    auto tap = SecurityTap::create();
    auto hash = tap.value()->compute_sha256(test_data.bytes());

    EXPECT(!hash.is_error());
    EXPECT_EQ(hash.value(), expected_hash);
}

// Test 5: Sentinel Connection Failure
TEST_CASE(test_sentinel_unavailable)
{
    // Stop Sentinel service
    system("pkill -9 Sentinel");

    auto tap = SecurityTap::create();
    auto result = tap.value()->inspect_download("test"sv.bytes(), "test.txt"sv, "text/plain"sv, "https://example.com"sv);

    // Should fail-open and allow
    EXPECT(!result.is_error());
    EXPECT(!result.value().is_threat);
}

// Test 6: Async Inspection
TEST_CASE(test_async_inspection)
{
    auto tap = SecurityTap::create();

    bool callback_invoked = false;
    ScanResult async_result;

    tap.value()->async_inspect_download(
        "test data"sv.bytes(),
        "test.txt"sv,
        "text/plain"sv,
        "https://example.com"sv,
        [&](auto result) {
            callback_invoked = true;
            async_result = result;
        }
    );

    // Wait for completion (up to 5 seconds)
    for (int i = 0; i < 50 && !callback_invoked; ++i) {
        usleep(100000); // 100ms
    }

    EXPECT(callback_invoked);
    EXPECT(!async_result.is_threat);
}
```

**Implementation**: ~300 lines

---

#### Task 6: Quarantine Operations Tests (CRITICAL)
**Issue ID**: COVERAGE-002 (See SENTINEL_DAY29_KNOWN_ISSUES.md for full details)
**Time**: 1.5 hours
**File**: `Tests/Sentinel/TestQuarantineOperations.cpp` (new file)

**Test Cases**:
```cpp
TEST_CASE(test_quarantine_and_restore)
{
    // Create test file
    auto test_file = "/tmp/test_malware.txt";
    auto test_data = "malicious content"sv;
    {
        auto file = Core::File::open(test_file, Core::File::OpenMode::Write);
        file.value()->write_until_depleted(test_data.bytes());
    }

    // Initialize quarantine
    auto quarantine = Quarantine::initialize();
    EXPECT(!quarantine.is_error());

    // Quarantine file
    QuarantineMetadata metadata;
    metadata.filename = "test_malware.txt"_string;
    metadata.origin_url = "https://malicious.com"_string;
    metadata.threat_type = "TestThreat"_string;
    metadata.sha256 = "abc123"_string;
    metadata.file_size = test_data.length();

    auto qid = quarantine.value()->quarantine_file(test_file, metadata);
    EXPECT(!qid.is_error());

    // Verify original file gone
    EXPECT(!FileSystem::exists(test_file));

    // Verify quarantine file exists
    auto q_path = ByteString::formatted("{}/.local/share/Ladybird/Quarantine/{}",
        Core::System::getenv("HOME"sv).value(), qid.value());
    EXPECT(FileSystem::exists(q_path));

    // Verify permissions (r--------)
    auto stat_result = Core::System::stat(q_path);
    EXPECT_EQ(stat_result.value().st_mode & 0777, 0400);

    // Restore file
    auto restore_result = quarantine.value()->restore_file(qid.value(), "/tmp"_string);
    EXPECT(!restore_result.is_error());

    // Verify restored
    EXPECT(FileSystem::exists(test_file));

    // Cleanup
    FileSystem::remove(test_file, FileSystem::RecursionMode::Disallowed);
}

TEST_CASE(test_list_quarantine_entries)
{
    // ... test listing
}

TEST_CASE(test_delete_quarantined_file)
{
    // ... test deletion
}

TEST_CASE(test_concurrent_quarantine)
{
    // ... test concurrent operations
}
```

**Implementation**: ~400 lines

---

## Day 31: Performance Optimization + Error Handling

### Morning Session (4 hours): High-Impact Performance Fixes

#### Task 1: Fix LRU Cache O(n) Operation (CRITICAL)
**Issue ID**: ISSUE-014 (See SENTINEL_DAY29_KNOWN_ISSUES.md for full details)
**Time**: 2 hours
**File**: `Services/Sentinel/PolicyGraph.cpp:77-84`

**Current Implementation**:
```cpp
void PolicyGraphCache::update_lru(String const& key)
{
    m_lru_order.remove_all_matching([&](auto const& k) { return k == key; });
    m_lru_order.append(key);
}
```

**New Implementation** - Use HashMap + intrusive list:
```cpp
// PolicyGraph.h
struct CacheNode {
    String key;
    Policy policy;
    CacheNode* prev { nullptr };
    CacheNode* next { nullptr };
};

class PolicyGraphCache {
    HashMap<String, OwnPtr<CacheNode>> m_cache_map;
    CacheNode* m_lru_head { nullptr };
    CacheNode* m_lru_tail { nullptr };
    size_t m_max_size;

    void move_to_front(CacheNode* node) {
        if (node == m_lru_head)
            return;

        // Remove from current position
        if (node->prev)
            node->prev->next = node->next;
        if (node->next)
            node->next->prev = node->prev;
        if (node == m_lru_tail)
            m_lru_tail = node->prev;

        // Add to front
        node->next = m_lru_head;
        node->prev = nullptr;
        if (m_lru_head)
            m_lru_head->prev = node;
        m_lru_head = node;
        if (!m_lru_tail)
            m_lru_tail = node;
    }

    void evict_lru() {
        if (!m_lru_tail)
            return;

        auto* to_evict = m_lru_tail;
        m_lru_tail = to_evict->prev;
        if (m_lru_tail)
            m_lru_tail->next = nullptr;
        else
            m_lru_head = nullptr;

        m_cache_map.remove(to_evict->key);
        m_cache_evictions++;
    }

public:
    Optional<Policy> get(String const& key) {
        auto it = m_cache_map.find(key);
        if (it == m_cache_map.end()) {
            m_cache_misses++;
            return {};
        }

        m_cache_hits++;
        move_to_front(it->value.get());
        return it->value->policy;
    }

    void put(String const& key, Policy const& policy) {
        // Check if already exists
        auto it = m_cache_map.find(key);
        if (it != m_cache_map.end()) {
            it->value->policy = policy;
            move_to_front(it->value.get());
            return;
        }

        // Evict if at capacity
        if (m_cache_map.size() >= m_max_size)
            evict_lru();

        // Insert new node
        auto node = make<CacheNode>();
        node->key = key;
        node->policy = policy;

        auto* node_ptr = node.get();
        m_cache_map.set(key, move(node));

        // Add to front of LRU list
        node_ptr->next = m_lru_head;
        node_ptr->prev = nullptr;
        if (m_lru_head)
            m_lru_head->prev = node_ptr;
        m_lru_head = node_ptr;
        if (!m_lru_tail)
            m_lru_tail = node_ptr;
    }
};
```

**Benchmark**: Before/after with 1000 cache entries, 10k lookups

---

#### Task 2: Enable Async Scanning by Default (HIGH)
**Time**: 1 hour
**File**: `Services/RequestServer/SecurityTap.cpp`

**Changes Required**:
```cpp
// Add size threshold
constexpr size_t SYNC_SCAN_THRESHOLD = 1 * 1024 * 1024; // 1MB

ScanResult SecurityTap::inspect_download_smart(
    ReadonlyBytes content,
    StringView filename,
    StringView mime_type,
    StringView url,
    Function<void(ScanResult)> callback)
{
    // Small files: scan synchronously
    if (content.size() < SYNC_SCAN_THRESHOLD) {
        return inspect_download(content, filename, mime_type, url);
    }

    // Large files: scan asynchronously
    async_inspect_download(content, filename, mime_type, url, move(callback));

    // Return "scanning" result immediately
    return ScanResult {
        .is_threat = false,
        .scanning_in_progress = true
    };
}
```

**Update ConnectionFromClient** to use smart scan

---

#### Task 3: Add Database Indexes (MEDIUM)
**Time**: 30 minutes
**File**: `Services/Sentinel/PolicyGraph.cpp:100-162`

**Changes Required**:
```sql
-- Add to initialize_database()
CREATE INDEX IF NOT EXISTS idx_policies_active
    ON policies(expires_at, file_hash);

CREATE INDEX IF NOT EXISTS idx_threats_recent
    ON threat_history(detected_at DESC, action_taken);

CREATE INDEX IF NOT EXISTS idx_policies_match_type
    ON policies(match_type, expires_at);

-- Enable WAL mode for concurrent reads
PRAGMA journal_mode=WAL;
```

**Benchmark**: Query time before/after with 10k policies

---

#### Task 4: Extract Duplicate Parsing Code (MEDIUM)
**Time**: 30 minutes
**File**: `Services/Sentinel/PolicyGraph.cpp:515-677`

**Changes Required**:
```cpp
static Policy parse_policy_from_statement(SQL::Database& database, int statement_id)
{
    Policy policy;
    int col = 0;

    policy.id = database.result_column<i64>(statement_id, col++);
    policy.rule_name = database.result_column<String>(statement_id, col++);
    policy.url_pattern = database.result_column<String>(statement_id, col++);
    policy.file_hash = database.result_column<String>(statement_id, col++);
    policy.mime_type = database.result_column<String>(statement_id, col++);
    policy.action = static_cast<PolicyAction>(database.result_column<int>(statement_id, col++));
    policy.match_type = static_cast<PolicyMatchType>(database.result_column<int>(statement_id, col++));
    policy.enforcement_action = database.result_column<String>(statement_id, col++);
    policy.created_at = UnixDateTime::from_unix_time_parts(database.result_column<i64>(statement_id, col++), 0);
    policy.expires_at = database.result_column<i64>(statement_id, col++);
    policy.last_triggered_at = UnixDateTime::from_unix_time_parts(database.result_column<i64>(statement_id, col++), 0);
    policy.hit_count = database.result_column<int>(statement_id, col++);
    policy.created_by = database.result_column<String>(statement_id, col++);

    return policy;
}
```

Use in all three match functions - reduces 450 lines to ~50 lines + helper

---

### Afternoon Session (4 hours): More Error Handling + Testing

#### Task 5: Add Large File Scan Warnings (HIGH)
**Time**: 1 hour
**File**: `Services/RequestServer/SecurityTap.cpp:100-105`

**Changes Required**:
```cpp
if (content.size() > MAX_SCAN_SIZE) {
    dbgln("SecurityTap: File too large to scan ({}MB)", content.size() / (1024 * 1024));

    // Create alert for user notification
    JsonObject alert;
    alert.set("type"sv, "file_too_large"sv);
    alert.set("filename"sv, filename);
    alert.set("size_mb"sv, content.size() / (1024 * 1024));
    alert.set("max_size_mb"sv, MAX_SCAN_SIZE / (1024 * 1024));
    alert.set("recommendation"sv, "Review file manually before opening"sv);

    return ScanResult {
        .is_threat = false,
        .alert_json = MUST(alert.to_string()),
        .scan_skipped = true
    };
}
```

**Update UI** to show warning notification

---

#### Task 6: Add IPC Message Tests (HIGH)
**Time**: 2 hours
**File**: `Tests/Sentinel/TestIPCMessages.cpp` (new file)

**Test Cases**:
```cpp
TEST_CASE(test_credential_exfil_alert_message)
TEST_CASE(test_form_submission_detected_message)
TEST_CASE(test_quarantine_operations_messages)
TEST_CASE(test_malformed_message_handling)
TEST_CASE(test_large_payload_handling)
```

---

#### Task 7: Performance Benchmarking (REQUIRED)
**Time**: 1 hour
**File**: `Tools/benchmark_sentinel_phase5.cpp` (new file)

**Benchmark Tests**:
```cpp
1. Policy lookup throughput (1000 policies, 10k lookups)
2. Download scan latency (1MB, 10MB, 100MB files)
3. Cache hit rate over 24hr simulation
4. Database growth impact (100, 1k, 10k policies)
5. UI responsiveness (frame drops during downloads)
```

**Generate report**: Save results to `docs/SENTINEL_PHASE5_PERFORMANCE.md`

---

## Day 33: Error Handling and Resilience

### Morning Session (4 hours): Graceful Degradation

#### Task 1: Implement Sentinel Daemon Failure Handling (CRITICAL)
**Time**: 1.5 hours
**File**: `Services/RequestServer/SecurityTap.cpp:108-116`

**Current Problem**: When Sentinel daemon crashes or is not running, SecurityTap logs error and fails-open silently. Users are unaware that download protection is disabled.

**Solution**: Add connection health monitoring, retry logic, and user notifications for daemon unavailability.

**Changes Required**:

1. Add connection health tracking in SecurityTap.h:
```cpp
// Add to SecurityTap class
class SecurityTap {
    // ... existing members

    bool m_sentinel_available { true };
    UnixDateTime m_last_connection_attempt;
    size_t m_consecutive_failures { 0 };
    static constexpr size_t MAX_RETRY_ATTEMPTS = 3;
    static constexpr Duration RETRY_BACKOFF = Duration::from_seconds(5);

    ErrorOr<void> check_sentinel_health();
    void record_connection_failure();
    bool should_retry_connection();
};
```

2. Update SecurityTap.cpp to track failures:
```cpp
void SecurityTap::record_connection_failure()
{
    m_consecutive_failures++;
    m_last_connection_attempt = UnixDateTime::now();

    if (m_consecutive_failures == 1) {
        // First failure - log and continue
        dbgln("SecurityTap: Sentinel connection failed (attempt 1/{})", MAX_RETRY_ATTEMPTS);
    } else if (m_consecutive_failures >= MAX_RETRY_ATTEMPTS) {
        // Multiple failures - mark as unavailable
        m_sentinel_available = false;
        dbgln("SecurityTap: Sentinel marked unavailable after {} failures", m_consecutive_failures);
    }
}

bool SecurityTap::should_retry_connection()
{
    if (m_sentinel_available)
        return true;

    auto now = UnixDateTime::now();
    auto elapsed = now.to_seconds() - m_last_connection_attempt.to_seconds();

    // Retry every 5 seconds
    if (elapsed >= RETRY_BACKOFF.to_seconds()) {
        dbgln("SecurityTap: Retrying Sentinel connection after {} seconds", elapsed);
        m_consecutive_failures = 0;
        m_sentinel_available = true;
        return true;
    }

    return false;
}
```

3. Update inspect_download() to use health checks:
```cpp
ErrorOr<SecurityTap::ScanResult> SecurityTap::inspect_download(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Check file size limit
    constexpr size_t MAX_SCAN_SIZE = 100 * 1024 * 1024;
    if (content.size() > MAX_SCAN_SIZE) {
        dbgln("SecurityTap: File too large to scan ({}MB)", content.size() / (1024 * 1024));

        // Return special result indicating scan skipped
        JsonObject warning;
        warning.set("type"sv, "scan_skipped_size"sv);
        warning.set("reason"sv, "File exceeds maximum scan size"sv);
        warning.set("file_size_mb"sv, static_cast<u64>(content.size() / (1024 * 1024)));
        warning.set("max_size_mb"sv, static_cast<u64>(MAX_SCAN_SIZE / (1024 * 1024)));
        warning.set("recommendation"sv, "Review file contents manually before opening"sv);

        return ScanResult {
            .is_threat = false,
            .alert_json = MUST(warning.to_string()),
            .scan_skipped = true
        };
    }

    // Check Sentinel health before attempting scan
    if (!should_retry_connection()) {
        dbgln("SecurityTap: Sentinel unavailable, failing open");

        // Return result indicating protection disabled
        JsonObject warning;
        warning.set("type"sv, "protection_disabled"sv);
        warning.set("reason"sv, "Sentinel security service is unavailable"sv);
        warning.set("action"sv, "Download allowed without scanning"sv);
        warning.set("recommendation"sv, "Check that Sentinel daemon is running"sv);

        return ScanResult {
            .is_threat = false,
            .alert_json = MUST(warning.to_string()),
            .protection_disabled = true
        };
    }

    // Attempt scan
    auto response_json_result = send_scan_request(metadata, content);

    if (response_json_result.is_error()) {
        record_connection_failure();
        dbgln("SecurityTap: Sentinel scan failed: {}", response_json_result.error());

        // Generate user-friendly warning
        JsonObject warning;
        warning.set("type"sv, "scan_failed"sv);
        warning.set("reason"sv, "Could not communicate with security service"sv);
        warning.set("error"sv, response_json_result.error().string_literal());
        warning.set("action"sv, "Download allowed without scanning (fail-open)"sv);

        return ScanResult {
            .is_threat = false,
            .alert_json = MUST(warning.to_string()),
            .protection_disabled = true
        };
    }

    // Success - reset failure counter
    m_consecutive_failures = 0;

    // ... rest of existing parsing logic
}
```

4. Update ScanResult structure (SecurityTap.h):
```cpp
struct ScanResult {
    bool is_threat { false };
    Optional<String> alert_json;
    bool scan_skipped { false };
    bool protection_disabled { false };
};
```

**Testing**:
- Stop Sentinel daemon, attempt download
- Verify warning displayed to user
- Verify download proceeds (fail-open)
- Start Sentinel, verify recovery after 5 seconds
- Test with consecutive failures

---

#### Task 2: Add Database Failure Recovery (HIGH)
**Time**: 1 hour
**File**: `Services/Sentinel/PolicyGraph.cpp:285-313`

**Current Problem**: Database failures cause silent policy creation failures. No fallback mechanism exists.

**Solution**: Add error detection, retry logic, and in-memory fallback for critical operations.

**Changes Required**:

1. Add retry wrapper in PolicyGraph.cpp:
```cpp
template<typename Callback>
ErrorOr<void> PolicyGraph::execute_with_retry(
    SQL::StatementID statement_id,
    Callback callback,
    size_t max_attempts = 3)
{
    size_t attempt = 0;
    ErrorOr<void> last_error = Error::from_string_literal("No attempts made");

    while (attempt < max_attempts) {
        attempt++;

        try {
            m_database->execute_statement(statement_id, callback);
            return {}; // Success
        } catch (...) {
            last_error = Error::from_string_literal("Database operation failed");

            if (attempt < max_attempts) {
                // Brief delay before retry (10ms * attempt)
                usleep(10000 * attempt);
                dbgln("PolicyGraph: Retrying database operation (attempt {}/{})", attempt + 1, max_attempts);
            }
        }
    }

    return last_error;
}
```

2. Update create_policy() with proper error handling:
```cpp
i64 PolicyGraph::create_policy(Policy const& policy)
{
    // Convert enum to strings
    auto action_str = policy_action_to_string(policy.action);
    auto match_type_str = policy_match_type_to_string(policy.match_type);

    Optional<i64> expires_ms;
    if (policy.expires_at.has_value())
        expires_ms = policy.expires_at->milliseconds_since_epoch();

    // Execute insert with retry logic
    bool insert_succeeded = false;
    auto exec_result = execute_with_retry(
        m_statements.create_policy,
        [&](auto) { insert_succeeded = true; },
        3  // retry 3 times
    );

    if (exec_result.is_error() || !insert_succeeded) {
        dbgln("PolicyGraph: Failed to create policy after retries: {}",
            exec_result.is_error() ? exec_result.error().string_literal() : "Unknown error");

        // Check for disk full
        auto stat_result = Core::System::stat(m_database_path);
        if (stat_result.is_error()) {
            dbgln("PolicyGraph: Cannot access database file");
            return 0;  // Signal failure
        }

        // Check database file exists and is writable
        if (!FileSystem::is_writable(m_database_path)) {
            dbgln("PolicyGraph: Database file is not writable");
            return 0;
        }

        dbgln("PolicyGraph: Database operation failed - possible corruption");
        return 0;  // Signal failure
    }

    // Get last insert ID with verification
    i64 last_id = 0;
    bool callback_invoked = false;

    auto id_result = execute_with_retry(
        m_statements.get_last_insert_id,
        [&](auto statement_id) {
            last_id = m_database->result_column<i64>(statement_id, 0);
            callback_invoked = true;
        }
    );

    if (id_result.is_error() || !callback_invoked) {
        dbgln("PolicyGraph: Failed to retrieve last insert ID");
        return 0;  // Signal failure
    }

    if (last_id == 0) {
        dbgln("PolicyGraph: Invalid policy ID returned (0)");
        return 0;  // Signal failure
    }

    // Successfully created - invalidate cache
    m_cache.invalidate();

    dbgln("PolicyGraph: Created policy with ID: {}", last_id);
    return last_id;
}
```

3. Update callers to check for failure:
```cpp
// In ConnectionFromClient.cpp or wherever create_policy is called
auto policy_id = policy_graph->create_policy(policy);
if (policy_id == 0) {
    // Policy creation failed - show error to user
    dbgln("Failed to create policy: Database error");

    // Send error response via IPC
    async_policy_creation_failed("Database error: Unable to store policy. Check disk space and permissions.");
    return;
}

// Policy created successfully
async_policy_created(policy_id);
```

**Testing**:
- Simulate database locked (open in another process with exclusive lock)
- Simulate disk full (fill filesystem)
- Verify retry logic attempts 3 times
- Verify user-friendly error message shown
- Verify system remains functional

---

#### Task 3: Fix Quarantine String-Based Error Matching (MEDIUM)
**Time**: 45 minutes
**File**: `Services/RequestServer/Quarantine.cpp:118-128`

**Current Code**:
```cpp
auto error_str = move_result.error().string_literal();
if (ByteString(error_str).contains("No space left"sv)) {
    return Error::from_string_literal("Cannot quarantine file: Disk is full...");
} else if (ByteString(error_str).contains("Permission denied"sv)) {
    return Error::from_string_literal("Cannot quarantine file: Permission denied...");
}
```

**Problem**: String matching is brittle and locale-dependent.

**Solution**: Use errno codes instead of string matching.

**Changes Required**:

1. Update Quarantine.cpp error handling:
```cpp
ErrorOr<String> Quarantine::quarantine_file(
    String const& source_path,
    QuarantineMetadata const& metadata)
{
    // ... existing code up to move operation

    // Move file to quarantine directory
    dbgln("Quarantine: Moving {} to {}", source_path, dest_path);
    auto move_result = FileSystem::move_file(dest_path, source_path,
        FileSystem::PreserveMode::Nothing);

    if (move_result.is_error()) {
        auto error = move_result.error();
        dbgln("Quarantine: Failed to move file: {}", error);

        // Check errno for specific error types
        if (error.code() == ENOSPC) {
            return Error::from_string_literal(
                "Cannot quarantine file: Disk is full. "
                "Free up space in quarantine directory and try again.");
        } else if (error.code() == EACCES || error.code() == EPERM) {
            return Error::from_string_literal(
                "Cannot quarantine file: Permission denied. "
                "Check quarantine directory permissions.");
        } else if (error.code() == EXDEV) {
            // Source and dest on different filesystems - use copy+delete
            dbgln("Quarantine: Cross-device move, using copy strategy");

            auto copy_result = copy_and_delete(source_path, dest_path);
            if (copy_result.is_error()) {
                return Error::from_string_literal(
                    "Cannot quarantine file: Failed to copy across filesystems.");
            }
        } else if (error.code() == ENOENT) {
            return Error::from_string_literal(
                "Cannot quarantine file: Source file not found. "
                "File may have been deleted.");
        } else {
            // Generic error with errno code
            return Error::from_string_literal(
                "Cannot quarantine file: Filesystem error. "
                "Check logs for details.");
        }
    }

    // ... rest of function
}
```

2. Add copy_and_delete helper for cross-filesystem moves:
```cpp
ErrorOr<void> Quarantine::copy_and_delete(
    StringView source_path,
    StringView dest_path)
{
    // Open source file
    auto source_file = TRY(Core::File::open(source_path, Core::File::OpenMode::Read));

    // Read content
    auto content = TRY(source_file->read_until_eof());

    // Create destination file
    auto dest_file = TRY(Core::File::open(dest_path,
        Core::File::OpenMode::Write | Core::File::OpenMode::Truncate));

    // Write content
    TRY(dest_file->write_until_depleted(content.bytes()));

    // Sync to disk
    TRY(dest_file->close());

    // Delete source only after successful copy
    TRY(FileSystem::remove(source_path, FileSystem::RecursionMode::Disallowed));

    return {};
}
```

**Testing**:
- Test with source and quarantine on different filesystems
- Test with permission denied error
- Test with disk full error
- Verify error messages are user-friendly
- Verify no string matching used

---

#### Task 4: Add YARA Rule File Size Limits (MEDIUM)
**Time**: 45 minutes
**File**: `Services/Sentinel/SentinelServer.cpp:45-70`

**Current Problem**: YARA rules loaded without size validation, could cause OOM.

**Solution**: Add size checks before loading, limit total rules size.

**Changes Required**:

1. Add validation in SentinelServer.cpp:
```cpp
ErrorOr<void> SentinelServer::load_yara_rules(ByteString const& rules_path)
{
    // Validate path exists
    if (!FileSystem::exists(rules_path)) {
        return Error::from_string_literal("YARA rules file not found");
    }

    // Check file size before loading
    auto stat_result = TRY(Core::System::stat(rules_path));
    constexpr size_t MAX_RULES_SIZE = 10 * 1024 * 1024; // 10MB

    if (static_cast<size_t>(stat_result.st_size) > MAX_RULES_SIZE) {
        dbgln("Sentinel: YARA rules file too large: {}MB (max: {}MB)",
            stat_result.st_size / (1024 * 1024),
            MAX_RULES_SIZE / (1024 * 1024));
        return Error::from_string_literal("YARA rules file exceeds size limit");
    }

    // Check file is regular file (not device, socket, etc)
    if (!S_ISREG(stat_result.st_mode)) {
        return Error::from_string_literal("YARA rules path is not a regular file");
    }

    // Check file permissions (should not be world-writable)
    if (stat_result.st_mode & S_IWOTH) {
        dbgln("Sentinel: Warning - YARA rules file is world-writable");
        return Error::from_string_literal(
            "YARA rules file has insecure permissions (world-writable)");
    }

    // Load rules into memory
    auto file = TRY(Core::File::open(rules_path, Core::File::OpenMode::Read));
    auto rules_content = TRY(file->read_until_eof());

    // Validate rules content is valid UTF-8
    auto rules_string = String::from_utf8(rules_content);
    if (rules_string.is_error()) {
        return Error::from_string_literal("YARA rules file contains invalid UTF-8");
    }

    // Compile rules with YARA (existing code)
    // ...

    dbgln("Sentinel: Loaded YARA rules from {} ({} bytes)",
        rules_path, rules_content.size());

    return {};
}
```

2. Add configuration for rules size limit in SentinelConfig:
```cpp
// In SentinelConfig.h
struct ScanConfig {
    size_t max_scan_size_bytes { 100 * 1024 * 1024 };  // 100MB
    size_t max_yara_rules_size_bytes { 10 * 1024 * 1024 };  // 10MB
    Duration scan_timeout { Duration::from_seconds(30) };
};

struct SentinelConfig {
    // ... existing members
    ScanConfig scan_config;
};
```

**Testing**:
- Create YARA rules file > 10MB
- Verify rejection with clear error message
- Test with device file (/dev/zero)
- Test with world-writable rules file
- Verify normal rules still load

---

### Afternoon Session (4 hours): Recovery Mechanisms and UI

#### Task 5: Improve UI Error Messages and Notifications (HIGH)
**Time**: 1.5 hours
**File**: `UI/Qt/SecurityNotificationBanner.cpp`, `Libraries/LibWebView/WebUI/SecurityUI.cpp`

**Current Problem**: Generic error messages don't help users understand or resolve issues.

**Solution**: Add specific error types with actionable guidance and recovery options.

**Changes Required**:

1. Add error notification types in SecurityNotificationBanner.cpp:
```cpp
void SecurityNotificationBanner::show_error_notification(
    ErrorType error_type,
    String const& details)
{
    QString icon_path;
    QString message;
    QString action_text;
    QString background_color;

    switch (error_type) {
    case ErrorType::SentinelUnavailable:
        icon_path = ":/Icons/warning.svg";
        message = "Security Protection Disabled";
        details_text = QString::fromStdString(
            "Cannot connect to Sentinel security service. "
            "Downloads are allowed without scanning.");
        action_text = "Check Service";
        background_color = "#FF9800"; // Orange
        break;

    case ErrorType::DatabaseError:
        icon_path = ":/Icons/error.svg";
        message = "Policy Storage Error";
        details_text = QString::fromStdString(
            "Cannot save security policies. "
            "Check disk space and permissions.");
        action_text = "Check Storage";
        background_color = "#F44336"; // Red
        break;

    case ErrorType::QuarantineFailed:
        icon_path = ":/Icons/error.svg";
        message = "Quarantine Failed";
        details_text = QString::fromStdString(details.to_byte_string());
        action_text = "View Options";
        background_color = "#F44336"; // Red
        break;

    case ErrorType::ScanSkipped:
        icon_path = ":/Icons/info.svg";
        message = "File Not Scanned";
        details_text = QString::fromStdString(
            "File too large for scanning. "
            "Review contents manually before opening.");
        action_text = "Dismiss";
        background_color = "#2196F3"; // Blue
        break;

    case ErrorType::ConfigurationError:
        icon_path = ":/Icons/warning.svg";
        message = "Configuration Error";
        details_text = QString::fromStdString(details.to_byte_string());
        action_text = "View Settings";
        background_color = "#FF9800"; // Orange
        break;
    }

    show_notification(message, details_text, icon_path,
        background_color, action_text);
}
```

2. Add diagnostic information panel in about:security:
```html
<!-- Add to Base/res/ladybird/about-pages/security.html -->
<div class="diagnostic-panel" id="diagnosticsPanel" style="display: none;">
    <h3>System Diagnostics</h3>

    <div class="diagnostic-item">
        <span class="diagnostic-label">Sentinel Status:</span>
        <span class="diagnostic-value" id="sentinelStatus">Unknown</span>
    </div>

    <div class="diagnostic-item">
        <span class="diagnostic-label">Database Status:</span>
        <span class="diagnostic-value" id="databaseStatus">Unknown</span>
    </div>

    <div class="diagnostic-item">
        <span class="diagnostic-label">Quarantine Directory:</span>
        <span class="diagnostic-value" id="quarantineDir">Unknown</span>
    </div>

    <div class="diagnostic-item">
        <span class="diagnostic-label">YARA Rules Loaded:</span>
        <span class="diagnostic-value" id="yaraRules">Unknown</span>
    </div>

    <div class="diagnostic-item">
        <span class="diagnostic-label">Last Error:</span>
        <span class="diagnostic-value error-text" id="lastError">None</span>
    </div>

    <button onclick="refreshDiagnostics()" class="action-button">
        Refresh Diagnostics
    </button>

    <button onclick="restartSentinel()" class="action-button">
        Restart Security Service
    </button>
</div>

<script>
function refreshDiagnostics() {
    // Request diagnostic info from backend
    fetch('ladybird://sentinel/diagnostics')
        .then(response => response.json())
        .then(data => {
            document.getElementById('sentinelStatus').textContent =
                data.sentinel_running ? 'Running' : 'Stopped';
            document.getElementById('databaseStatus').textContent =
                data.database_accessible ? 'OK' : 'Error';
            document.getElementById('quarantineDir').textContent =
                data.quarantine_path;
            document.getElementById('yaraRules').textContent =
                data.yara_rules_count + ' rules';
            document.getElementById('lastError').textContent =
                data.last_error || 'None';
        })
        .catch(err => {
            console.error('Failed to fetch diagnostics:', err);
        });
}

function restartSentinel() {
    if (confirm('Restart Sentinel security service?')) {
        fetch('ladybird://sentinel/restart', { method: 'POST' })
            .then(response => {
                if (response.ok) {
                    alert('Sentinel restarted successfully');
                    refreshDiagnostics();
                } else {
                    alert('Failed to restart Sentinel');
                }
            });
    }
}
</script>
```

3. Add IPC message for diagnostics:
```cpp
// In WebContentServer.ipc
messages WebContentServer {
    // ... existing messages

    get_sentinel_diagnostics() => (struct DiagnosticInfo info)
    restart_sentinel_service() => (bool success)
}

// In ConnectionFromClient.cpp
Messages::WebContentServer::GetSentinelDiagnosticsResponse
ConnectionFromClient::get_sentinel_diagnostics()
{
    DiagnosticInfo info;

    // Check Sentinel connectivity
    info.sentinel_running = SecurityTap::check_sentinel_available();

    // Check database
    info.database_accessible = PolicyGraph::verify_database_health();

    // Get quarantine path
    auto quarantine_path = Quarantine::get_quarantine_directory();
    info.quarantine_path = quarantine_path.value_or("Unknown"_string);

    // Get YARA rules count
    info.yara_rules_count = SentinelServer::get_loaded_rules_count();

    // Get last error from error log
    info.last_error = ErrorLog::get_last_error();

    return info;
}
```

**Testing**:
- Trigger each error type
- Verify appropriate message and color shown
- Test diagnostic panel displays correct info
- Test restart functionality
- Verify actionable guidance provided

---

#### Task 6: Add Quarantine Operation Rollback (MEDIUM)
**Time**: 1 hour
**File**: `Services/RequestServer/Quarantine.cpp:86-156`

**Current Problem**: As noted in technical debt, metadata write failure after file move causes orphaned files.

**Solution**: Already implemented in existing code! Verify and enhance with better logging.

**Verification**:
The current code at lines 143-152 already implements rollback:
```cpp
auto metadata_result = write_metadata(quarantine_id, updated_metadata);
if (metadata_result.is_error()) {
    dbgln("Quarantine: Failed to write metadata: {}", metadata_result.error());
    // Clean up quarantined file if metadata write fails
    auto cleanup = FileSystem::remove(dest_path, FileSystem::RecursionMode::Disallowed);
    if (cleanup.is_error()) {
        dbgln("Quarantine: Warning - failed to clean up quarantined file after metadata error");
    }
    return Error::from_string_literal("Cannot write quarantine metadata. The file was not quarantined.");
}
```

**Enhancement**: Add orphan detection and recovery on startup:
```cpp
// Add to Quarantine initialization
ErrorOr<void> Quarantine::detect_and_recover_orphans()
{
    auto quarantine_dir = TRY(get_quarantine_directory());

    // List all .bin files
    Vector<String> orphaned_files;

    auto dir_iterator = Core::DirIterator(quarantine_dir,
        Core::DirIterator::SkipDots);

    if (dir_iterator.has_error()) {
        return Error::from_string_literal("Cannot scan quarantine directory");
    }

    while (dir_iterator.has_next()) {
        auto entry = dir_iterator.next();
        if (!entry.has_value())
            break;

        auto filename = entry.value().name;

        // Check for .bin files
        if (filename.ends_with(".bin"sv)) {
            // Extract quarantine ID (remove .bin extension)
            auto id = filename.substring(0, filename.length() - 4);

            // Check if corresponding .json exists
            auto json_path = ByteString::formatted("{}/{}.json",
                quarantine_dir, id);

            if (!FileSystem::exists(json_path)) {
                dbgln("Quarantine: Found orphaned file: {}", filename);
                orphaned_files.append(String::from_byte_string(filename).release_value());
            }
        }
    }

    // Clean up orphaned files
    if (!orphaned_files.is_empty()) {
        dbgln("Quarantine: Cleaning up {} orphaned files", orphaned_files.size());

        for (auto const& filename : orphaned_files) {
            auto file_path = ByteString::formatted("{}/{}",
                quarantine_dir, filename);

            auto remove_result = FileSystem::remove(file_path,
                FileSystem::RecursionMode::Disallowed);

            if (remove_result.is_error()) {
                dbgln("Quarantine: Failed to remove orphan {}: {}",
                    filename, remove_result.error());
            } else {
                dbgln("Quarantine: Removed orphaned file: {}", filename);
            }
        }
    }

    return {};
}
```

**Testing**:
- Simulate metadata write failure (chmod quarantine dir read-only)
- Verify file removed from quarantine
- Verify error message clear
- Create orphaned .bin file manually
- Verify cleanup on next initialization

---

#### Task 7: Add Comprehensive Logging for Debugging (MEDIUM)
**Time**: 1 hour
**File**: All Sentinel components

**Current Problem**: Insufficient logging makes debugging production issues difficult.

**Solution**: Add structured logging with severity levels and context.

**Changes Required**:

1. Create logging utility in `Services/Sentinel/SentinelLogger.h`:
```cpp
#pragma once

#include <AK/String.h>
#include <AK/StringView.h>
#include <AK/Time.h>

namespace Sentinel {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static void log(LogLevel level, StringView component, StringView message);

    static void debug(StringView component, StringView message) {
        log(LogLevel::Debug, component, message);
    }

    static void info(StringView component, StringView message) {
        log(LogLevel::Info, component, message);
    }

    static void warning(StringView component, StringView message) {
        log(LogLevel::Warning, component, message);
    }

    static void error(StringView component, StringView message) {
        log(LogLevel::Error, component, message);
    }

    static void critical(StringView component, StringView message) {
        log(LogLevel::Critical, component, message);
    }

    static void set_log_file(String const& path);
    static void set_min_level(LogLevel level);

private:
    static LogLevel s_min_level;
    static Optional<String> s_log_file_path;
};

}
```

2. Implement in `Services/Sentinel/SentinelLogger.cpp`:
```cpp
#include <Services/Sentinel/SentinelLogger.h>
#include <AK/Format.h>
#include <LibCore/File.h>

namespace Sentinel {

LogLevel Logger::s_min_level = LogLevel::Info;
Optional<String> Logger::s_log_file_path;

void Logger::log(LogLevel level, StringView component, StringView message)
{
    if (level < s_min_level)
        return;

    // Format: [TIMESTAMP] [LEVEL] [COMPONENT] Message
    auto now = UnixDateTime::now();
    auto timestamp = now.to_string();

    StringView level_str;
    switch (level) {
    case LogLevel::Debug:
        level_str = "DEBUG"sv;
        break;
    case LogLevel::Info:
        level_str = "INFO"sv;
        break;
    case LogLevel::Warning:
        level_str = "WARN"sv;
        break;
    case LogLevel::Error:
        level_str = "ERROR"sv;
        break;
    case LogLevel::Critical:
        level_str = "CRIT"sv;
        break;
    }

    auto log_line = String::formatted("[{}] [{}] [{}] {}\n",
        timestamp, level_str, component, message);

    // Write to stderr
    warnln("{}", log_line);

    // Write to log file if configured
    if (s_log_file_path.has_value()) {
        auto file_result = Core::File::open(s_log_file_path.value(),
            Core::File::OpenMode::Append | Core::File::OpenMode::Write);

        if (!file_result.is_error()) {
            auto file = file_result.release_value();
            auto write_result = file->write_until_depleted(log_line.bytes());
            if (write_result.is_error()) {
                warnln("Failed to write to log file: {}", write_result.error());
            }
        }
    }
}

void Logger::set_log_file(String const& path)
{
    s_log_file_path = path;
}

void Logger::set_min_level(LogLevel level)
{
    s_min_level = level;
}

}
```

3. Replace dbgln() calls with structured logging:
```cpp
// Before:
dbgln("Quarantine: Failed to move file: {}", move_result.error());

// After:
Logger::error("Quarantine"sv,
    String::formatted("Failed to move file {}: {}",
        source_path, move_result.error()));
```

4. Add context to critical operations:
```cpp
// In PolicyGraph::create_policy()
Logger::info("PolicyGraph"sv,
    String::formatted("Creating policy: rule={}, action={}",
        policy.rule_name, policy_action_to_string(policy.action)));

if (exec_result.is_error()) {
    Logger::error("PolicyGraph"sv,
        String::formatted("Database insert failed: {}",
            exec_result.error()));
}

// In SecurityTap::inspect_download()
Logger::debug("SecurityTap"sv,
    String::formatted("Scanning file: {} ({} bytes)",
        metadata.filename, content.size()));

if (scan_result.is_threat) {
    Logger::warning("SecurityTap"sv,
        String::formatted("Threat detected: {} from {}",
            metadata.filename, metadata.url));
}
```

**Testing**:
- Enable debug logging
- Trigger various operations
- Verify log output is structured and readable
- Check log file contains all entries
- Test log rotation (if implemented)

---

## Day 33 Summary

### Deliverables
1. Sentinel daemon failure handling with retry and notifications
2. Database failure recovery with retry logic
3. Improved error detection using errno codes
4. YARA rule file size limits and validation
5. Enhanced UI error messages with actionable guidance
6. Quarantine orphan detection and recovery
7. Comprehensive structured logging system

### Testing Checklist
- [ ] Stop Sentinel, verify graceful degradation
- [ ] Lock database, verify retry and error handling
- [ ] Test quarantine with full disk
- [ ] Test YARA rules > 10MB
- [ ] Verify all error messages are user-friendly
- [ ] Test orphan file cleanup
- [ ] Verify logging captures all important events

### Success Criteria
- ✓ System handles all failure scenarios gracefully
- ✓ No crashes or undefined behavior on errors
- ✓ Users receive actionable error messages
- ✓ Recovery mechanisms automatically fix transient issues
- ✓ Comprehensive logging aids debugging
- ✓ All tests pass

---

## Day 34: Configuration System + Remaining Tests

(Day 34 will focus on configuration system implementation, environment variable support, and remaining test coverage as outlined in the prioritized action plan)

---

## Day 35: Phase 6 Foundation (Milestone 0.2 Preparation)

### Overview
Day 35 resolves the **2 critical TODO items blocking Phase 6** and implements the foundation components (FormMonitor, FlowInspector, PolicyGraph extensions) needed for Milestone 0.2 (Credential Exfiltration Detection).

**Critical TODO Items**:
1. `Services/RequestServer/ConnectionFromClient.cpp:1624` - Forward credential alerts to WebContent
2. `Services/WebContent/ConnectionFromClient.cpp:1509` - Send form submissions to Sentinel
3. `Base/res/ladybird/about-pages/security.html:730,740` - Complete policies/threats tables

---

### Morning Session (4 hours): FormMonitor Integration + TODO Resolution

#### Task 1: Complete FormMonitor Implementation (CRITICAL)
**Time**: 2 hours
**Files**:
- `Services/WebContent/FormMonitor.h` (EXISTS - needs completion)
- `Services/WebContent/FormMonitor.cpp` (EXISTS - needs completion)
- `Services/WebContent/ConnectionFromClient.cpp:1500-1512`

**Status**: Foundation exists, needs completion and IPC integration

**Current Implementation Review**:
The FormMonitor foundation is already in place with:
- Basic structure defined in `FormMonitor.h`
- Alert generation logic in `FormMonitor.cpp`
- Placeholder IPC handler in `ConnectionFromClient.cpp:1509`

**Changes Required**:

1. **Complete FormMonitor.cpp Implementation** (add missing methods after line 100):
```cpp
String FormMonitor::extract_origin(URL::URL const& url) const
{
    // Origin = scheme + "://" + host + (port if non-default)
    StringBuilder origin_builder;
    origin_builder.append(url.scheme());
    origin_builder.append("://"sv);
    origin_builder.append(url.host().map([](auto const& h) { return h.serialize(); }).value_or(String{}));

    // Include port if it's not the default for the scheme
    if (url.port().has_value()) {
        auto port = url.port().value();
        bool is_default_port = (url.scheme() == "https"sv && port == 443) ||
                               (url.scheme() == "http"sv && port == 80);
        if (!is_default_port) {
            origin_builder.append(':');
            origin_builder.appendff("{}", port);
        }
    }

    return MUST(origin_builder.to_string());
}

bool FormMonitor::is_cross_origin_submission(URL::URL const& form_url, URL::URL const& action_url) const
{
    auto form_origin = extract_origin(form_url);
    auto action_origin = extract_origin(action_url);

    return form_origin != action_origin;
}

bool FormMonitor::is_insecure_credential_submission(FormSubmitEvent const& event) const
{
    // Check if password is submitted over HTTP
    if (!event.has_password_field)
        return false;

    return event.action_url.scheme() != "https"sv;
}

bool FormMonitor::is_third_party_submission(URL::URL const& form_url, URL::URL const& action_url) const
{
    // Extract eTLD+1 (effective top-level domain + 1 label)
    // For now, use simple heuristic: compare host domains

    auto form_host = form_url.host().map([](auto const& h) { return h.serialize(); }).value_or(String{});
    auto action_host = action_url.host().map([](auto const& h) { return h.serialize(); }).value_or(String{});

    // If hosts are identical, not third-party
    if (form_host == action_host)
        return false;

    // Check if action host is subdomain of form host or vice versa
    if (form_host.ends_with(action_host) || action_host.ends_with(form_host))
        return false;

    // Different domains = third-party
    return true;
}

String FormMonitor::CredentialAlert::to_json() const
{
    JsonObject json;
    json.set("alert_type"sv, alert_type);
    json.set("severity"sv, severity);
    json.set("form_origin"sv, form_origin);
    json.set("action_origin"sv, action_origin);
    json.set("uses_https"sv, uses_https);
    json.set("has_password_field"sv, has_password_field);
    json.set("is_cross_origin"sv, is_cross_origin);
    json.set("timestamp"sv, timestamp.seconds_since_epoch());

    StringBuilder builder;
    json.serialize(builder);
    return MUST(builder.to_string());
}
```

2. **Resolve TODO at ConnectionFromClient.cpp:1509** - Send form events to Sentinel:
```cpp
void ConnectionFromClient::form_submission_detected(u64 page_id, String form_origin, String action_origin, bool has_password, bool has_email, bool uses_https)
{
    dbgln("WebContent: Form submission detected on page {}", page_id);
    dbgln("  Form origin: {}", form_origin);
    dbgln("  Action origin: {}", action_origin);
    dbgln("  Has password: {}", has_password);
    dbgln("  Has email: {}", has_email);
    dbgln("  Uses HTTPS: {}", uses_https);

    // RESOLVED TODO: Send form submission alert to Sentinel via IPC for monitoring
    // Build JSON payload for Sentinel
    JsonObject event;
    event.set("form_origin"sv, form_origin);
    event.set("action_origin"sv, action_origin);
    event.set("has_password_field"sv, has_password);
    event.set("has_email_field"sv, has_email);
    event.set("uses_https"sv, uses_https);
    event.set("is_cross_origin"sv, form_origin != action_origin);
    event.set("timestamp"sv, UnixDateTime::now().seconds_since_epoch());

    StringBuilder builder;
    event.serialize(builder);
    auto event_json = MUST(builder.to_string());

    // Forward to RequestServer which will relay to Sentinel
    if (auto request_server = client().request_server_connection()) {
        request_server->async_form_submission_event(event_json.to_byte_string());
    } else {
        dbgln("WebContent: Warning - No RequestServer connection for form event");
    }
}
```

**Testing**:
```cpp
// Tests/WebContent/TestFormMonitor.cpp
TEST_CASE(test_cross_origin_detection)
{
    FormMonitor monitor;

    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::URL("https://example.com/login"sv);
    event.action_url = URL::URL("https://malicious.com/steal"sv);
    event.has_password_field = true;
    event.method = "POST"_string;

    EXPECT(monitor.is_suspicious_submission(event));

    auto alert = monitor.analyze_submission(event);
    EXPECT(alert.has_value());
    EXPECT_EQ(alert->alert_type, "credential_exfiltration"_string);
    EXPECT_EQ(alert->severity, "high"_string);
}

TEST_CASE(test_insecure_http_detection)
{
    FormMonitor monitor;

    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::URL("http://bank.com/login"sv);
    event.action_url = URL::URL("http://bank.com/auth"sv);
    event.has_password_field = true;
    event.method = "POST"_string;

    EXPECT(monitor.is_suspicious_submission(event));

    auto alert = monitor.analyze_submission(event);
    EXPECT(alert.has_value());
    EXPECT_EQ(alert->alert_type, "insecure_credential_post"_string);
    EXPECT_EQ(alert->severity, "critical"_string);
}

TEST_CASE(test_legitimate_same_origin_form)
{
    FormMonitor monitor;

    FormMonitor::FormSubmitEvent event;
    event.document_url = URL::URL("https://example.com/login"sv);
    event.action_url = URL::URL("https://example.com/auth"sv);
    event.has_password_field = true;
    event.method = "POST"_string;

    EXPECT(!monitor.is_suspicious_submission(event));
}
```

---

#### Task 2: Add IPC Message for Form Submissions to Sentinel (CRITICAL)
**Time**: 1 hour
**Files**:
- `Services/RequestServer/RequestServer.ipc:62-64`
- `Services/RequestServer/ConnectionFromClient.h`
- `Services/RequestServer/ConnectionFromClient.cpp`
- `Services/RequestServer/SecurityTap.h/cpp`

**Status**: Placeholder exists at line 63, needs implementation

**Changes Required**:

1. **Update RequestServer.ipc** - Add new IPC message after line 63:
```cpp
    // Credential exfiltration detection (Sentinel Phase 5 Day 35)
    credential_exfil_alert(ByteString alert_json) =|

    // NEW: Form submission events from WebContent
    form_submission_event(ByteString event_json) =|
```

2. **Add Handler in ConnectionFromClient.h**:
```cpp
private:
    virtual void credential_exfil_alert(ByteString alert_json) override;
    virtual void form_submission_event(ByteString event_json) override;  // NEW
```

3. **Implement in ConnectionFromClient.cpp** (after line 1625):
```cpp
void ConnectionFromClient::form_submission_event(ByteString event_json)
{
    dbgln("RequestServer: Form submission event received: {}", event_json);

    // Parse the event JSON
    auto json_result = JsonValue::from_string(event_json);
    if (json_result.is_error()) {
        dbgln("RequestServer: Failed to parse form submission event JSON: {}",
              json_result.error());
        return;
    }

    auto json = json_result.release_value();
    if (!json.is_object()) {
        dbgln("RequestServer: Form submission event is not a JSON object");
        return;
    }

    auto obj = json.as_object();

    // Extract event data
    auto form_origin = obj.get("form_origin"sv).value_or(""sv);
    auto action_origin = obj.get("action_origin"sv).value_or(""sv);
    bool has_password = obj.get("has_password_field"sv).value_or(JsonValue(false)).as_bool();
    bool has_email = obj.get("has_email_field"sv).value_or(JsonValue(false)).as_bool();
    bool uses_https = obj.get("uses_https"sv).value_or(JsonValue(false)).as_bool();
    bool is_cross_origin = obj.get("is_cross_origin"sv).value_or(JsonValue(false)).as_bool();

    // Forward to Sentinel's FlowInspector via SecurityTap
    if (m_security_tap) {
        auto result = m_security_tap->send_form_submission_event(
            form_origin.to_byte_string(),
            action_origin.to_byte_string(),
            has_password,
            has_email,
            uses_https,
            is_cross_origin
        );

        if (result.is_error()) {
            dbgln("RequestServer: Failed to forward form event to Sentinel: {}",
                  result.error());
        }
    } else {
        dbgln("RequestServer: No SecurityTap connection, cannot forward form event");
    }
}
```

4. **Add SecurityTap Method** in `SecurityTap.h`:
```cpp
class SecurityTap {
public:
    // ... existing methods ...

    // NEW: Send form submission event to Sentinel
    ErrorOr<void> send_form_submission_event(
        ByteString const& form_origin,
        ByteString const& action_origin,
        bool has_password_field,
        bool has_email_field,
        bool uses_https,
        bool is_cross_origin
    );
};
```

5. **Implement in SecurityTap.cpp**:
```cpp
ErrorOr<void> SecurityTap::send_form_submission_event(
    ByteString const& form_origin,
    ByteString const& action_origin,
    bool has_password_field,
    bool has_email_field,
    bool uses_https,
    bool is_cross_origin)
{
    if (!m_sentinel_socket || m_connection_failed) {
        // Fail open - don't block form submissions
        return {};
    }

    // Build JSON message for Sentinel
    JsonObject message;
    message.set("operation"sv, "analyze_form_submission"sv);

    JsonObject event;
    event.set("form_origin"sv, form_origin);
    event.set("action_origin"sv, action_origin);
    event.set("has_password_field"sv, has_password_field);
    event.set("has_email_field"sv, has_email_field);
    event.set("uses_https"sv, uses_https);
    event.set("is_cross_origin"sv, is_cross_origin);
    event.set("timestamp"sv, UnixDateTime::now().seconds_since_epoch());

    message.set("event"sv, event);

    StringBuilder message_builder;
    message.serialize(message_builder);
    message_builder.append('\n');

    auto message_str = TRY(message_builder.to_string());

    // Send to Sentinel (async, don't wait for response)
    auto write_result = m_sentinel_socket->write_until_depleted(message_str.bytes());
    if (write_result.is_error()) {
        dbgln("SecurityTap: Failed to send form event to Sentinel: {}",
              write_result.error());
        m_connection_failed = true;
        return write_result.release_error();
    }

    return {};
}
```

**Testing**:
- Test IPC message serialization
- Test connection to Sentinel
- Test fail-open behavior when Sentinel unavailable
- Verify JSON format matches FlowInspector expectations

---

#### Task 3: Resolve RequestServer TODO (Phase 6 Blocker)
**Time**: 45 minutes
**File**: `Services/RequestServer/ConnectionFromClient.cpp:1619-1625`

**Current Code**:
```cpp
void ConnectionFromClient::credential_exfil_alert(ByteString alert_json)
{
    // This method will be fully implemented in Phase 6 (Milestone 0.2)
    // For now, just log the alert for debugging
    dbgln("ConnectionFromClient: Credential exfiltration alert received: {}", alert_json);
    // TODO: Phase 6 - Forward to WebContent for user notification
}
```

**Changes Required** - Forward alerts to WebContent:

```cpp
void ConnectionFromClient::credential_exfil_alert(ByteString alert_json)
{
    dbgln("ConnectionFromClient: Credential exfiltration alert received");

    // Parse the alert JSON
    auto json_result = JsonValue::from_string(alert_json);
    if (json_result.is_error()) {
        dbgln("RequestServer: Failed to parse credential alert JSON: {}",
              json_result.error());
        return;
    }

    auto json = json_result.release_value();
    if (!json.is_object()) {
        dbgln("RequestServer: Credential alert is not a JSON object");
        return;
    }

    // RESOLVED TODO: Forward to WebContent for user notification
    // Broadcast to all WebContent clients
    // Each WebContent process will determine if alert applies to their pages
    for_each_client([&](auto& web_content_client) {
        web_content_client.async_display_credential_exfil_alert(alert_json);
    });

    dbgln("RequestServer: Forwarded credential alert to WebContent client(s)");
}
```

**Add IPC Message to WebContentServer.ipc** (after line 149):
```cpp
    // Form submission monitoring (Sentinel Phase 5 Day 35)
    form_submission_detected(u64 page_id, String form_origin, String action_origin, bool has_password, bool has_email, bool uses_https) =|

    // NEW: Display credential exfiltration alert to user
    display_credential_exfil_alert(ByteString alert_json) =|
```

**Implement Handler in WebContent/ConnectionFromClient.h/cpp** (after line 1512):
```cpp
void ConnectionFromClient::display_credential_exfil_alert(ByteString alert_json)
{
    dbgln("WebContent: Received credential exfiltration alert");

    // Parse alert
    auto json_result = JsonValue::from_string(alert_json);
    if (json_result.is_error()) {
        dbgln("WebContent: Failed to parse alert JSON");
        return;
    }

    auto json = json_result.release_value();
    if (!json.is_object())
        return;

    auto obj = json.as_object();
    auto alert_type = obj.get("alert_type"sv).value_or(""sv);
    auto severity = obj.get("severity"sv).value_or(""sv);
    auto form_origin = obj.get("form_origin"sv).value_or(""sv);
    auto action_origin = obj.get("action_origin"sv).value_or(""sv);

    // Forward to UI layer for display
    // This will be handled by SecurityNotificationBanner in Qt UI
    client().async_show_security_alert(
        String::from_utf8(alert_type).release_value_but_fixme_should_propagate_errors(),
        String::from_utf8(severity).release_value_but_fixme_should_propagate_errors(),
        String::from_utf8(form_origin).release_value_but_fixme_should_propagate_errors(),
        String::from_utf8(action_origin).release_value_but_fixme_should_propagate_errors()
    );
}
```

**Testing**:
- Simulate Sentinel sending credential alert
- Verify WebContent receives and displays alert
- Test with multiple WebContent processes
- Verify UI notification appears

---

#### Task 4: Complete about:security UI Tables (Medium Priority)
**Time**: 15 minutes
**File**: `Base/res/ladybird/about-pages/security.html:730,740`

**RESOLVED TODO**: Replace TODO comments with actual implementation:

```javascript
function renderPolicies(data) {
    if (!data.policies || data.policies.length === 0) {
        document.getElementById('policies-empty').classList.remove('hidden');
        return;
    }

    document.getElementById('policies-empty').classList.add('hidden');

    // RESOLVED TODO: Render policies table when PolicyGraph integration is complete
    const tbody = document.querySelector('#policies-table tbody');
    tbody.innerHTML = '';

    data.policies.forEach(policy => {
        const row = document.createElement('tr');

        // Rule Name
        const nameCell = document.createElement('td');
        nameCell.textContent = policy.rule_name || 'Unknown';
        row.appendChild(nameCell);

        // Match Type
        const typeCell = document.createElement('td');
        typeCell.textContent = formatMatchType(policy.match_type);
        row.appendChild(typeCell);

        // Action
        const actionCell = document.createElement('td');
        actionCell.textContent = formatPolicyAction(policy.action);
        actionCell.className = `action-${policy.action.toLowerCase()}`;
        row.appendChild(actionCell);

        // Created
        const createdCell = document.createElement('td');
        createdCell.textContent = formatTimestamp(policy.created_at);
        row.appendChild(createdCell);

        // Hit Count
        const hitsCell = document.createElement('td');
        hitsCell.textContent = policy.hit_count || 0;
        row.appendChild(hitsCell);

        // Actions
        const actionsCell = document.createElement('td');
        const deleteBtn = document.createElement('button');
        deleteBtn.textContent = 'Delete';
        deleteBtn.className = 'btn-small btn-danger';
        deleteBtn.onclick = () => deletePolicy(policy.id);
        actionsCell.appendChild(deleteBtn);
        row.appendChild(actionsCell);

        tbody.appendChild(row);
    });
}

function renderThreats(data) {
    if (!data.threats || data.threats.length === 0) {
        document.getElementById('threats-empty').classList.remove('hidden');
        return;
    }

    document.getElementById('threats-empty').classList.add('hidden');

    // RESOLVED TODO: Render threats table when PolicyGraph integration is complete
    const tbody = document.querySelector('#threats-table tbody');
    tbody.innerHTML = '';

    data.threats.forEach(threat => {
        const row = document.createElement('tr');

        // Time
        const timeCell = document.createElement('td');
        timeCell.textContent = formatTimestamp(threat.detected_at);
        row.appendChild(timeCell);

        // Threat Type
        const typeCell = document.createElement('td');
        typeCell.textContent = threat.threat_type || 'Unknown';
        row.appendChild(typeCell);

        // Source URL
        const urlCell = document.createElement('td');
        urlCell.textContent = threat.url || 'N/A';
        urlCell.className = 'text-truncate';
        urlCell.title = threat.url;
        row.appendChild(urlCell);

        // Filename
        const fileCell = document.createElement('td');
        fileCell.textContent = threat.filename || 'N/A';
        row.appendChild(fileCell);

        // Action Taken
        const actionCell = document.createElement('td');
        actionCell.textContent = formatActionTaken(threat.action_taken);
        actionCell.className = `action-${threat.action_taken.toLowerCase()}`;
        row.appendChild(actionCell);

        tbody.appendChild(row);
    });
}

// Helper functions
function formatMatchType(type) {
    const types = {
        0: 'Download Origin + File Type',
        1: 'Form Action Mismatch',
        2: 'Insecure Credential POST',
        3: 'Third-Party Form POST'
    };
    return types[type] || 'Unknown';
}

function formatPolicyAction(action) {
    const actions = {
        0: 'Allow',
        1: 'Block',
        2: 'Quarantine',
        3: 'Block Autofill',
        4: 'Warn User'
    };
    return actions[action] || 'Unknown';
}

function formatActionTaken(action) {
    return action.charAt(0).toUpperCase() + action.slice(1).toLowerCase();
}

function formatTimestamp(timestamp) {
    if (!timestamp) return 'Unknown';
    const date = new Date(timestamp * 1000);
    return date.toLocaleString();
}

function deletePolicy(policyId) {
    if (!confirm('Are you sure you want to delete this policy?'))
        return;

    ladybird.sendMessage("deletePolicy", { id: policyId });
    setTimeout(refreshPolicies, 500);
}
```

**Testing**:
- Load about:security page
- Verify policies table renders with sample data
- Verify threats table renders with sample data
- Test delete button functionality

---

### Afternoon Session (4 hours): FlowInspector Integration + PolicyGraph Extensions

#### Task 5: Complete FlowInspector Implementation (CRITICAL)
**Time**: 1.5 hours
**Files**:
- `Services/Sentinel/FlowInspector.h` (EXISTS - needs completion)
- `Services/Sentinel/FlowInspector.cpp` (EXISTS - needs completion)
- `Services/Sentinel/SentinelServer.cpp` (needs integration)

**Status**: Foundation exists, needs completion and Sentinel integration

**Changes Required**:

1. **Complete FlowInspector.cpp** (add missing methods after line 100):
```cpp
ErrorOr<void> FlowInspector::learn_trusted_relationship(
    String const& form_origin,
    String const& action_origin)
{
    dbgln("FlowInspector: Learning trusted relationship: {} -> {}",
          form_origin, action_origin);

    // Check if already exists
    auto it = m_trusted_relationships.find(form_origin);
    if (it != m_trusted_relationships.end()) {
        // Check if this specific action_origin is already trusted
        for (auto& rel : it->value) {
            if (rel.action_origin == action_origin) {
                rel.submission_count++;
                dbgln("FlowInspector: Updated existing relationship (count: {})",
                      rel.submission_count);
                return {};
            }
        }
    }

    // Add new trusted relationship
    TrustedFormRelationship relationship;
    relationship.form_origin = form_origin;
    relationship.action_origin = action_origin;
    relationship.learned_at = UnixDateTime::now();
    relationship.submission_count = 1;

    m_trusted_relationships.ensure(form_origin).append(relationship);

    dbgln("FlowInspector: Added new trusted relationship");
    return {};
}

bool FlowInspector::is_trusted_relationship(
    String const& form_origin,
    String const& action_origin) const
{
    auto it = m_trusted_relationships.find(form_origin);
    if (it == m_trusted_relationships.end())
        return false;

    for (auto const& rel : it->value) {
        if (rel.action_origin == action_origin)
            return true;
    }

    return false;
}

Vector<FlowInspector::CredentialAlert> const& FlowInspector::get_recent_alerts(
    Optional<UnixDateTime> since) const
{
    if (!since.has_value())
        return m_alerts;

    // TODO: Filter by timestamp when needed
    // For now, return all alerts
    return m_alerts;
}

void FlowInspector::cleanup_old_alerts(u64 hours_to_keep)
{
    auto cutoff = UnixDateTime::now().seconds_since_epoch() - (hours_to_keep * 3600);

    m_alerts.remove_all_matching([cutoff](auto const& alert) {
        return alert.timestamp.seconds_since_epoch() < cutoff;
    });

    dbgln("FlowInspector: Cleaned up old alerts (keeping last {} hours)", hours_to_keep);
}

String FlowInspector::generate_description(CredentialAlert const& alert) const
{
    StringBuilder description;

    switch (alert.alert_type) {
    case AlertType::InsecureCredentialPost:
        description.appendff(
            "Form on {} is submitting passwords over insecure HTTP to {}",
            alert.form_origin, alert.action_origin);
        break;
    case AlertType::CredentialExfiltration:
        description.appendff(
            "Form on {} is attempting to send credentials to different origin {}",
            alert.form_origin, alert.action_origin);
        break;
    case AlertType::ThirdPartyFormPost:
        description.appendff(
            "Form on {} is posting data to third-party domain {}",
            alert.form_origin, alert.action_origin);
        break;
    case AlertType::FormActionMismatch:
        description.appendff(
            "Form on {} has action URL on different origin {}",
            alert.form_origin, alert.action_origin);
        break;
    }

    return MUST(description.to_string());
}

String FlowInspector::alert_type_to_string(AlertType type)
{
    switch (type) {
    case AlertType::InsecureCredentialPost:
        return "insecure_credential_post"_string;
    case AlertType::CredentialExfiltration:
        return "credential_exfiltration"_string;
    case AlertType::ThirdPartyFormPost:
        return "third_party_form_post"_string;
    case AlertType::FormActionMismatch:
        return "form_action_mismatch"_string;
    }
    return "unknown"_string;
}

String FlowInspector::severity_to_string(AlertSeverity severity)
{
    switch (severity) {
    case AlertSeverity::Low:
        return "low"_string;
    case AlertSeverity::Medium:
        return "medium"_string;
    case AlertSeverity::High:
        return "high"_string;
    case AlertSeverity::Critical:
        return "critical"_string;
    }
    return "unknown"_string;
}

String FlowInspector::CredentialAlert::to_json() const
{
    JsonObject json;
    json.set("alert_type"sv, alert_type_to_string(alert_type));
    json.set("severity"sv, severity_to_string(severity));
    json.set("form_origin"sv, form_origin);
    json.set("action_origin"sv, action_origin);
    json.set("uses_https"sv, uses_https);
    json.set("has_password_field"sv, has_password_field);
    json.set("is_cross_origin"sv, is_cross_origin);
    json.set("timestamp"sv, timestamp.seconds_since_epoch());
    json.set("description"sv, description);

    StringBuilder builder;
    json.serialize(builder);
    return MUST(builder.to_string());
}
```

2. **Integrate FlowInspector into SentinelServer.cpp**:

Add member variable around line 30-40:
```cpp
class SentinelServer {
    // ... existing members ...
    OwnPtr<FlowInspector> m_flow_inspector;
};
```

Initialize in constructor:
```cpp
SentinelServer::SentinelServer()
    : m_policy_graph(/* ... */)
    , m_flow_inspector(make<FlowInspector>())
{
    // ... existing initialization ...
}
```

Add handler in `handle_client_message()` around line 180-200:
```cpp
} else if (operation == "analyze_form_submission"sv) {
    // Extract form submission event from message
    auto event_obj = message_obj.get("event"sv);
    if (!event_obj.has_value() || !event_obj->is_object()) {
        return Error::from_string_literal("Missing or invalid 'event' field");
    }

    auto event = event_obj->as_object();

    FlowInspector::FormSubmissionEvent submission_event;
    submission_event.form_origin = event.get("form_origin"sv).value().to_string();
    submission_event.action_origin = event.get("action_origin"sv).value().to_string();
    submission_event.has_password_field = event.get("has_password_field"sv).value().as_bool();
    submission_event.has_email_field = event.get("has_email_field"sv).value().as_bool();
    submission_event.uses_https = event.get("uses_https"sv).value().as_bool();
    submission_event.is_cross_origin = event.get("is_cross_origin"sv).value().as_bool();
    submission_event.timestamp = UnixDateTime::now();

    // Analyze with FlowInspector
    auto alert_result = m_flow_inspector->analyze_form_submission(submission_event);
    if (alert_result.is_error()) {
        return alert_result.release_error();
    }

    // Build response
    JsonObject response;
    response.set("status"sv, "success"sv);

    if (alert_result.value().has_value()) {
        auto alert = alert_result.value().value();
        response.set("alert_generated"sv, true);
        response.set("alert"sv, JsonValue::from_string(alert.to_json()).release_value());

        // TODO Phase 6: Forward alert to RequestServer for UI notification
        // This will be implemented in Phase 6 with full UI integration
    } else {
        response.set("alert_generated"sv, false);
    }

    return response;
```

**Testing**:
```cpp
// Tests/Sentinel/TestFlowInspector.cpp
TEST_CASE(test_insecure_credential_post_detection)
{
    FlowInspector inspector;

    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "http://bank.com"_string;
    event.action_origin = "http://bank.com"_string;
    event.has_password_field = true;
    event.uses_https = false;
    event.is_cross_origin = false;

    auto result = inspector.analyze_form_submission(event);
    EXPECT(!result.is_error());
    EXPECT(result.value().has_value());

    auto alert = result.value().value();
    EXPECT_EQ(alert.alert_type, FlowInspector::AlertType::InsecureCredentialPost);
    EXPECT_EQ(alert.severity, FlowInspector::AlertSeverity::Critical);
}

TEST_CASE(test_trusted_relationship_learning)
{
    FlowInspector inspector;

    auto result = inspector.learn_trusted_relationship(
        "https://example.com"_string,
        "https://auth.example.com"_string
    );
    EXPECT(!result.is_error());

    EXPECT(inspector.is_trusted_relationship(
        "https://example.com"_string,
        "https://auth.example.com"_string
    ));
}

TEST_CASE(test_trusted_relationship_prevents_alert)
{
    FlowInspector inspector;

    // Learn relationship first
    inspector.learn_trusted_relationship(
        "https://example.com"_string,
        "https://auth.example.com"_string
    );

    // Submit form with trusted relationship
    FlowInspector::FormSubmissionEvent event;
    event.form_origin = "https://example.com"_string;
    event.action_origin = "https://auth.example.com"_string;
    event.has_password_field = true;
    event.uses_https = true;
    event.is_cross_origin = true;

    auto result = inspector.analyze_form_submission(event);
    EXPECT(!result.is_error());
    EXPECT(!result.value().has_value());  // No alert
}
```

---

#### Task 6: Extend PolicyGraph for Phase 6 Policy Types (CRITICAL)
**Time**: 1.5 hours
**Files**:
- `Services/Sentinel/PolicyGraph.h:64-96` (already extended)
- `Services/Sentinel/PolicyGraph.cpp` (needs query support)
- `Services/Sentinel/DatabaseMigrations.cpp` (needs schema update)

**Status**: Enum extensions exist (BlockAutofill, WarnUser, FormActionMismatch, etc.), need database and query support

**Changes Required**:

1. **Add Database Schema Migration** in `DatabaseMigrations.cpp`:
```cpp
// Add to migrate_to_version_2() or create migrate_to_version_3()
ErrorOr<void> DatabaseMigrations::migrate_to_version_3(SQL::Database& database)
{
    dbgln("Sentinel: Migrating PolicyGraph database to version 3");

    // The match_type column already exists, but ensure it supports new values
    // PolicyMatchType enum now includes:
    // 0 = DownloadOriginFileType
    // 1 = FormActionMismatch (NEW)
    // 2 = InsecureCredentialPost (NEW)
    // 3 = ThirdPartyFormPost (NEW)

    // The action column needs to support new values:
    // 0 = Allow
    // 1 = Block
    // 2 = Quarantine
    // 3 = BlockAutofill (NEW)
    // 4 = WarnUser (NEW)

    // No schema changes needed - existing INTEGER columns support these
    // Just update version number
    TRY(database.execute_statement("PRAGMA user_version = 3;"sv));

    dbgln("Sentinel: Migration to version 3 complete");
    return {};
}
```

2. **Add PolicyGraph Query Methods** in `PolicyGraph.cpp`:
```cpp
// Add after existing match_* methods (around line 450-680)

ErrorOr<Optional<Policy>> PolicyGraph::match_by_form_action(
    String const& form_origin,
    String const& action_origin)
{
    // Check cache first
    auto cache_key = String::formatted("form:{}:{}", form_origin, action_origin);
    if (auto cached = m_cache.get_cached(cache_key); cached.has_value())
        return cached.value();

    Optional<Policy> matched_policy;

    // Query database for form-related policies
    m_database->execute_statement(
        m_statements.match_by_form_action,
        [&](auto statement_id) {
            matched_policy = parse_policy_from_statement(*m_database, statement_id);
        },
        form_origin,
        action_origin,
        UnixDateTime::now().seconds_since_epoch()
    );

    // Cache result
    m_cache.cache_policy(cache_key, matched_policy.has_value()
        ? Optional<int>(matched_policy->id)
        : Optional<int>{});

    return matched_policy;
}

ErrorOr<Vector<Policy>> PolicyGraph::get_form_policies()
{
    Vector<Policy> policies;

    auto statement = TRY(m_database->prepare_statement(R"(
        SELECT * FROM policies
        WHERE match_type IN (1, 2, 3)
        ORDER BY created_at DESC;
    )"sv));

    m_database->execute_statement(statement, [&](auto statement_id) {
        policies.append(parse_policy_from_statement(*m_database, statement_id));
    });

    return policies;
}
```

3. **Add Prepared Statement** in `initialize_statements()`:
```cpp
// Around line 240-250 in PolicyGraph.cpp
m_statements.match_by_form_action = TRY(m_database->prepare_statement(R"(
    SELECT * FROM policies
    WHERE match_type IN (1, 2, 3)
      AND (url_pattern = ? OR url_pattern = ?)
      AND (expires_at IS NULL OR expires_at > ?)
    ORDER BY created_at DESC
    LIMIT 1;
)"sv));
```

4. **Add to PreparedStatements struct** in `PolicyGraph.h`:
```cpp
struct PreparedStatements {
    // ... existing statements ...
    int match_by_form_action;  // NEW
};
```

**Testing**:
```cpp
// Tests/Sentinel/TestPolicyGraphPhase6.cpp
TEST_CASE(test_create_form_block_policy)
{
    auto pg = PolicyGraph::create("/tmp/test_pg_phase6.db");

    PolicyGraph::Policy policy;
    policy.rule_name = "block_malicious_form"_string;
    policy.url_pattern = "https://fake-bank.com"_string;
    policy.action = PolicyGraph::PolicyAction::Block;
    policy.match_type = PolicyGraph::PolicyMatchType::FormActionMismatch;
    policy.enforcement_action = "block_form_submission"_string;
    policy.created_at = UnixDateTime::now();
    policy.created_by = "user"_string;

    auto result = pg.value()->create_policy(policy);
    EXPECT(!result.is_error());
    EXPECT_GT(result.value(), 0);
}

TEST_CASE(test_match_by_form_action)
{
    auto pg = PolicyGraph::create("/tmp/test_pg_phase6_match.db");

    // Create policy
    PolicyGraph::Policy policy;
    policy.rule_name = "block_cross_origin"_string;
    policy.url_pattern = "https://example.com"_string;
    policy.action = PolicyGraph::PolicyAction::WarnUser;
    policy.match_type = PolicyGraph::PolicyMatchType::FormActionMismatch;
    policy.created_at = UnixDateTime::now();
    policy.created_by = "flow_inspector"_string;

    pg.value()->create_policy(policy);

    // Match
    auto result = pg.value()->match_by_form_action(
        "https://example.com"_string,
        "https://malicious.com"_string
    );

    EXPECT(!result.is_error());
    EXPECT(result.value().has_value());
    EXPECT_EQ(result.value()->action, PolicyGraph::PolicyAction::WarnUser);
}
```

---

#### Task 7: Integration Testing + End-to-End Flow (CRITICAL)
**Time**: 1 hour
**File**: `Tests/Sentinel/TestPhase6Foundation.cpp` (NEW)

**Test Complete Flow**:
```cpp
#include <LibTest/TestCase.h>
#include <Services/WebContent/FormMonitor.h>
#include <Services/Sentinel/FlowInspector.h>
#include <Services/Sentinel/PolicyGraph.h>

TEST_CASE(test_end_to_end_credential_exfil_detection)
{
    // 1. FormMonitor detects suspicious form
    WebContent::FormMonitor form_monitor;

    WebContent::FormMonitor::FormSubmitEvent form_event;
    form_event.document_url = URL::URL("https://bank.com/login"sv);
    form_event.action_url = URL::URL("https://attacker.ru/steal"sv);
    form_event.has_password_field = true;
    form_event.method = "POST"_string;

    EXPECT(form_monitor.is_suspicious_submission(form_event));

    auto form_alert = form_monitor.analyze_submission(form_event);
    EXPECT(form_alert.has_value());

    // 2. FlowInspector analyzes event
    Sentinel::FlowInspector flow_inspector;

    Sentinel::FlowInspector::FormSubmissionEvent sentinel_event;
    sentinel_event.form_origin = form_alert->form_origin;
    sentinel_event.action_origin = form_alert->action_origin;
    sentinel_event.has_password_field = form_alert->has_password_field;
    sentinel_event.uses_https = form_alert->uses_https;
    sentinel_event.is_cross_origin = form_alert->is_cross_origin;

    auto flow_result = flow_inspector.analyze_form_submission(sentinel_event);
    EXPECT(!flow_result.is_error());
    EXPECT(flow_result.value().has_value());

    auto flow_alert = flow_result.value().value();
    EXPECT_EQ(flow_alert.alert_type,
              Sentinel::FlowInspector::AlertType::CredentialExfiltration);
    EXPECT_EQ(flow_alert.severity,
              Sentinel::FlowInspector::AlertSeverity::High);

    // 3. User creates policy to block
    auto pg = Sentinel::PolicyGraph::create("/tmp/test_e2e.db");

    Sentinel::PolicyGraph::Policy policy;
    policy.rule_name = "block_attacker_ru"_string;
    policy.url_pattern = "https://attacker.ru"_string;
    policy.action = Sentinel::PolicyGraph::PolicyAction::Block;
    policy.match_type = Sentinel::PolicyGraph::PolicyMatchType::FormActionMismatch;
    policy.created_at = UnixDateTime::now();
    policy.created_by = "user"_string;

    auto policy_result = pg.value()->create_policy(policy);
    EXPECT(!policy_result.is_error());

    // 4. Future submission blocked by policy
    auto match_result = pg.value()->match_by_form_action(
        "https://bank.com"_string,
        "https://attacker.ru"_string
    );

    EXPECT(!match_result.is_error());
    EXPECT(match_result.value().has_value());
    EXPECT_EQ(match_result.value()->action,
              Sentinel::PolicyGraph::PolicyAction::Block);

    dbgln("✓ End-to-end credential exfiltration detection flow complete");
}

TEST_CASE(test_trusted_form_learning)
{
    // 1. Initial submission triggers alert
    Sentinel::FlowInspector inspector;

    Sentinel::FlowInspector::FormSubmissionEvent event;
    event.form_origin = "https://example.com"_string;
    event.action_origin = "https://auth.example.com"_string;
    event.has_password_field = true;
    event.uses_https = true;
    event.is_cross_origin = true;

    auto result = inspector.analyze_form_submission(event);
    EXPECT(!result.is_error());
    EXPECT(result.value().has_value());  // Alert generated

    // 2. User marks as trusted
    auto learn_result = inspector.learn_trusted_relationship(
        "https://example.com"_string,
        "https://auth.example.com"_string
    );
    EXPECT(!learn_result.is_error());

    // 3. Future submissions no longer trigger alert
    auto result2 = inspector.analyze_form_submission(event);
    EXPECT(!result2.is_error());
    EXPECT(!result2.value().has_value());  // No alert

    dbgln("✓ Trusted form learning flow complete");
}
```

**Run Tests**:
```bash
cd Build/default
./bin/TestPhase6Foundation
```

---

### Day 35 Summary

**TODO Items Resolved**:
1. ✅ `ConnectionFromClient.cpp:1509` - Form submissions now sent to Sentinel via IPC
2. ✅ `ConnectionFromClient.cpp:1624` - Credential alerts forwarded to WebContent for UI display
3. ✅ `security.html:730,740` - Policies and threats tables fully implemented

**Components Completed**:
1. ✅ FormMonitor fully implemented with all detection methods
2. ✅ FlowInspector fully implemented with trusted relationship learning
3. ✅ PolicyGraph extended for Phase 6 policy types (BlockAutofill, WarnUser, form match types)
4. ✅ IPC messages defined and handlers implemented (form_submission_event, display_credential_exfil_alert)
5. ✅ SecurityTap extended to forward form events
6. ✅ about:security UI tables rendering

**Integration Testing**:
- ✅ End-to-end flow: FormMonitor → IPC → FlowInspector → PolicyGraph
- ✅ Trusted form learning workflow
- ✅ Cross-origin detection
- ✅ Insecure HTTP detection
- ✅ Policy matching and enforcement

**Phase 6 Readiness**:
- ✅ Foundation components operational
- ✅ IPC communication established
- ✅ Detection algorithms working
- ✅ Policy system extended
- ✅ UI integration points defined
- ✅ All TODO blockers resolved

**Next Steps (Phase 6 - Milestone 0.2)**:
- DOM integration for form submission hooks
- Full UI alert dialogs with user actions
- Autofill blocking system
- User education components
- Complete testing and documentation

---

## Success Criteria for Phase 5

### Day 29-30 Complete When:
- ✓ 4 critical security vulnerabilities fixed
- ✓ Integration tests passing
- ✓ ASAN/UBSAN clean
- ✓ Known issues documented

### Day 31 Complete When:
- ✓ LRU cache is O(1)
- ✓ Async scanning enabled
- ✓ Database indexed
- ✓ Performance benchmarked

### Day 32-35 Complete When:
- ✓ All tests passing (80%+ coverage)
- ✓ 0 critical security issues
- ✓ Phase 6 blockers implemented
- ✓ Configuration system complete

---

**Plan Version**: 1.0
**Created**: 2025-10-30
**Estimated Effort**: 40-50 hours across 7 days
