# IPC Attack Surface Analysis

Comprehensive catalog of attack patterns targeting IPC in multi-process browsers, with examples and mitigations.

## Table of Contents
1. [Message Injection Attacks](#1-message-injection-attacks)
2. [Integer Overflow Attacks](#2-integer-overflow-attacks)
3. [TOCTOU Race Conditions](#3-toctou-race-conditions)
4. [Resource Exhaustion Attacks](#4-resource-exhaustion-attacks)
5. [Privilege Escalation](#5-privilege-escalation)
6. [Denial of Service](#6-denial-of-service)

---

## 1. Message Injection Attacks

### 1.1 CRLF Injection in HTTP Headers

**Attack Surface:** IPC messages containing HTTP headers (RequestServer)

**Threat Actor:** Malicious JavaScript in sandboxed WebContent process

**Attack Vector:**
```javascript
// Attacker-controlled JavaScript
fetch('https://example.com', {
    headers: {
        'X-Custom': 'value\r\nX-Injected: malicious\r\nHost: evil.com'
    }
});
```

**IPC Message Flow:**
```
WebContent (sandboxed) -> RequestServer (privileged)
    start_request(
        request_id=1,
        method="GET",
        url="https://example.com",
        headers={ "X-Custom": "value\r\nX-Injected: malicious\r\nHost: evil.com" }
    )
```

**Exploitation:**
1. WebContent sends header with embedded `\r\n`
2. RequestServer builds HTTP request without validation
3. Injected headers modify request behavior:
   ```
   GET / HTTP/1.1
   Host: example.com
   X-Custom: value
   X-Injected: malicious
   Host: evil.com
   ```
4. Duplicate `Host` header can redirect request to attacker's server
5. Response poisoning, session hijacking, or data exfiltration

**Impact:**
- HTTP request smuggling
- Cache poisoning
- Cross-site scripting via injected headers
- Authentication bypass

**Detection:**
- String contains `\r` (0x0D) or `\n` (0x0A)
- UBSAN: None (logical bug, not memory safety)
- Functional testing: Send request, verify headers in network log

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:281-309
bool validate_header_map(HTTP::HeaderMap const& headers,
                         SourceLocation location = SourceLocation::current())
{
    for (auto const& header : headers.headers()) {
        // Check for CRLF injection
        if (header.name.contains('\r') || header.name.contains('\n') ||
            header.value.contains('\r') || header.value.contains('\n')) {
            dbgln("Security: CRLF injection in header at {}:{}",
                  location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }
    }
    return true;
}

void ConnectionFromClient::start_request(/* ... */, HTTP::HeaderMap headers, /* ... */)
{
    if (!validate_header_map(headers))
        return;  // Reject request
    // Continue processing...
}
```

**Testing:**
```cpp
TEST_CASE(request_server_rejects_crlf_in_headers)
{
    HTTP::HeaderMap headers;
    headers.set("X-Test", "value\r\nX-Injected: evil");

    auto result = connection.start_request(1, "GET", url, headers, {}, {}, 0);

    // Should reject the request
    EXPECT(connection.m_validation_failures > 0);
}
```

---

### 1.2 Path Traversal in File Operations

**Attack Surface:** File path parameters in IPC (UI process file access)

**Threat Actor:** Compromised WebContent process

**Attack Vector:**
```javascript
// Malicious JavaScript triggers file picker, then modifies path in IPC
// (hypothetical attack on poorly-validated file access)
```

**IPC Message Flow:**
```
WebContent -> UI Process
    read_file(path="/home/user/Downloads/../../etc/passwd")
```

**Exploitation:**
1. WebContent requests file read with `../` in path
2. UI process doesn't canonicalize path
3. Path resolves outside allowed directory:
   ```
   /home/user/Downloads/../../etc/passwd
   -> /home/user/etc/passwd  (wrong)
   -> /etc/passwd             (correct resolution)
   ```
4. Attacker reads arbitrary files: `/etc/passwd`, `/proc/self/maps`, SSH keys

**Impact:**
- Information disclosure (passwords, keys, source code)
- Privacy violation (browser history, cookies)
- Sandbox escape preparation (ASLR bypass via `/proc/self/maps`)

**Detection:**
- Path contains `../` or `..\`
- Canonical path outside allowed directory
- Access to sensitive files logged

**Mitigation:**
```cpp
ErrorOr<String> validate_and_canonicalize_path(String const& path,
                                                String const& allowed_dir)
{
    // 1. Reject absolute paths from untrusted source
    if (path.starts_with('/')) {
        return Error::from_string_literal("Absolute paths not allowed");
    }

    // 2. Combine with allowed directory
    auto full_path = String::formatted("{}/{}", allowed_dir, path);

    // 3. Canonicalize (resolve .., symlinks)
    auto canonical = TRY(FileSystem::real_path(full_path));

    // 4. Verify still within allowed directory
    if (!canonical.starts_with(allowed_dir)) {
        return Error::from_string_literal("Path escapes allowed directory");
    }

    return canonical;
}

void handle_read_file(String path)
{
    auto canonical = TRY(validate_and_canonicalize_path(path, m_download_dir));

    // Additional checks
    auto stat = TRY(FileSystem::stat(canonical));
    if (!FileSystem::is_regular_file(stat))  // No devices, symlinks
        return Error::from_string_literal("Not a regular file");

    // Safe to read
    auto file = TRY(Core::File::open(canonical, Core::File::OpenMode::Read));
    return file->read_until_eof();
}
```

**Testing:**
```cpp
TEST_CASE(ui_process_rejects_path_traversal)
{
    auto result = ui_process.read_file("../../etc/passwd");
    EXPECT(result.is_error());

    result = ui_process.read_file("subdir/../../../etc/passwd");
    EXPECT(result.is_error());

    result = ui_process.read_file("/etc/passwd");  // Absolute path
    EXPECT(result.is_error());
}
```

---

### 1.3 SQL Injection in PolicyGraph

**Attack Surface:** Database queries in Sentinel PolicyGraph (if user input unsanitized)

**Threat Actor:** Malicious website domain used in query

**Attack Vector:**
```cpp
// Hypothetical vulnerable code (NOT in Ladybird - example only)
auto domain = "evil.com'; DROP TABLE policies; --";
auto query = String::formatted("SELECT * FROM policies WHERE domain = '{}'", domain);
// Results in: SELECT * FROM policies WHERE domain = 'evil.com'; DROP TABLE policies; --'
```

**Exploitation:**
1. Attacker crafts domain with SQL metacharacters
2. Query concatenates user input without escaping
3. Injected SQL executes: `DROP TABLE`, `UPDATE`, `SELECT` for exfiltration

**Impact:**
- Data destruction (DROP TABLE)
- Data modification (UPDATE to mark malware as safe)
- Information disclosure (SELECT to leak policies)

**Mitigation (Ladybird uses parameterized queries):**
```cpp
// Services/Sentinel/PolicyGraph.cpp (correct implementation)
auto stmt = TRY(m_db.prepare_statement(
    "SELECT action FROM policies WHERE domain = ? AND resource_type = ?"
));

stmt->bind_text(1, domain);  // Parameterized - safe
stmt->bind_text(2, resource_type);

return stmt->execute();  // SQL injection impossible
```

**Testing:**
```cpp
TEST_CASE(policy_graph_handles_sql_metacharacters)
{
    PolicyGraph graph;

    // Attempt SQL injection in domain
    auto evil_domain = "'; DROP TABLE policies; --";
    auto result = graph.create_policy(evil_domain, "download", "allow");

    EXPECT(!result.is_error());  // Should handle gracefully

    // Verify table not dropped
    auto policies = graph.list_policies();
    EXPECT(policies.has_value());
}
```

---

## 2. Integer Overflow Attacks

### 2.1 Allocation Size Overflow

**Attack Surface:** Buffer allocation based on IPC-provided dimensions

**Threat Actor:** Malicious WebContent or ImageDecoder

**Attack Vector:**
```javascript
// Attacker-controlled dimensions
var canvas = document.createElement('canvas');
canvas.width = 0x10000;   // 65536
canvas.height = 0x10000;  // 65536
// Requests 65536 * 65536 * 4 = 17,179,869,184 bytes (16GB)
// But u32 overflow: (u32)(0x10000 * 0x10000) = 0
```

**IPC Message Flow:**
```
WebContent -> ImageDecoder
    decode_image(width=0x10000, height=0x10000, format=RGBA)
```

**Exploitation:**
1. Send `width=0x10000, height=0x10000`
2. Vulnerable code: `size = width * height * 4`
   ```cpp
   u32 width = 0x10000;
   u32 height = 0x10000;
   u32 bytes_per_pixel = 4;
   u32 size = width * height * bytes_per_pixel;  // Overflow!
   // 0x10000 * 0x10000 = 0x100000000
   // Truncated to u32: 0x00000000
   ```
3. `size = 0` or small value after overflow
4. Allocate 0-byte or small buffer
5. Write `width * height * 4` bytes (actual large size)
6. Heap buffer overflow

**Impact:**
- Heap corruption
- Code execution (overwrite function pointers, vtables)
- Information disclosure (overwrite adjacent heap objects)

**Detection:**
- UBSAN: `unsigned-integer-overflow` or `signed-integer-overflow`
- ASAN: `heap-buffer-overflow` (when writing to undersized buffer)
- Crashes with specific width/height values

**Mitigation (Ladybird):**
```cpp
// Libraries/LibGfx/Bitmap.cpp (example pattern)
ErrorOr<NonnullRefPtr<Bitmap>> Bitmap::create(IntSize size, BitmapFormat format)
{
    // Check individual dimensions
    if (size.width() < 0 || size.height() < 0)
        return Error::from_string_literal("Negative dimensions");

    if (size.width() > 16384 || size.height() > 16384)
        return Error::from_string_literal("Dimensions too large");

    // Calculate size with overflow checking
    auto width_checked = AK::checked_cast<size_t>(size.width());
    auto height_checked = AK::checked_cast<size_t>(size.height());
    auto bpp_checked = AK::checked_cast<size_t>(bytes_per_pixel(format));

    auto size_per_row = AK::checked_mul(width_checked, bpp_checked);
    if (size_per_row.has_overflow())
        return Error::from_errno(EOVERFLOW);

    auto total_size = AK::checked_mul(size_per_row.value(), height_checked);
    if (total_size.has_overflow())
        return Error::from_errno(EOVERFLOW);

    // Additional limit check
    if (total_size.value() > 100 * 1024 * 1024)  // 100MB
        return Error::from_string_literal("Image too large");

    auto buffer = TRY(ByteBuffer::create_uninitialized(total_size.value()));
    return create_with_buffer(buffer, size, format);
}
```

**Testing:**
```cpp
TEST_CASE(image_decoder_rejects_overflow_dimensions)
{
    // Test u32 overflow boundary
    auto result = Bitmap::create({ 0x10000, 0x10000 }, BitmapFormat::RGBA8888);
    EXPECT(result.is_error());

    // Test near SIZE_MAX
    result = Bitmap::create({ 0x7FFFFFFF, 0x7FFFFFFF }, BitmapFormat::RGBA8888);
    EXPECT(result.is_error());

    // Test maximum allowed (should succeed)
    result = Bitmap::create({ 1024, 1024 }, BitmapFormat::RGBA8888);
    EXPECT(!result.is_error());
}
```

---

### 2.2 Vector Size Integer Overflow

**Attack Surface:** Vector allocation with IPC-provided count and element size

**Attack Vector:**
```
WebContent -> RequestServer
    allocate_buffers(count=0x1000000, size=0x1000)
```

**Exploitation:**
1. Send `count=0x1000000` (16M), `size=0x1000` (4KB)
2. Calculate: `total = count * size = 0x1000000 * 0x1000 = 0x1000000000`
3. Overflow to 0 (if u32) or wrap around
4. Allocate small vector, write 16M * 4KB

**Mitigation:**
```cpp
ErrorOr<Vector<ByteBuffer>> allocate_buffers(u32 count, u32 size)
{
    // Validate count
    if (count > IPC::Limits::MaxVectorSize)
        return Error::from_string_literal("Too many buffers");

    // Validate individual buffer size
    if (size > 100 * 1024 * 1024)  // 100MB
        return Error::from_string_literal("Buffer too large");

    // Validate total with overflow check
    auto total = AK::checked_mul<size_t>(count, size);
    if (total.has_overflow())
        return Error::from_errno(EOVERFLOW);

    if (total.value() > 1024 * 1024 * 1024)  // 1GB total
        return Error::from_string_literal("Total allocation too large");

    Vector<ByteBuffer> buffers;
    TRY(buffers.try_ensure_capacity(count));
    for (u32 i = 0; i < count; ++i) {
        buffers.unchecked_append(TRY(ByteBuffer::create_uninitialized(size)));
    }
    return buffers;
}
```

---

## 3. TOCTOU Race Conditions

### 3.1 File Permissions TOCTOU

**Attack Surface:** File operations with separate check and use

**Threat Actor:** Local attacker with filesystem access

**Attack Vector:**
```cpp
// Vulnerable code
if (has_permission(path)) {
    // RACE WINDOW: File could change here
    auto file = open(path, O_RDWR);
    write(file, data);
}
```

**Exploitation:**
1. Attacker creates benign file: `/tmp/safe_file`
2. Browser checks: `has_permission("/tmp/safe_file")` -> true
3. **Race window:** Attacker replaces with symlink: `/tmp/safe_file -> /etc/passwd`
4. Browser opens: `open("/tmp/safe_file")` -> opens `/etc/passwd`
5. Browser writes sensitive data to `/etc/passwd`

**Impact:**
- Write to arbitrary files (privilege escalation)
- Delete files by replacing with symlink to target
- Bypass access controls

**Detection:**
- Manual code review (look for check-then-use pattern)
- Difficult to reproduce (timing-dependent)
- TSAN: Won't detect (single-threaded race with external process)

**Mitigation:**
```cpp
// Option 1: Atomic open with O_NOFOLLOW
ErrorOr<OwnPtr<Core::File>> safe_open(String const& path)
{
    // Open with O_NOFOLLOW prevents symlink following
    int fd = ::open(path.characters(), O_RDWR | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return Error::from_errno(errno);

    // Check permissions on open file descriptor (not path)
    struct stat st;
    if (fstat(fd, &st) < 0) {
        ::close(fd);
        return Error::from_errno(errno);
    }

    if (!S_ISREG(st.st_mode)) {  // Only regular files
        ::close(fd);
        return Error::from_string_literal("Not a regular file");
    }

    // Check ownership, permissions on fd (atomic with open)
    if (!has_permission_on_fd(fd, &st)) {
        ::close(fd);
        return Error::from_string_literal("Permission denied");
    }

    return Core::File::adopt_fd(fd, Core::File::OpenMode::ReadWrite);
}

// Option 2: Use file descriptor operations only
ErrorOr<void> write_to_fd(int fd, ByteBuffer const& data)
{
    // fstat on fd, not path (no race)
    struct stat st;
    TRY(Core::System::fstat(fd, &st));

    if (!S_ISREG(st.st_mode))
        return Error::from_string_literal("Not a regular file");

    return Core::System::write(fd, data);
}
```

**Testing:**
```bash
# Race condition test script
#!/bin/bash
# Create benign file
echo "safe" > /tmp/safe_file

# Background loop: replace with symlink
while true; do
    rm -f /tmp/safe_file
    ln -s /etc/passwd /tmp/safe_file
    rm -f /tmp/safe_file
    echo "safe" > /tmp/safe_file
done &

# Run test
./test_file_operations  # Should never write to /etc/passwd
```

---

### 3.2 Object ID Reuse TOCTOU

**Attack Surface:** IPC operations on object IDs

**Attack Vector:**
```javascript
// Attacker JavaScript
var request_id = make_request("https://example.com");
cancel_request(request_id);
// Race: Make new request, might reuse ID
var request_id2 = make_request("https://evil.com");
// Old operation might act on new request
modify_request(request_id);  // Affects evil.com request?
```

**Exploitation:**
1. WebContent creates request (ID=5)
2. WebContent cancels request 5, ID freed
3. **Race:** WebContent creates new request 6, but RequestServer reuses ID 5
4. Old operation references ID 5, expecting original request
5. Operation acts on new request (wrong context)

**Impact:**
- Type confusion (different object types with same ID)
- Use-after-free (old object freed, ID reused)
- Data corruption (operation on wrong object)

**Mitigation:**
```cpp
// Use generation counters in IDs
class RequestIDGenerator {
    u32 m_next_id = 1;
    u32 m_generation = 0;

public:
    i64 generate_id() {
        if (m_next_id == 0xFFFFFFFF) {
            m_next_id = 1;
            m_generation++;
        }
        // ID = (generation << 32) | sequence
        return ((i64)m_generation << 32) | m_next_id++;
    }
};

// IDs never reused within same generation
// Operations on old ID fail (wrong generation)
```

**Testing:**
```cpp
TEST_CASE(request_id_not_reused)
{
    Vector<i32> used_ids;

    // Create and delete many requests
    for (int i = 0; i < 10000; ++i) {
        auto id = connection.create_request(/* ... */);
        EXPECT(!used_ids.contains_slow(id));  // Never reused
        used_ids.append(id);

        connection.delete_request(id);
    }
}
```

---

## 4. Resource Exhaustion Attacks

### 4.1 Memory Exhaustion via Large Allocations

**Attack Surface:** IPC requests for large buffers

**Attack Vector:**
```javascript
// Malicious JavaScript
for (let i = 0; i < 100; i++) {
    fetch('https://example.com/' + i, {
        body: new Uint8Array(100 * 1024 * 1024)  // 100MB each
    });
}
// Requests 10GB total
```

**IPC Message Flow:**
```
WebContent -> RequestServer (x100)
    start_request(body=100MB ByteBuffer)
```

**Exploitation:**
1. Send 100 requests with 100MB bodies each
2. RequestServer allocates 10GB total
3. System runs out of memory
4. Browser crashes or system becomes unresponsive

**Impact:**
- Denial of service (browser crash)
- System-wide DoS (OOM kills other processes)
- Swapping degradation (system slow)

**Detection:**
- Memory profiling shows large allocations
- OOM killer logs
- System monitoring (memory usage spike)

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:254-266
bool validate_buffer_size(size_t size, StringView field_name,
                          SourceLocation location = SourceLocation::current())
{
    static constexpr size_t MaxRequestBodySize = 100 * 1024 * 1024;  // 100MB
    if (size > MaxRequestBodySize) {
        dbgln("Security: Oversized {} ({} bytes, max {})",
              field_name, size, MaxRequestBodySize);
        track_validation_failure();
        return false;
    }
    return true;
}

// Per-connection memory limit
class ConnectionFromClient {
    size_t m_total_allocated = 0;
    static constexpr size_t MaxTotalAllocation = 500 * 1024 * 1024;  // 500MB

    ErrorOr<ByteBuffer> allocate(size_t size) {
        if (!validate_buffer_size(size, "allocation"sv))
            return Error::from_string_literal("Size too large");

        if (m_total_allocated + size > MaxTotalAllocation)
            return Error::from_string_literal("Connection allocation limit");

        auto buffer = TRY(ByteBuffer::create_uninitialized(size));
        m_total_allocated += size;
        return buffer;
    }

    void free(ByteBuffer& buffer) {
        m_total_allocated -= buffer.size();
    }
};
```

**Testing:**
```cpp
TEST_CASE(request_server_rejects_excessive_allocations)
{
    // Single large allocation
    auto result = connection.start_request(1, "POST", url, {},
                                           ByteBuffer::create(200 * 1024 * 1024),
                                           {}, 0);
    EXPECT(result.is_error());  // > 100MB limit

    // Multiple allocations exceeding total
    for (int i = 0; i < 10; ++i) {
        result = connection.start_request(i, "POST", url, {},
                                          ByteBuffer::create(60 * 1024 * 1024),
                                          {}, 0);
        if (i < 8)
            EXPECT(!result.is_error());  // Under 500MB
        else
            EXPECT(result.is_error());  // Over 500MB total
    }
}
```

---

### 4.2 File Descriptor Exhaustion

**Attack Surface:** IPC requests opening files or sockets

**Attack Vector:**
```javascript
// Open many connections
for (let i = 0; i < 10000; i++) {
    fetch('https://example.com/' + i);  // Each opens socket
}
```

**Exploitation:**
1. Send 10,000 requests
2. Each opens file descriptor (socket)
3. Process hits `ulimit -n` (typically 1024-4096)
4. Further file/socket opens fail
5. Browser can't open new tabs, files, etc.

**Impact:**
- Denial of service (browser can't open files)
- Crash (inability to allocate FDs)

**Mitigation:**
```cpp
class ConnectionFromClient {
    size_t m_open_file_count = 0;
    static constexpr size_t MaxOpenFiles = 100;

    ErrorOr<OwnPtr<Core::File>> open_file(String const& path) {
        if (m_open_file_count >= MaxOpenFiles)
            return Error::from_string_literal("Too many open files");

        auto file = TRY(Core::File::open(path, Core::File::OpenMode::Read));
        m_open_file_count++;
        return file;
    }

    void close_file(Core::File& file) {
        file.close();
        m_open_file_count--;
    }
};
```

---

## 5. Privilege Escalation

### 5.1 Sandbox Escape via File Write

**Attack Surface:** File write operations from sandboxed process

**Attack Vector:**
```javascript
// Malicious JavaScript attempts to write outside sandbox
save_file("/home/user/.bashrc", "malicious_code");
```

**Exploitation:**
1. WebContent requests write to `.bashrc`
2. UI process doesn't validate path
3. Attacker writes arbitrary commands
4. User's next shell session executes malicious code

**Impact:**
- Code execution outside sandbox
- Persistence (malware in startup files)
- Privilege escalation (if user is root)

**Mitigation:**
```cpp
// UI process handler
void handle_save_file(String path, ByteBuffer data)
{
    // Only allow writes to downloads directory
    auto canonical = validate_and_canonicalize_path(path, m_download_dir);
    if (canonical.is_error())
        return;  // Reject

    // Check for sensitive files
    if (is_sensitive_file(canonical.value()))
        return;  // Reject .bashrc, .ssh/*, etc.

    // Check file extension
    if (!is_allowed_extension(canonical.value()))
        return;  // Reject .sh, .exe, etc.

    // Safe to write
    auto file = TRY(Core::File::open(canonical.value(),
                                     Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(data));
}
```

---

### 5.2 Network Privilege Escalation

**Attack Surface:** Network requests to restricted URLs

**Attack Vector:**
```javascript
// SSRF attempt
fetch('http://127.0.0.1:8080/admin/delete_all_users');
fetch('http://169.254.169.254/latest/meta-data/iam/');  // AWS metadata
```

**Exploitation:**
1. WebContent requests localhost or metadata service
2. RequestServer doesn't validate target
3. Request to internal service (admin panel, cloud metadata)
4. Bypasses authentication (internal services trust localhost)

**Impact:**
- SSRF (Server-Side Request Forgery)
- Access to internal services
- Cloud credential theft (AWS/GCP metadata)

**Mitigation:**
```cpp
ErrorOr<void> validate_url_for_fetch(URL::URL const& url)
{
    // Validate scheme
    if (!url.scheme().is_one_of("http"sv, "https"sv))
        return Error::from_string_literal("Invalid scheme");

    // Block localhost
    auto host = url.host();
    if (host == "127.0.0.1" || host == "localhost" || host == "::1")
        return Error::from_string_literal("Localhost blocked");

    // Block private IP ranges
    if (host.starts_with("10.") ||
        host.starts_with("192.168.") ||
        host.starts_with("172.16.") ||  // TODO: Check full range
        host.starts_with("169.254."))  // Link-local, AWS metadata
        return Error::from_string_literal("Private IP blocked");

    return {};
}
```

---

## 6. Denial of Service

### 6.1 Message Flooding

**Attack Surface:** High rate of IPC messages

**Attack Vector:**
```javascript
// Send messages as fast as possible
while (true) {
    fetch('https://example.com');
}
```

**Exploitation:**
1. Tight loop sends IPC messages
2. Thousands per second
3. CPU exhaustion processing messages
4. Browser becomes unresponsive

**Impact:**
- CPU-based DoS
- UI freezes
- Other tabs slow/crash

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:323-332, 345
class ConnectionFromClient {
    IPC::RateLimiter m_rate_limiter { 1000, AK::Duration::from_milliseconds(10) };
    size_t m_validation_failures = 0;

    bool check_rate_limit(SourceLocation location = SourceLocation::current())
    {
        if (!m_rate_limiter.try_consume()) {
            dbgln("Security: Rate limit exceeded at {}:{}",
                  location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }
        return true;
    }

    void track_validation_failure()
    {
        m_validation_failures++;
        if (m_validation_failures >= 100) {
            dbgln("Security: Too many validation failures, disconnecting");
            die();  // Close connection
        }
    }

    void handle_any_message() {
        if (!check_rate_limit())
            return;  // Drop message
        // Process...
    }
};
```

**Testing:**
```cpp
TEST_CASE(request_server_rate_limits_messages)
{
    // Send 2000 messages rapidly
    int successful = 0;
    for (int i = 0; i < 2000; ++i) {
        auto result = connection.start_request(i, "GET", url, {}, {}, {}, 0);
        if (!result.is_error())
            successful++;
    }

    // Only ~1000 should succeed (rate limit)
    EXPECT(successful < 1200);
    EXPECT(successful > 900);  // Some tolerance for timing
}
```

---

### 6.2 Infinite Loop via Recursive Messages

**Attack Surface:** IPC messages triggering recursive processing

**Attack Vector:**
```javascript
// Trigger recursive rendering
document.body.innerHTML = "<div>".repeat(1000000) + "</div>".repeat(1000000);
```

**Exploitation:**
1. Deeply nested DOM structure
2. Triggers layout/paint recursion
3. Stack overflow or infinite loop
4. WebContent crashes

**Impact:**
- Tab crash
- CPU exhaustion

**Mitigation:**
```cpp
// LibWeb recursion depth limit
class LayoutTreeBuilder {
    static constexpr size_t MaxRecursionDepth = 512;

    ErrorOr<void> build_layout_tree(DOM::Node& node, size_t depth = 0)
    {
        if (depth > MaxRecursionDepth)
            return Error::from_string_literal("Recursion depth exceeded");

        for (auto* child = node.first_child(); child; child = child->next_sibling()) {
            TRY(build_layout_tree(*child, depth + 1));
        }
        return {};
    }
};
```

---

## Attack Surface Summary

| Attack Pattern | Severity | Affected Component | Primary Defense |
|----------------|----------|-------------------|-----------------|
| CRLF Injection | High | RequestServer | Reject `\r\n` in headers |
| Path Traversal | Critical | UI Process | Canonicalize paths, check allow-list |
| Integer Overflow | Critical | All | Checked arithmetic |
| TOCTOU Races | High | File operations | Atomic ops, fd-based checks |
| Memory Exhaustion | Medium | All | Per-connection limits |
| Message Flooding | Medium | All | Rate limiting |
| Sandbox Escape | Critical | UI Process | Path/capability validation |
| SSRF | High | RequestServer | Block private IPs |

---

## Defensive Coding Principles

1. **Validate everything**: All IPC input is hostile
2. **Fail closed**: Reject on validation failure, don't proceed
3. **Defense in depth**: Multiple validation layers
4. **Audit logging**: Log all security-relevant operations
5. **Rate limiting**: Prevent resource exhaustion
6. **Least privilege**: Grant minimal necessary permissions
7. **Atomic operations**: Avoid TOCTOU races
8. **Resource limits**: Per-connection memory, FD, message limits

---

## References

- `.claude/skills/ipc-security/SKILL.md` - IPC security patterns
- `.claude/skills/ipc-security/references/known-vulnerabilities.md` - CVE catalog
- `.claude/skills/ipc-security/references/security-checklist.md` - Review checklists
- `Services/RequestServer/ConnectionFromClient.h:197-348` - Validation implementation
