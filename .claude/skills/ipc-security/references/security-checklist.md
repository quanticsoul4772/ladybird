# IPC Security Review Checklist

Comprehensive checklists for implementing and reviewing IPC security in Ladybird's multi-process architecture.

## Table of Contents
1. [Pre-Implementation Checklist](#pre-implementation-checklist)
2. [Code Review Checklist](#code-review-checklist)
3. [Testing Checklist](#testing-checklist)
4. [Deployment Checklist](#deployment-checklist)
5. [Component-Specific Checklists](#component-specific-checklists)

---

## Pre-Implementation Checklist

Before implementing new IPC message handlers or interfaces:

### Threat Modeling
- [ ] **Identify trust boundaries**: Which processes are untrusted? (WebContent, ImageDecoder are sandboxed)
- [ ] **Map data flow**: Where does untrusted data enter trusted processes?
- [ ] **List assets**: What sensitive resources does this IPC access? (filesystem, network, memory)
- [ ] **Enumerate attack vectors**: How could malicious IPC messages exploit this?
- [ ] **Document assumptions**: What invariants must the IPC maintain?

### Design Review
- [ ] **Principle of least privilege**: Does IPC grant minimal necessary permissions?
- [ ] **Minimize attack surface**: Can we reduce number of messages or simplify handlers?
- [ ] **Synchronous vs asynchronous**: Use `=>` (sync) only when necessary, prefer `=|` (async)
- [ ] **Capability-based**: Does IPC check capabilities, not trust flags from caller?
- [ ] **Fail-safe defaults**: Do validation failures reject operations (not proceed)?

### IPC Definition Review
- [ ] **Message parameters validated**: Each parameter has size/range constraints documented
- [ ] **Strong typing**: Use enums, not `u32` for categorical values
- [ ] **Avoid `void*` or `ByteBuffer`**: Use structured types when possible
- [ ] **Version compatibility**: Can old clients work with new servers (forward compatibility)?

**Example .ipc file review:**
```cpp
// GOOD: Structured, typed parameters
start_request(i32 request_id, ByteString method, URL::URL url,
              HTTP::HeaderMap headers, ByteBuffer body,
              Core::ProxyData proxy_data, u64 page_id) =|

// BAD: Untyped, unvalidated
start_request(i32 id, ByteBuffer data) =|
```

---

## Code Review Checklist

For reviewing IPC message handler implementations:

### Input Validation (Critical)
- [ ] **Size limits enforced**: All strings, buffers, vectors validated against `IPC::Limits`
  - `MaxStringLength` = 10,000,000 (10MB)
  - `MaxVectorSize` = 10,000 elements
  - `MaxURLLength` = 10,000 chars
  - `MaxRequestBodySize` = 100MB
- [ ] **Integer overflow checks**: All arithmetic uses `AK::checked_add/mul/sub`
- [ ] **Enum validation**: All enum values validated against known set
- [ ] **Optional checks**: All `Optional<T>` checked with `has_value()` before access
- [ ] **Null pointer checks**: All pointers validated before dereference
- [ ] **Range validation**: Numeric values within acceptable ranges

**Code pattern to look for:**
```cpp
// GOOD: Comprehensive validation
void handle_message(Messages::Foo const& msg) {
    if (!validate_string_length(msg.name(), "name"sv))
        return;
    if (!validate_buffer_size(msg.data().size(), "data"sv))
        return;
    if (!validate_url(msg.url()))
        return;
    if (!check_rate_limit())
        return;
    // Process...
}

// BAD: No validation
void handle_message(Messages::Foo const& msg) {
    m_name = msg.name();  // Could be huge
    auto buffer = ByteBuffer::create_uninitialized(msg.size());  // Could overflow
}
```

### Error Handling
- [ ] **All operations return `ErrorOr<T>`**: No silent failures
- [ ] **TRY macro used**: Errors propagate correctly
- [ ] **Specific error messages**: Include field name, constraint violated
- [ ] **Logging**: Security violations logged with `dbgln("Security: ...")`
- [ ] **No unchecked casts**: Use `TRY(decoder.decode<T>())`, not C-style casts

### Resource Management
- [ ] **Smart pointers used**: `OwnPtr`, `RefPtr`, `NonnullRefPtr` (no raw `new`/`delete`)
- [ ] **RAII for cleanup**: Resources auto-freed on error paths
- [ ] **Leak prevention**: HashMap entries removed when objects deleted
- [ ] **Resource limits**: Per-connection limits enforced (memory, file descriptors)

### Security Validation
- [ ] **Object ID validation**: All IDs checked with `validate_request_id()` pattern
- [ ] **Path validation**: File paths canonicalized, checked against allow-list
- [ ] **Origin validation**: Cross-origin checks for sensitive operations
- [ ] **TOCTOU prevention**: No time-of-check-time-of-use races
- [ ] **Injection prevention**: No CRLF in headers, no shell metacharacters in commands

**Validation helper pattern (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:197-206
bool validate_request_id(i32 request_id, SourceLocation location = SourceLocation::current())
{
    if (!m_active_requests.contains(request_id)) {
        dbgln("Security: Invalid request_id {} at {}:{}",
              request_id, location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}
```

### Rate Limiting
- [ ] **Rate limiter present**: All IPC connections have rate limiter
- [ ] **Per-message checks**: Expensive operations have additional rate limits
- [ ] **Violation tracking**: Repeated violations disconnect client
- [ ] **Appropriate limits**: 1000 msg/sec general, lower for expensive ops

**Rate limiting pattern (Ladybird):**
```cpp
// Services/RequestServer/ConnectionFromClient.h:345
IPC::RateLimiter m_rate_limiter { 1000, AK::Duration::from_milliseconds(10) };

bool check_rate_limit(SourceLocation location = SourceLocation::current())
{
    if (!m_rate_limiter.try_consume()) {
        dbgln("Security: Rate limit exceeded at {}:{}", /* ... */);
        track_validation_failure();
        return false;
    }
    return true;
}
```

### Privilege Checks
- [ ] **Capability verification**: Permissions checked server-side, not from IPC flags
- [ ] **Path restrictions**: File access restricted to allowed directories
- [ ] **Network restrictions**: URL schemes validated (http/https/ipfs/ipns only)
- [ ] **Sandbox awareness**: Code assumes WebContent/ImageDecoder are hostile

---

## Testing Checklist

For testing IPC security:

### Unit Tests
- [ ] **Valid input test**: Handler accepts correct messages
- [ ] **Oversized string test**: Reject strings > `MaxStringLength`
- [ ] **Oversized vector test**: Reject vectors > `MaxVectorSize`
- [ ] **Integer overflow test**: Test boundary values (0xFFFFFFFF, SIZE_MAX)
- [ ] **Invalid enum test**: Reject enum values outside valid range
- [ ] **Invalid ID test**: Reject operations on non-existent IDs
- [ ] **Invalid path test**: Reject paths outside allowed directories
- [ ] **CRLF injection test**: Reject headers with `\r` or `\n`
- [ ] **Rate limit test**: Verify rate limiter blocks excessive messages

**Example test:**
```cpp
TEST_CASE(request_server_rejects_oversized_url)
{
    auto url_string = String::repeated('a', 20000);  // > MaxURLLength (10000)
    auto url = URL::URL(url_string);

    auto result = connection.start_request(1, "GET"sv, url, {}, {}, {}, 0);

    EXPECT(result.is_error());  // Should reject
}

TEST_CASE(request_server_rejects_invalid_request_id)
{
    auto result = connection.stop_request(999);  // Non-existent ID

    EXPECT_EQ(result, false);  // Should fail
}
```

### Fuzzing
- [ ] **IPC decoder fuzzing**: Fuzz `IPC::Decoder` with malformed messages
- [ ] **Message handler fuzzing**: Fuzz each handler with random inputs
- [ ] **AFL++ integration**: Run 24+ hours per handler
- [ ] **LibFuzzer with ASAN**: Catch memory errors during fuzzing
- [ ] **Boundary values**: Include max/min values in corpus

**Fuzzing setup:**
```bash
# Build with sanitizers
cmake --preset Sanitizers
cmake --build Build/sanitizers

# Fuzz IPC decoder
./Build/sanitizers/bin/FuzzIPCDecoder \
    -max_len=10000 \
    -timeout=5 \
    -jobs=4 \
    corpus/

# Check for crashes
ls crash-* timeout-*
```

### Integration Tests
- [ ] **Multi-process test**: Verify WebContent -> RequestServer -> UI IPC chain
- [ ] **Error propagation test**: Errors in child process reported to parent
- [ ] **Connection death test**: Parent handles child process crash
- [ ] **Resource cleanup test**: Resources freed when connection dies

### Sanitizer Testing
- [ ] **ASAN clean**: No heap-use-after-free, buffer-overflow, memory leaks
- [ ] **UBSAN clean**: No integer-overflow, null-pointer-dereference, undefined behavior
- [ ] **TSAN clean** (if multi-threaded): No data races

**Sanitizer testing:**
```bash
# Run with sanitizers
cmake --preset Sanitizers
cmake --build Build/sanitizers
cd Build/sanitizers && ctest

# Check specific service
./Build/sanitizers/bin/RequestServer
./Build/sanitizers/bin/WebContent
```

---

## Deployment Checklist

Before releasing IPC changes:

### Pre-Deployment
- [ ] **Code review complete**: At least 2 reviewers, 1 security-focused
- [ ] **All tests passing**: Unit, integration, fuzzing, sanitizers
- [ ] **Documentation updated**: `.ipc` file comments, security notes
- [ ] **Changelog entry**: Document security-relevant changes

### Performance Validation
- [ ] **Latency acceptable**: IPC validation doesn't add >10% overhead
- [ ] **Throughput acceptable**: Rate limits don't block legitimate usage
- [ ] **Memory overhead acceptable**: Validation structures don't bloat memory

### Security Validation
- [ ] **Threat model updated**: New attack vectors documented
- [ ] **Security review**: External review if major change
- [ ] **Fuzzing 24+ hours**: No crashes found
- [ ] **Penetration test**: Red team testing if available

### Rollout
- [ ] **Staged rollout**: Deploy to dev -> staging -> production
- [ ] **Monitoring**: Watch for validation failure logs, crashes
- [ ] **Rollback plan**: Can revert if issues found
- [ ] **Incident response**: Team knows how to respond to exploits

---

## Component-Specific Checklists

### WebContent Process (Sandboxed, Untrusted)

**Incoming IPC (from UI Process):**
- [ ] **URLs validated**: Schemes allowed, no file:// to sensitive paths
- [ ] **HTML sanitized**: No script injection in `load_html()`
- [ ] **Resource limits**: Image size, script execution time limited
- [ ] **User input validated**: Keyboard/mouse events within valid ranges

**Outgoing IPC (to UI Process):**
- [ ] **No sensitive data leaked**: Don't send raw memory contents
- [ ] **Reasonable requests**: Don't request 1GB downloads, 100k file opens
- [ ] **Rate limited**: Don't flood UI with alerts, file dialogs

### RequestServer Process (Privileged)

**Incoming IPC (from WebContent):**
- [ ] **URL validation**: Scheme, length, no SSRF targets (localhost, 169.254.x.x)
- [ ] **Header validation**: No CRLF injection, reasonable count/size
- [ ] **Body size limits**: Max 100MB request body
- [ ] **Rate limiting**: Max requests/sec per connection
- [ ] **Network isolation**: Per-page routing (Tor/proxy) enforced

**File Downloads:**
- [ ] **Path validation**: Save to allowed directories only
- [ ] **Filename sanitization**: No path traversal (`../`)
- [ ] **Size limits**: Max download size enforced
- [ ] **MIME validation**: Check Content-Type matches extension
- [ ] **Malware scanning**: Sentinel YARA scanning for executables

**Phishing/Security:**
- [ ] **URL analysis**: Phishing detection for downloads
- [ ] **SSL validation**: Certificate pinning, no invalid certs
- [ ] **Redirect limits**: Max 10 redirects

### ImageDecoder Process (Sandboxed, Untrusted)

**Incoming IPC:**
- [ ] **Image size limits**: Max dimensions (e.g., 10000x10000)
- [ ] **Format validation**: Only allowed formats (PNG, JPEG, GIF, WebP)
- [ ] **Buffer size validation**: Input buffer < 100MB

**Decoding:**
- [ ] **Integer overflow checks**: Width * height * BPP checked
- [ ] **Memory limits**: Max decoded size (e.g., 100MB)
- [ ] **Parser robustness**: Fuzzed with malformed images
- [ ] **Sandbox confinement**: No filesystem, network access

### UI Process (Privileged, Trusted)

**Incoming IPC (from WebContent/RequestServer):**
- [ ] **Alert limits**: Max alerts/sec to prevent spam
- [ ] **Dialog validation**: File dialogs restricted to safe directories
- [ ] **Clipboard access**: Limited clipboard read/write rates
- [ ] **Window operations**: Position/size within screen bounds

**Outgoing IPC:**
- [ ] **User consent**: Sensitive operations require user approval
- [ ] **Input sanitization**: User input to WebContent sanitized

---

## Quick Reference: Common Pitfalls

### Top 10 IPC Vulnerabilities in Ladybird-style Codebases

1. **Unchecked integer overflow** in size calculations
   - Fix: Use `AK::checked_mul()` everywhere
2. **Missing string length validation**
   - Fix: Always call `validate_string_length()`
3. **No rate limiting**
   - Fix: Add `IPC::RateLimiter` to all connections
4. **Trusting IPC flags** (is_privileged, can_execute)
   - Fix: Derive permissions from connection identity
5. **Object ID reuse** without validation
   - Fix: Use `validate_request_id()` pattern
6. **Path traversal** (`../../../etc/passwd`)
   - Fix: Canonicalize paths, check against allow-list
7. **TOCTOU races** (check-then-use)
   - Fix: Make operations atomic or use locks
8. **Type confusion** in message handling
   - Fix: Use generated IPC code, not manual casts
9. **Resource exhaustion** (memory, file descriptors)
   - Fix: Enforce per-connection resource limits
10. **Injection attacks** (CRLF, command injection)
    - Fix: Validate no metacharacters, use structured APIs

---

## Validation Code Templates

### Template: IPC Handler with Full Validation

```cpp
// Services/RequestServer/ConnectionFromClient.cpp
void ConnectionFromClient::handle_example_operation(
    i32 request_id,
    String name,
    URL::URL url,
    Vector<u32> data)
{
    // 1. Rate limiting
    if (!check_rate_limit())
        return;

    // 2. ID validation
    if (!validate_request_id(request_id))
        return;

    // 3. String validation
    if (!validate_string_length(name, "name"sv))
        return;

    // 4. URL validation
    if (!validate_url(url))
        return;

    // 5. Vector validation
    if (!validate_vector_size(data, "data"sv))
        return;

    // 6. Business logic validation
    if (name.is_empty()) {
        dbgln("Security: Empty name in example_operation");
        track_validation_failure();
        return;
    }

    // 7. Process operation
    auto& request = *m_active_requests.get(request_id).value();
    auto result = request.process(name, url, data);

    if (result.is_error()) {
        dbgln("Error processing operation: {}", result.error());
        return;
    }
}
```

### Template: Validation Helper Function

```cpp
template<typename T>
[[nodiscard]] bool validate_vector_size(
    Vector<T> const& vector,
    StringView field_name,
    SourceLocation location = SourceLocation::current())
{
    if (vector.size() > IPC::Limits::MaxVectorSize) {
        dbgln("Security: Oversized {} ({} elements, max {}) at {}:{}",
              field_name, vector.size(), IPC::Limits::MaxVectorSize,
              location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}
```

---

## Review Sign-off Template

```markdown
## IPC Security Review: [Feature Name]

**Reviewer:** [Name]
**Date:** [YYYY-MM-DD]
**Component:** [WebContent/RequestServer/ImageDecoder/UI]

### Pre-Implementation Checklist
- [x] Threat model reviewed
- [x] Design follows least privilege
- [x] IPC definition uses strong typing

### Code Review Checklist
- [x] All inputs validated (size, range, format)
- [x] Integer arithmetic uses checked operations
- [x] Rate limiting implemented
- [x] Object IDs validated
- [x] Error handling uses ErrorOr<T>

### Testing Checklist
- [x] Unit tests cover validation failures
- [x] Fuzzed for 24+ hours, no crashes
- [x] ASAN/UBSAN clean

### Issues Found
1. [Issue description] - Fixed in commit [hash]
2. [Issue description] - Fixed in commit [hash]

### Security Assessment
- **Risk Level:** Low / Medium / High
- **Exploitability:** Low / Medium / High
- **Impact:** Low / Medium / High

**Recommendation:** Approve / Approve with conditions / Reject

**Signature:** [Reviewer Name]
```

---

## References

- `Libraries/LibIPC/Limits.h` - Size limit constants
- `Services/RequestServer/ConnectionFromClient.h:197-348` - Validation pattern implementation
- `.claude/skills/ipc-security/SKILL.md` - Security principles and patterns
- `.claude/skills/ipc-security/references/known-vulnerabilities.md` - Vulnerability catalog
