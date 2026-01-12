---
name: ipc-security
description: Security patterns for Inter-Process Communication in Ladybird's multi-process architecture, including validation, rate limiting, and attack surface reduction
use-when: Implementing or reviewing IPC message handlers, designing new IPC interfaces, or conducting security audits
allowed-tools:
  - Read
  - Grep
  - Bash
tags:
  - security
  - ipc
  - validation
---

# IPC Security Patterns

## Threat Model
```
┌──────────────┐         ┌──────────────┐
│  WebContent  │  IPC    │  UI Process  │
│ (Sandboxed)  ├────────→│ (Privileged) │
└──────────────┘         └──────────────┘
     Untrusted               Trusted

Attack Vector: Malicious web content sends crafted IPC messages
Goal: Escape sandbox, execute code in UI process, access filesystem
```

## Security Principles

### 1. Validate Everything

**RULE: Never trust IPC input from sandboxed processes**
```cpp
// ✅ CORRECT - Comprehensive validation
IPC::ResponseOr<void> handle_set_cookie(Messages::WebContent::SetCookie const& message)
{
    // Size validation
    if (message.cookie_string().length() > MAX_COOKIE_SIZE)
        return IPC::Error::InvalidMessage;

    // Format validation
    auto cookie = Cookie::parse(message.cookie_string());
    if (!cookie.has_value())
        return IPC::Error::InvalidMessage;

    // Origin validation
    if (!origin_matches(message.origin(), cookie->domain()))
        return IPC::Error::PermissionDenied;

    // Security flags validation
    if (cookie->is_http_only() && !message.from_http())
        return IPC::Error::PermissionDenied;

    store_cookie(move(cookie.value()));
    return {};
}

// ❌ WRONG - No validation
IPC::ResponseOr<void> handle_set_cookie(Messages::WebContent::SetCookie const& message)
{
    store_cookie(message.cookie_string());
    return {};
}
```

### 2. Rate Limiting

**RULE: Prevent DoS through message flooding**
```cpp
class MessageRateLimiter {
public:
    bool check_and_increment(u32 message_type)
    {
        auto now = MonotonicTime::now();

        // Reset counters if window expired
        if (now - m_window_start > RATE_LIMIT_WINDOW) {
            m_counters.clear();
            m_window_start = now;
        }

        auto& count = m_counters.ensure(message_type);
        if (count >= MAX_MESSAGES_PER_WINDOW)
            return false;

        count++;
        return true;
    }

private:
    HashMap<u32, size_t> m_counters;
    MonotonicTime m_window_start;

    static constexpr auto RATE_LIMIT_WINDOW = Duration::from_seconds(1);
    static constexpr size_t MAX_MESSAGES_PER_WINDOW = 100;
};
```

### 3. Integer Overflow Protection
```cpp
// ✅ CORRECT - Check for overflow
ErrorOr<ByteBuffer> allocate_buffer(size_t size, size_t count)
{
    size_t total_size;
    if (__builtin_mul_overflow(size, count, &total_size))
        return Error::from_errno(EOVERFLOW);

    if (total_size > MAX_ALLOCATION_SIZE)
        return Error::from_string_literal("Allocation too large");

    return ByteBuffer::create_uninitialized(total_size);
}

// ❌ WRONG - Overflow can wrap to small number
ByteBuffer allocate_buffer(size_t size, size_t count)
{
    return ByteBuffer::create_uninitialized(size * count);
}
```

### 4. Privilege Checks
```cpp
// ✅ CORRECT - Verify caller privileges
IPC::ResponseOr<void> handle_write_file(Messages::WebContent::WriteFile const& message)
{
    // Check if sandboxed process has permission
    if (!m_allowed_paths.contains_slow(message.path()))
        return IPC::Error::PermissionDenied;

    // Additional checks for sensitive operations
    if (is_sensitive_path(message.path()))
        return IPC::Error::PermissionDenied;

    return write_file(message.path(), message.data());
}
```

### 5. Type Confusion Prevention
```cpp
// ✅ CORRECT - Type-safe IPC messages
enum class MessageType : u32 {
    LoadURL = 1,
    SetCookie = 2,
    // Explicit values, no reuse
};

// Use strong typing
IPC::ResponseOr<void> handle_message(MessageType type, IPC::Decoder& decoder)
{
    switch (type) {
    case MessageType::LoadURL:
        return handle_load_url(TRY(decoder.decode<Messages::WebContent::LoadURL>()));
    case MessageType::SetCookie:
        return handle_set_cookie(TRY(decoder.decode<Messages::WebContent::SetCookie>()));
    default:
        return IPC::Error::InvalidMessage;
    }
}
```

## Validation Checklist

For every IPC message handler:

- [ ] **Size limits**: Check all string/buffer lengths
- [ ] **Format validation**: Parse and validate format
- [ ] **Range checks**: Validate numeric ranges
- [ ] **Integer overflow**: Check arithmetic operations
- [ ] **Origin validation**: Verify caller identity/origin
- [ ] **Privilege checks**: Verify permission for operation
- [ ] **Rate limiting**: Prevent message flooding
- [ ] **Type safety**: Use strong types, no type confusion
- [ ] **Sanitization**: Sanitize strings for injection
- [ ] **Null checks**: Handle null/invalid references

## Attack Patterns to Watch For

### 1. Buffer Overflow
```cpp
// ❌ VULNERABLE
void handle_data(u8 const* data, size_t size) {
    u8 buffer[1024];
    memcpy(buffer, data, size); // No bounds check!
}
```

### 2. Integer Wraparound
```cpp
// ❌ VULNERABLE
size_t calculate_size(u32 width, u32 height) {
    return width * height; // Can overflow!
}
```

### 3. TOCTOU (Time-of-Check-Time-of-Use)
```cpp
// ❌ VULNERABLE
if (is_allowed(path)) {
    // File could change here!
    write_file(path, data);
}
```

### 4. Type Confusion
```cpp
// ❌ VULNERABLE
void* handle_message(u32 type_id) {
    switch (type_id) {
    case 1: return new ClassA();
    case 2: return new ClassB();
    }
}
// Caller might cast to wrong type
```

## Security Testing

### Fuzzing IPC Messages
```cpp
// Create fuzz target
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    IPC::Decoder decoder(data, size);

    // Try to decode and handle message
    auto message_type = decoder.decode<u32>();
    if (!message_type.has_value())
        return 0;

    handle_ipc_message(*message_type, decoder);
    return 0;
}
```

## References

- [Chromium IPC Security](https://www.chromium.org/Home/chromium-security/education/security-tips-for-ipc)
- CVE-2021-30551 - Type confusion in V8
- CVE-2020-6418 - Use-after-free in IPC

## Quick Reference

**Golden Rules:**
1. All inputs are malicious until validated
2. Check sizes before allocation/copy
3. Use ErrorOr<T> for all fallible operations
4. Rate limit all message types
5. Log suspicious activity

## Related Skills

### Complementary Skills
- **[fuzzing-workflow](../fuzzing-workflow/SKILL.md)**: Use fuzzing to test IPC handlers for security vulnerabilities. Combine IPC validation patterns with fuzzing for comprehensive security testing. Fuzz targets should exercise all IPC message types.
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug memory safety issues found in IPC handlers using ASAN/UBSAN. Reference sanitizer patterns when implementing IPC validation. Run sanitizer builds to catch buffer overflows and use-after-free in IPC code.

### Prerequisite Skills
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Understanding Ladybird's process architecture is essential before working with IPC security patterns. Know which processes communicate and what trust boundaries exist.
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Follow C++ coding patterns when implementing IPC handlers and validation logic. Use ErrorOr<T> for fallible operations and smart pointers for memory safety.

### Implementation Skills
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Compile IPC definitions with compile_ipc() macro. Add IPC endpoint files to build system using ladybird_generated_sources().
- **[ci-cd-patterns](../ci-cd-patterns/SKILL.md)**: Set up CI to run sanitizers on IPC handlers. Add IPC fuzzing to continuous testing pipeline.
