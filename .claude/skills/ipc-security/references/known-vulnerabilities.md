# IPC Vulnerability Catalog

Comprehensive reference of IPC vulnerabilities in multi-process browsers, their detection, and mitigation.

## Table of Contents
1. [Integer Overflow Vulnerabilities](#1-integer-overflow-vulnerabilities)
2. [Buffer Overflow Vulnerabilities](#2-buffer-overflow-vulnerabilities)
3. [Type Confusion Vulnerabilities](#3-type-confusion-vulnerabilities)
4. [Use-After-Free via IPC](#4-use-after-free-via-ipc)
5. [Privilege Escalation](#5-privilege-escalation)
6. [Resource Exhaustion](#6-resource-exhaustion)
7. [Injection Attacks](#7-injection-attacks)

---

## 1. Integer Overflow Vulnerabilities

### 1.1 Size Calculation Overflow

**CVE Reference:** Common pattern in browser IPC (CVE-2020-6418 variant)

**Description:** Attacker sends IPC message with large width/height values that overflow when multiplied, causing allocation of undersized buffer followed by out-of-bounds write.

**Attack Pattern:**
```cpp
// Vulnerable code
void handle_allocate_buffer(u32 width, u32 height) {
    u32 size = width * height;  // Overflow: 0x10000 * 0x10000 = 0
    auto buffer = allocate(size);  // Allocates 0 or small size
    fill_buffer(buffer, width * height);  // Writes beyond allocation
}
```

**Exploitation:**
- Send `width=0x10000, height=0x10000`
- Calculation wraps to 0 or small number
- Small buffer allocated
- Full data written, causing heap overflow
- Heap corruption leads to RCE

**Detection:**
- UBSAN: `signed-integer-overflow` or `unsigned-integer-overflow`
- ASAN: `heap-buffer-overflow` (if overflow causes subsequent buffer overflow)
- Fuzzing with large numeric values

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:254-266
ErrorOr<void> handle_allocate_buffer(u32 width, u32 height) {
    auto size = AK::checked_mul(width, height);
    if (size.has_overflow())
        return Error::from_string_literal("Size overflow");

    if (size.value() > MAX_ALLOCATION_SIZE)
        return Error::from_string_literal("Allocation too large");

    auto buffer = TRY(ByteBuffer::create_uninitialized(size.value()));
    return {};
}
```

**Prevention Checklist:**
- [ ] Use `AK::checked_add`, `checked_mul`, `checked_sub` for all size calculations
- [ ] Validate against maximum allocation limits (e.g., 100MB)
- [ ] Test with boundary values: `0xFFFFFFFF`, `0x7FFFFFFF`, `SIZE_MAX`
- [ ] Fuzz with integer overflow patterns

---

### 1.2 Index Calculation Overflow

**Description:** Array index calculation overflows, causing out-of-bounds access.

**Attack Pattern:**
```cpp
// Vulnerable code
u8 get_pixel(u32 x, u32 y, u32 stride) {
    u32 index = y * stride + x;  // May overflow
    return m_pixels[index];
}
```

**Exploitation:**
- Send `y=0xFFFFFFFF, stride=2, x=0`
- `0xFFFFFFFF * 2` overflows to `0xFFFFFFFE`
- Add `x` wraps to small index
- Out-of-bounds read at wrong location

**Mitigation:**
```cpp
ErrorOr<u8> get_pixel(u32 x, u32 y, u32 stride) {
    auto index_y = AK::checked_mul(y, stride);
    if (index_y.has_overflow())
        return Error::from_string_literal("Y overflow");

    auto index = AK::checked_add(index_y.value(), x);
    if (index.has_overflow())
        return Error::from_string_literal("Index overflow");

    if (index.value() >= m_pixels.size())
        return Error::from_string_literal("Out of bounds");

    return m_pixels[index.value()];
}
```

---

## 2. Buffer Overflow Vulnerabilities

### 2.1 Unchecked IPC String Copy

**CVE Reference:** CVE-2021-30551 pattern (V8 type confusion leading to buffer overflow)

**Description:** IPC message contains oversized string that overflows fixed-size buffer.

**Attack Pattern:**
```cpp
// Vulnerable code
void handle_set_name(Messages::SetName const& message) {
    char name[256];
    strcpy(name, message.name().characters());  // No bounds check
}
```

**Exploitation:**
- Send name with 1000 characters
- Stack buffer overflow
- Overwrite return address
- RCE

**Detection:**
- ASAN: `stack-buffer-overflow`
- Crash at return from function

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:242-252
void handle_set_name(Messages::SetName const& message) {
    if (!validate_string_length(message.name(), "name"sv))
        return;  // Reject oversized string

    // Use String instead of char[]
    m_name = message.name();
}

bool validate_string_length(StringView string, StringView field_name,
                            SourceLocation location = SourceLocation::current())
{
    if (string.length() > IPC::Limits::MaxStringLength) {
        dbgln("Security: Oversized {} ({} bytes, max {})",
              field_name, string.length(), IPC::Limits::MaxStringLength);
        track_validation_failure();
        return false;
    }
    return true;
}
```

**Prevention:**
- [ ] Never use `strcpy`, `sprintf`, fixed-size `char[]` for IPC strings
- [ ] Use `String`, `ByteString`, `StringView`
- [ ] Validate all string lengths against `IPC::Limits::MaxStringLength`
- [ ] Use `String::formatted()` instead of `sprintf`

---

### 2.2 Vector Size Mismatch

**Description:** IPC message declares size N but sends M elements, causing buffer overrun.

**Attack Pattern:**
```cpp
// Vulnerable code (generated IPC)
ErrorOr<Vector<u32>> decode_vector(Decoder& decoder) {
    size_t count = TRY(decoder.decode<size_t>());
    Vector<u32> vec;
    for (size_t i = 0; i < count; ++i) {
        vec.append(TRY(decoder.decode<u32>()));  // What if stream ends early?
    }
    return vec;
}
```

**Exploitation:**
- Send `count=1000000` but only 10 elements
- Decoder reads past end of message
- Information leak or crash

**Mitigation:**
```cpp
// Libraries/LibIPC/Decoder.h:63
ErrorOr<size_t> Decoder::decode_size()
{
    auto size = TRY(decode<u64>());
    if (size > NumericLimits<size_t>::max())
        return Error::from_string_literal("Size too large");
    return static_cast<size_t>(size);
}

// Validation in handler
ErrorOr<Vector<u32>> decode_vector(Decoder& decoder) {
    size_t count = TRY(decoder.decode_size());
    if (count > IPC::Limits::MaxVectorSize)  // 10000
        return Error::from_string_literal("Vector too large");

    Vector<u32> vec;
    TRY(vec.try_ensure_capacity(count));  // Pre-allocate
    for (size_t i = 0; i < count; ++i) {
        vec.unchecked_append(TRY(decoder.decode<u32>()));
    }
    return vec;
}
```

**Prevention:**
- [ ] Validate vector sizes before allocation
- [ ] Pre-allocate with `try_ensure_capacity`
- [ ] Use `IPC::Limits::MaxVectorSize` (10000)
- [ ] Validate stream has enough data

---

## 3. Type Confusion Vulnerabilities

### 3.1 IPC Message Type Confusion

**CVE Reference:** CVE-2021-30551 (V8 type confusion)

**Description:** Attacker sends message type A but receiver interprets as type B, accessing wrong fields.

**Attack Pattern:**
```cpp
// Vulnerable code (legacy IPC)
void handle_message(u32 message_id, void* data) {
    switch (message_id) {
    case MSG_SET_STRING:
        handle_set_string(*(SetStringMessage*)data);  // Wrong cast!
        break;
    case MSG_SET_NUMBER:
        handle_set_number(*(SetNumberMessage*)data);
        break;
    }
}

// Attacker sends MSG_SET_STRING with number data
// Type confusion: interprets number as string pointer
```

**Exploitation:**
- Send `MSG_SET_STRING` with controlled 64-bit number
- Number interpreted as pointer
- Arbitrary read/write

**Detection:**
- ASAN: `heap-buffer-overflow`, `use-after-free`
- UBSAN: `vptr` (virtual pointer corruption)
- Crashes when accessing "pointer"

**Mitigation (Ladybird):**
```cpp
// Generated IPC code uses type-safe decoding
// Services/WebContent/ConnectionFromClient.cpp (generated)
IPC::ResponseOr<void> handle_message(MessageType type, IPC::Decoder& decoder)
{
    switch (type) {
    case MessageType::LoadURL: {
        auto message = TRY(decoder.decode<Messages::WebContent::LoadURL>());
        return handle_load_url(message);
    }
    case MessageType::SetCookie: {
        auto message = TRY(decoder.decode<Messages::WebContent::SetCookie>());
        return handle_set_cookie(message);
    }
    default:
        return IPC::Error::InvalidMessage;
    }
}
```

**Prevention:**
- [ ] Use generated IPC code, not manual casting
- [ ] Each message type has unique struct
- [ ] Decoder validates message structure
- [ ] No `void*` or C-style casts

---

### 3.2 Enum Value Confusion

**Description:** IPC sends invalid enum value, causes incorrect behavior.

**Attack Pattern:**
```cpp
// Vulnerable code
enum class FileMode : u32 {
    Read = 0,
    Write = 1,
    ReadWrite = 2
};

void handle_open_file(FileMode mode, String path) {
    if (mode == FileMode::Write || mode == FileMode::ReadWrite) {
        // Check write permissions
        if (!has_write_permission(path))
            return;
    }

    int flags = mode_to_flags(mode);  // Converts enum to int
    open(path, flags);
}

// Attacker sends mode=255
// Bypasses write check, opens with arbitrary flags
```

**Mitigation:**
```cpp
ErrorOr<void> handle_open_file(FileMode mode, String path) {
    // Validate enum value
    if (mode != FileMode::Read &&
        mode != FileMode::Write &&
        mode != FileMode::ReadWrite) {
        return Error::from_string_literal("Invalid FileMode");
    }

    // Rest of function...
}
```

**Prevention:**
- [ ] Validate all enum values after decoding
- [ ] Use `switch` with explicit cases (no `default:` fallthrough)
- [ ] Consider using `Optional<Enum>` and checking `has_value()`

---

## 4. Use-After-Free via IPC

### 4.1 Object ID Reuse

**CVE Reference:** CVE-2020-6418 (Chromium IPC use-after-free)

**Description:** IPC references object by ID, object deleted, ID reused for different object, original reference accesses wrong object.

**Attack Pattern:**
```cpp
// Vulnerable code
class ObjectRegistry {
    HashMap<i32, OwnPtr<Object>> m_objects;

    void create_object(i32 id) {
        m_objects.set(id, make<Object>());
    }

    void delete_object(i32 id) {
        m_objects.remove(id);  // Removes entry
    }

    void use_object(i32 id) {
        auto* obj = m_objects.get(id).value();  // May be null!
        obj->do_something();  // Crash or use-after-free
    }
};

// Attack sequence:
// 1. create_object(5)
// 2. delete_object(5)
// 3. use_object(5)  // Use-after-free
```

**Exploitation:**
- Create object with ID 5
- Delete object 5
- Race: reuse ID 5 for different object
- Old reference accesses new object (type confusion + UAF)

**Detection:**
- ASAN: `heap-use-after-free`
- Crashes with freed memory pattern

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:197-206
bool validate_request_id(i32 request_id, SourceLocation location = SourceLocation::current())
{
    if (!m_active_requests.contains(request_id)) {
        dbgln("Security: Attempted access to invalid request_id {} at {}:{}",
              request_id, location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}

// Usage in handlers
void use_request(i32 request_id) {
    if (!validate_request_id(request_id))
        return;  // Reject invalid ID

    auto& request = *m_active_requests.get(request_id).value();
    request.do_something();
}
```

**Prevention:**
- [ ] Validate all object IDs before use
- [ ] Use `validate_request_id()` pattern
- [ ] Never assume ID exists
- [ ] Consider using generation counters (ID includes generation)

---

## 5. Privilege Escalation

### 5.1 Unrestricted File Access

**Description:** Sandboxed process requests file access, privileged process doesn't validate path.

**Attack Pattern:**
```cpp
// Vulnerable code in UI process
void handle_read_file(String path) {
    auto file = File::open(path, File::OpenMode::Read);
    send_file_contents(file->read_all());
}

// WebContent sends path="/etc/passwd"
// UI process reads and sends sensitive file
```

**Exploitation:**
- Request `/etc/passwd`, `/proc/self/maps`, etc.
- Read arbitrary files
- Information disclosure

**Mitigation:**
```cpp
void handle_read_file(String path) {
    // Validate path is in allowed list
    if (!m_allowed_paths.contains_slow(path))
        return;  // Reject

    // Additional checks
    if (is_sensitive_path(path))
        return;

    // Canonicalize to prevent directory traversal
    auto canonical_path = FileSystem::real_path(path);
    if (!canonical_path.has_value())
        return;

    // Check still in allowed directory
    if (!canonical_path.value().starts_with(m_download_directory))
        return;

    auto file = File::open(canonical_path.value(), File::OpenMode::Read);
    send_file_contents(file->read_all());
}
```

**Prevention:**
- [ ] Maintain allow-list of accessible paths
- [ ] Reject absolute paths from sandboxed processes
- [ ] Canonicalize paths to prevent `../` traversal
- [ ] Check file type (no devices, sockets, symlinks)
- [ ] Validate paths don't escape allowed directory

---

### 5.2 Capability Confusion

**Description:** Sandboxed process claims to have capability it doesn't, privileged process doesn't verify.

**Attack Pattern:**
```cpp
// Vulnerable code
void handle_execute_command(String command, bool is_privileged) {
    if (is_privileged) {
        system(command);  // Trusts is_privileged flag from untrusted source!
    } else {
        execute_sandboxed(command);
    }
}
```

**Mitigation:**
```cpp
void handle_execute_command(String command) {
    // Never trust capability flags from IPC
    // Always derive from internal state
    if (m_connection_privileges.has_execute_permission()) {
        system(command);
    } else {
        return;  // Reject
    }
}
```

**Prevention:**
- [ ] Never trust privilege/capability flags in IPC messages
- [ ] Derive permissions from connection identity
- [ ] Maintain privilege state server-side only

---

## 6. Resource Exhaustion

### 6.1 Memory Exhaustion via Large Allocations

**Description:** Attacker sends IPC messages requesting huge allocations, exhausting memory.

**Attack Pattern:**
```cpp
// Vulnerable code
void handle_allocate(size_t size) {
    auto buffer = ByteBuffer::create_uninitialized(size);
    m_buffers.append(move(buffer));
}

// Send allocate(1GB) x 100
// System runs out of memory
```

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

void handle_allocate(size_t size) {
    if (!validate_buffer_size(size, "allocation"sv))
        return;

    auto buffer = TRY(ByteBuffer::create_uninitialized(size));
    m_buffers.append(move(buffer));
}
```

**Prevention:**
- [ ] Enforce maximum allocation sizes (100MB)
- [ ] Track total memory allocated per connection
- [ ] Implement per-connection memory limits

---

### 6.2 Message Flooding DoS

**Description:** Attacker sends thousands of IPC messages per second, exhausting CPU/memory.

**Attack Pattern:**
```cpp
// Send 1,000,000 messages instantly
for (int i = 0; i < 1000000; ++i) {
    send_message(MSG_DUMMY);
}
```

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:323-332, 345
class ConnectionFromClient {
    IPC::RateLimiter m_rate_limiter { 1000, AK::Duration::from_milliseconds(10) };

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

    void handle_any_message() {
        if (!check_rate_limit())
            return;  // Drop message
        // Process message...
    }
};
```

**Prevention:**
- [ ] Implement rate limiting (1000 msg/sec in Ladybird)
- [ ] Disconnect on repeated violations
- [ ] Per-message-type rate limits for expensive operations

---

## 7. Injection Attacks

### 7.1 CRLF Injection in Headers

**Description:** Attacker injects `\r\n` in HTTP header values, adding malicious headers.

**Attack Pattern:**
```cpp
// Vulnerable code
void add_header(String name, String value) {
    m_headers += name + ": " + value + "\r\n";  // No validation
}

// Attacker sends value="foo\r\nX-Evil: bar"
// Results in:
//   Header: foo
//   X-Evil: bar
```

**Mitigation (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:281-309
bool validate_header_map(HTTP::HeaderMap const& headers,
                         SourceLocation location = SourceLocation::current())
{
    // Validate each header
    for (auto const& header : headers.headers()) {
        if (!validate_string_length(header.name, "header name"sv, location))
            return false;
        if (!validate_string_length(header.value, "header value"sv, location))
            return false;

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
```

**Prevention:**
- [ ] Reject header names/values containing `\r` or `\n`
- [ ] Validate before adding to HTTP request
- [ ] Use structured header APIs, not string concatenation

---

### 7.2 Command Injection

**Description:** IPC message contains shell metacharacters, executed by `system()`.

**Attack Pattern:**
```cpp
// Vulnerable code
void handle_convert_file(String filename) {
    String command = "convert " + filename + " output.png";
    system(command.characters());
}

// Attacker sends filename="; rm -rf /"
// Executes: convert ; rm -rf / output.png
```

**Mitigation:**
```cpp
void handle_convert_file(String filename) {
    // Option 1: Reject shell metacharacters
    if (filename.contains_any_of(";|&$`"sv))
        return;

    // Option 2: Use argument array, not system()
    Vector<String> args = { "convert", filename, "output.png" };
    Core::Process::spawn(args);
}
```

**Prevention:**
- [ ] Never pass IPC strings directly to `system()`
- [ ] Use `Core::Process::spawn()` with argument arrays
- [ ] If shell required, escape all metacharacters
- [ ] Validate against allowed character sets

---

## Vulnerability Discovery Techniques

### Fuzzing
```bash
# AFL++ IPC fuzzing
./Build/release/bin/FuzzIPCDecoder @@

# LibFuzzer with sanitizers
./Build/sanitizers/bin/FuzzIPCMessages -max_len=10000 -timeout=5
```

### Static Analysis
```bash
# Search for dangerous patterns
grep -r "system(" Services/
grep -r "strcpy\|sprintf" Services/
grep -r "operator\*" Services/ | grep -v VERIFY
```

### Manual Code Review Checklist
- [ ] All IPC handlers validate input sizes
- [ ] Integer arithmetic uses checked operations
- [ ] No `void*` casts or type confusion
- [ ] Object IDs validated before use
- [ ] File paths validated and canonicalized
- [ ] Rate limiting implemented
- [ ] No command injection vectors

---

## References

### CVEs
- **CVE-2021-30551**: Type confusion in V8 (Chrome)
- **CVE-2020-6418**: Use-after-free in audio (Chrome)
- **CVE-2019-5786**: FileReader UAF (Chrome)
- **CVE-2020-15999**: Integer overflow in FreeType (Chrome)

### Resources
- [Chromium IPC Security Tips](https://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc)
- [Firefox IPC Security](https://wiki.mozilla.org/Security/Reviews/Firefox/IPC)
- [Project Zero: Browser Exploitation](https://googleprojectzero.blogspot.com/2019/04/virtually-unlimited-memory-escaping.html)

### Ladybird References
- `Libraries/LibIPC/Limits.h` - IPC size limits
- `Services/RequestServer/ConnectionFromClient.h:197-348` - Validation patterns
- `.claude/skills/ipc-security/SKILL.md` - Security patterns
