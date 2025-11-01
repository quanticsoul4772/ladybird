---
name: security-critical-changes
description: Workflow for security-critical changes (IPC, sandboxing, Sentinel)
applies_to:
  - IPC message handlers
  - Sandbox implementation
  - Sentinel security features
  - RequestServer SecurityTap
  - FormMonitor
  - FingerprintingDetector
  - PolicyGraph operations
  - Input validation and parsing
  - Cryptographic operations
agents:
  - security
  - fuzzer
  - reviewer
skills:
  - ipc-security
  - multi-process-architecture
  - fuzzing-workflow
  - memory-safety-debugging
---

# Security-Critical Changes Workflow

## Overview

This workflow is for changes that cross **security boundaries** in Ladybird's multi-process architecture. Security-critical code includes:

- **IPC message handlers** - Untrusted WebContent → Trusted RequestServer/ImageDecoder
- **Sandbox implementation** - Process isolation, capability restrictions
- **Sentinel security features** - Malware detection, credential protection, fingerprinting detection
- **Input validation** - Parsing untrusted data (URLs, files, user input)
- **Cryptographic operations** - TLS, certificate validation, hashing
- **Policy enforcement** - PolicyGraph, SecurityTap, FormMonitor decisions

**When to use this workflow**: Any change that handles untrusted input from web content, implements security controls, or enforces security policies.

## Prerequisites

Before starting security-critical work, ensure understanding of:

1. **Ladybird's Multi-Process Architecture**
   - UI Process (trusted)
   - WebContent Process (untrusted, sandboxed)
   - RequestServer Process (semi-trusted, network access)
   - ImageDecoder Process (untrusted, sandboxed)
   - Sentinel Service (trusted, security daemon)

2. **Trust Boundaries**
   - WebContent → RequestServer: **UNTRUSTED** → **TRUSTED** (validate everything!)
   - WebContent → ImageDecoder: **UNTRUSTED** → **UNTRUSTED** (still validate)
   - RequestServer → Sentinel: **TRUSTED** → **TRUSTED** (internal)
   - UI → RequestServer: **TRUSTED** → **TRUSTED** (internal)

3. **IPC Security Patterns**
   - Integer overflow checks (SafeMath)
   - String length validation
   - URL validation and canonicalization
   - Buffer size limits
   - Rate limiting

4. **Sentinel Architecture**
   - PolicyGraph for persistent security decisions
   - SecurityTap for download scanning
   - FormMonitor for credential exfiltration detection
   - FingerprintingDetector for browser fingerprinting

5. **Error Handling**
   - Use `ErrorOr<T>` for fallible operations
   - Use `TRY()` for error propagation
   - Never crash on malformed input
   - Graceful degradation on security feature failures

## Phase 1: Threat Modeling

### 1.1 Identify Attack Surface

**Questions to answer**:
- What untrusted data enters this code?
- From what source? (WebContent, network, user input, filesystem?)
- What operations are performed on untrusted data?
- What privileges does this code have? (network, filesystem, IPC?)
- What can go wrong if this code is exploited?

**Document**:
```
Attack Surface Analysis:
- Input source: [WebContent IPC / Network / Filesystem / User input]
- Data types: [URLs, buffers, integers, strings, files]
- Trust level: [Untrusted / Semi-trusted / Trusted]
- Privileges: [Network access, filesystem access, IPC to other processes]
- Impact if compromised: [RCE, info leak, DoS, bypass security policy]
```

### 1.2 List Potential Threats

Use STRIDE threat model:

- **Spoofing**: Can attacker impersonate another origin/user?
- **Tampering**: Can attacker modify data in transit/storage?
- **Repudiation**: Can attacker deny actions?
- **Information Disclosure**: Can attacker extract sensitive data?
- **Denial of Service**: Can attacker crash/hang the process?
- **Elevation of Privilege**: Can attacker gain more permissions?

**Example** (IPC message handler):
```
Threat: Integer overflow in buffer size parameter
STRIDE: Denial of Service (heap corruption → crash)
Impact: WebContent can crash RequestServer, DoS all tabs
Mitigation: Add SafeMath overflow checks
```

### 1.3 Document Trust Boundaries

Draw a diagram showing data flow across trust boundaries:

```
[WebContent (UNTRUSTED)]
        ↓ IPC: start_request(i32 request_id, ByteString method, URL::URL url, ...)
[RequestServer (TRUSTED)]
        ↓ Validate: method is valid HTTP verb, URL is well-formed
        ↓ Validate: request_id is in valid range
        ↓ Sanitize: URL canonicalization, scheme check
        ↓ Rate limit: Max 100 requests/second per WebContent
[Network (UNTRUSTED)]
```

**Trust Boundary Checklist**:
- [ ] Identified all untrusted input sources
- [ ] Documented data flow across trust boundaries
- [ ] Listed all validation/sanitization steps
- [ ] Identified privilege changes at each boundary

## Phase 2: Design Review

### 2.1 Security Design Patterns

Choose appropriate security patterns:

**Pattern 1: Fail-Safe Defaults**
```cpp
// Default to most restrictive policy
PolicyAction default_action = PolicyAction::Block;

// Only allow if explicitly whitelisted
if (auto policy = m_policy_graph->evaluate_policy(origin, resource)) {
    if (policy->action == PolicyAction::Allow)
        default_action = PolicyAction::Allow;
}
return default_action;
```

**Pattern 2: Complete Mediation**
```cpp
// Check permission on EVERY access, not just first
ErrorOr<void> RequestServer::handle_request(URL const& url) {
    // Check policy every time, not once at connection
    TRY(check_security_policy(url));

    // Perform request
    return fetch_resource(url);
}
```

**Pattern 3: Least Privilege**
```cpp
// Give minimum necessary permissions
// ImageDecoder doesn't need network access
// WebContent doesn't need direct filesystem access
// RequestServer doesn't need arbitrary IPC
```

**Pattern 4: Defense in Depth**
```cpp
// Multiple layers of validation
ErrorOr<void> SecurityTap::inspect_download(ReadonlyBytes content) {
    // Layer 1: Size check
    if (content.size() > MAX_SCAN_SIZE)
        return Error::from_string_literal("File too large");

    // Layer 2: YARA signature scan
    TRY(yara_scan(content));

    // Layer 3: ML-based malware detection
    TRY(ml_scan(content));

    // Layer 4: PolicyGraph check (user override?)
    TRY(check_user_policy(url));

    return {};
}
```

### 2.2 Input Validation Strategy

For each untrusted input, define validation rules:

**Integer Parameters**:
```cpp
// IPC: start_request(i32 request_id, ...)

// Validation rules:
// - request_id must be > 0
// - request_id must be < 2^31 (avoid overflow)
// - request_id must not already be in use

ErrorOr<void> validate_request_id(i32 request_id) {
    if (request_id <= 0)
        return Error::from_string_literal("Invalid request_id: must be positive");

    if (m_active_requests.contains(request_id))
        return Error::from_string_literal("Duplicate request_id");

    return {};
}
```

**String Parameters**:
```cpp
// IPC: set_dns_server(ByteString host_or_address, ...)

// Validation rules:
// - Length <= 256 bytes (prevent memory exhaustion)
// - Valid hostname/IP format
// - No null bytes (C string safety)
// - No control characters

ErrorOr<void> validate_hostname(ByteString const& host) {
    if (host.length() > 256)
        return Error::from_string_literal("Hostname too long");

    if (host.is_empty())
        return Error::from_string_literal("Empty hostname");

    // Check for null bytes
    if (host.contains('\0'))
        return Error::from_string_literal("Hostname contains null byte");

    // Validate hostname format (alphanumeric + dots + hyphens)
    for (char c : host) {
        if (!is_ascii_alphanumeric(c) && c != '.' && c != '-')
            return Error::from_string_literal("Invalid hostname character");
    }

    return {};
}
```

**Buffer Parameters**:
```cpp
// IPC: websocket_send(i64 websocket_id, bool is_text, ByteBuffer data)

// Validation rules:
// - Buffer size <= 10 MB (prevent memory exhaustion)
// - Buffer size matches declared size (no underflow/overflow)
// - Buffer not null if size > 0

ErrorOr<void> validate_websocket_data(ByteBuffer const& data) {
    constexpr size_t MAX_WEBSOCKET_MESSAGE_SIZE = 10 * 1024 * 1024; // 10 MB

    if (data.size() > MAX_WEBSOCKET_MESSAGE_SIZE)
        return Error::from_string_literal("WebSocket message too large");

    // Note: ByteBuffer is move-only, size is trusted
    return {};
}
```

**URL Parameters**:
```cpp
// IPC: start_request(..., URL::URL url, ...)

// Validation rules:
// - URL is well-formed (has scheme, host)
// - Scheme is allowed (http, https, wss, ipfs, etc.)
// - No file:// URLs from web content (local file access)
// - No javascript: URLs (XSS)

ErrorOr<void> validate_request_url(URL::URL const& url) {
    // Check for valid scheme
    if (!url.scheme().is_one_of("http"sv, "https"sv, "ws"sv, "wss"sv))
        return Error::from_string_literal("Invalid URL scheme");

    // Ensure host is present
    if (url.host().is_empty())
        return Error::from_string_literal("URL missing host");

    // Block dangerous schemes
    if (url.scheme().is_one_of("file"sv, "javascript"sv, "data"sv))
        return Error::from_string_literal("Blocked URL scheme");

    return {};
}
```

### 2.3 Error Handling Approach

**Never crash on invalid input**:
```cpp
// BAD: Assertion on untrusted input
void handle_request(i32 request_id) {
    VERIFY(request_id > 0); // WRONG! WebContent can send negative ID
}

// GOOD: Return error on invalid input
ErrorOr<void> handle_request(i32 request_id) {
    if (request_id <= 0)
        return Error::from_string_literal("Invalid request_id");
    // Continue processing...
}
```

**Graceful degradation for security features**:
```cpp
// Sentinel features should never break core browser functionality
ErrorOr<void> RequestServer::start_download(URL const& url) {
    // Download the file (core functionality)
    auto file_data = TRY(fetch_resource(url));

    // Attempt malware scan (security feature)
    if (m_security_tap) {
        auto scan_result = m_security_tap->inspect_download(metadata, file_data);
        if (scan_result.is_error()) {
            dbgln("Warning: Malware scan failed: {}", scan_result.error());
            // Continue with download - don't block user
        } else if (scan_result.value().is_threat) {
            // Quarantine file
            quarantine_file(file_data);
            return Error::from_string_literal("File blocked by malware scanner");
        }
    }

    return save_file(file_data);
}
```

## Phase 3: Implementation

### 3.1 Secure Coding Patterns

**Use ErrorOr<T> for fallible operations**:
```cpp
// Function that can fail
ErrorOr<NonnullOwnPtr<SecurityTap>> SecurityTap::create() {
    auto socket = TRY(Core::Socket::connect(SENTINEL_SOCKET_PATH));
    return adopt_nonnull_own(new SecurityTap(move(socket)));
}

// Caller uses TRY to propagate errors
ErrorOr<void> RequestServer::init() {
    m_security_tap = TRY(SecurityTap::create());
    return {};
}
```

**Integer overflow protection**:
```cpp
#include <AK/SafeMath.h>

ErrorOr<void> allocate_buffer(size_t count, size_t element_size) {
    // Check for overflow before allocation
    Checked<size_t> total_size = count;
    total_size *= element_size;

    if (total_size.has_overflow())
        return Error::from_string_literal("Integer overflow in buffer size");

    auto buffer = TRY(ByteBuffer::create_uninitialized(total_size.value()));
    return {};
}
```

**Memory safety with AK containers**:
```cpp
// Use Vector, not raw pointers
Vector<Request> m_active_requests; // GOOD

// Use NonnullOwnPtr for ownership
NonnullOwnPtr<SecurityTap> m_security_tap; // GOOD

// Use RefPtr for shared ownership
RefPtr<PolicyGraph> m_policy_graph; // GOOD

// Avoid raw pointers
Request* m_requests; // BAD
```

**String safety**:
```cpp
// Use StringView for non-owning references (zero-copy)
void process_api_name(StringView api_name) {
    // No allocation, just view into existing string
}

// Use String for owned strings
String m_cached_url;

// Use ByteString for binary data / legacy APIs
ByteString m_file_hash;

// String literals use "text"sv suffix
if (scheme == "https"sv) { ... }
```

### 3.2 Input Validation (IPC Messages)

**Example: Adding IPC message handler**

```cpp
// Services/RequestServer/RequestServer.ipc
endpoint RequestServer {
    // NEW: Enforce security policy on request
    enforce_security_policy(i32 request_id, ByteString action) =|
}
```

**Implementation**:
```cpp
// Services/RequestServer/ConnectionFromClient.cpp

void ConnectionFromClient::enforce_security_policy(i32 request_id, ByteString action)
{
    // Phase 1: Validate request_id
    if (request_id <= 0) {
        dbgln("Invalid request_id: {}", request_id);
        return; // Fail silently - don't crash
    }

    // Phase 2: Check if request exists
    auto request = m_active_requests.get(request_id);
    if (!request.has_value()) {
        dbgln("Unknown request_id: {}", request_id);
        return;
    }

    // Phase 3: Validate action string
    if (action.length() > 64) {
        dbgln("Action string too long: {} bytes", action.length());
        return;
    }

    if (!action.is_one_of("allow"sv, "block"sv, "quarantine"sv)) {
        dbgln("Invalid action: {}", action);
        return;
    }

    // Phase 4: Enforce policy via PolicyGraph
    if (m_policy_graph) {
        auto result = m_policy_graph->enforce_policy(request.value(), action);
        if (result.is_error()) {
            dbgln("Failed to enforce policy: {}", result.error());
        }
    }
}
```

**Validation checklist**:
- [ ] Integer parameters checked for valid range
- [ ] String parameters checked for max length
- [ ] String parameters checked for null bytes
- [ ] Enum parameters validated against allowed values
- [ ] Buffer sizes checked for overflow
- [ ] URLs validated for allowed schemes
- [ ] Resource IDs checked for existence
- [ ] Rate limiting applied (if high-frequency message)

### 3.3 Integer Overflow Protection

**SafeMath for arithmetic**:
```cpp
#include <AK/SafeMath.h>

ErrorOr<size_t> calculate_total_size(size_t num_items, size_t item_size) {
    Checked<size_t> total = num_items;
    total *= item_size;

    if (total.has_overflow())
        return Error::from_string_literal("Size calculation overflow");

    return total.value();
}
```

**Common overflow scenarios**:
```cpp
// Scenario 1: Buffer allocation
// WebContent sends: num_elements = 1,000,000,000, element_size = 1,000
// Without check: 1B * 1K = overflow to small value → small buffer allocated
// With SafeMath: Detects overflow, returns error

// Scenario 2: Array indexing
// WebContent sends: index = 0x7FFFFFFF, offset = 0x7FFFFFFF
// Without check: index + offset = overflow to negative → out-of-bounds read
// With SafeMath: Detects overflow, returns error

// Scenario 3: Length calculation
// WebContent sends: start = 100, end = 50
// Without check: length = end - start = underflow to huge positive → crash
// With validation: Check start <= end before subtraction
```

### 3.4 Memory Safety

**Use RAII for resource management**:
```cpp
class FileScanner {
public:
    static ErrorOr<NonnullOwnPtr<FileScanner>> create() {
        auto fd = TRY(open_file());
        return adopt_nonnull_own(new FileScanner(move(fd)));
    }

    ~FileScanner() {
        // Automatically closes fd
    }

private:
    FileScanner(NonnullOwnPtr<Core::File> file)
        : m_file(move(file)) {}

    NonnullOwnPtr<Core::File> m_file;
};
```

**Avoid use-after-free**:
```cpp
// BAD: Storing raw pointer
void PolicyGraph::cache_policy(Policy const* policy) {
    m_cache = policy; // Dangling pointer if Policy deleted!
}

// GOOD: Store by value or RefPtr
void PolicyGraph::cache_policy(Policy policy) {
    m_cache = move(policy); // Copy, safe
}
```

**Vector bounds checking**:
```cpp
// BAD: Unchecked index
auto item = vector[index]; // Crashes if index >= size

// GOOD: Checked access
auto item = vector.at(index); // VERIFY() in debug, undefined in release
if (index < vector.size())
    auto item = vector[index]; // Safe
```

## Phase 4: Testing

### 4.1 Unit Tests for Validation

**Test valid inputs**:
```cpp
TEST_CASE(valid_request_id)
{
    auto result = validate_request_id(42);
    EXPECT(!result.is_error());
}
```

**Test invalid inputs**:
```cpp
TEST_CASE(negative_request_id)
{
    auto result = validate_request_id(-1);
    EXPECT(result.is_error());
}

TEST_CASE(zero_request_id)
{
    auto result = validate_request_id(0);
    EXPECT(result.is_error());
}

TEST_CASE(max_request_id)
{
    auto result = validate_request_id(INT32_MAX);
    EXPECT(!result.is_error());
}
```

**Test boundary conditions**:
```cpp
TEST_CASE(hostname_max_length)
{
    // Valid: exactly 256 characters
    ByteString valid_hostname = ByteString::repeated('a', 256);
    EXPECT(!validate_hostname(valid_hostname).is_error());

    // Invalid: 257 characters
    ByteString too_long = ByteString::repeated('a', 257);
    EXPECT(validate_hostname(too_long).is_error());
}

TEST_CASE(hostname_empty)
{
    ByteString empty = "";
    EXPECT(validate_hostname(empty).is_error());
}
```

**Test integer overflow**:
```cpp
TEST_CASE(buffer_size_overflow)
{
    // This should overflow size_t on 32-bit systems
    size_t huge_count = SIZE_MAX;
    size_t element_size = 2;

    auto result = calculate_total_size(huge_count, element_size);
    EXPECT(result.is_error());
}
```

### 4.2 Fuzzing Integration

**Create fuzz target for IPC handler**:
```cpp
// Services/RequestServer/Fuzz/FuzzIPCMessages.cpp

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size)
{
    // Parse fuzzer input as IPC message parameters
    if (size < 12)
        return 0;

    i32 request_id = *reinterpret_cast<i32 const*>(data);
    ByteString method(reinterpret_cast<char const*>(data + 4), 8);

    // Call IPC handler with fuzzed input
    auto server = RequestServer::ConnectionFromClient::create();
    server->handle_start_request(request_id, method, /* ... */);

    return 0;
}
```

**Run fuzzer**:
```bash
# Build with fuzzing support
cmake --preset Sanitizers -DENABLE_FUZZING=ON
cmake --build Build/sanitizers

# Run fuzzer
./Build/sanitizers/Fuzz/RequestServer/FuzzIPCMessages \
    -max_len=10000 \
    -timeout=10 \
    -runs=1000000

# With AFL++
afl-fuzz -i corpus/ -o findings/ -- ./FuzzIPCMessages @@
```

**Fuzz testing checklist**:
- [ ] Fuzz target created for IPC handler
- [ ] Fuzz with ASan enabled
- [ ] Fuzz with UBSan enabled
- [ ] Fuzz corpus includes valid inputs
- [ ] Fuzz corpus includes edge cases (empty, max size, etc.)
- [ ] Run for at least 1 million inputs

### 4.3 Sanitizer Testing

**Build with sanitizers**:
```bash
cmake --preset Sanitizers
cmake --build Build/sanitizers
```

**Run tests with sanitizers**:
```bash
cd Build/sanitizers
ctest

# Or specific test
./bin/TestPolicyGraph
./bin/TestFingerprintingDetector
```

**Manual testing with sanitizers**:
```bash
# Run Ladybird with ASan/UBSan
./Build/sanitizers/bin/Ladybird

# Navigate to test pages
# - Credential form submission test
# - Canvas fingerprinting test
# - Malware download test

# Check for sanitizer reports
```

**Common sanitizer findings**:
- **Use-after-free**: Accessing deleted memory
- **Heap buffer overflow**: Writing past allocated buffer
- **Stack buffer overflow**: Writing past stack array
- **Integer overflow**: Signed/unsigned overflow
- **Uninitialized memory**: Reading uninitialized variable
- **Memory leak**: Allocated memory not freed

### 4.4 Integration Tests

**Test IPC message flow**:
```cpp
// Tests/RequestServer/TestIPC.cpp

TEST_CASE(security_policy_enforcement)
{
    // 1. Set up RequestServer
    auto server = TRY_OR_FAIL(RequestServer::create());

    // 2. Send IPC message with valid parameters
    server->enforce_security_policy(42, "block"sv);

    // 3. Verify policy was enforced
    auto policy = server->policy_graph()->get_policy(42);
    EXPECT(policy.has_value());
    EXPECT_EQ(policy->action, PolicyAction::Block);
}

TEST_CASE(invalid_request_id_handling)
{
    auto server = TRY_OR_FAIL(RequestServer::create());

    // Send invalid request_id - should not crash
    server->enforce_security_policy(-1, "block"sv);
    server->enforce_security_policy(0, "block"sv);

    // Verify no policy created for invalid IDs
    EXPECT(!server->policy_graph()->get_policy(-1).has_value());
}
```

**Test Sentinel feature integration**:
```bash
# Test credential protection
./Meta/ladybird.py run test-web -- -f Text/input/credential-protection-basic.html
./Meta/ladybird.py run test-web -- -f Text/input/credential-protection-cross-origin.html

# Test fingerprinting detection
./Build/release/bin/Ladybird
# Navigate to: https://browserleaks.com/canvas
# Verify alert appears in console

# Test malware scanning
./Build/release/bin/Ladybird
# Download EICAR test file: http://www.eicar.org/download/eicar.com
# Verify quarantine alert appears
```

## Phase 5: Security Review

### 5.1 Self-Review Checklist

Before submitting code for review, verify:

**Input Validation**:
- [ ] All IPC parameters validated
- [ ] String lengths checked (<= reasonable max)
- [ ] Integer ranges checked (no negative sizes)
- [ ] URLs validated (scheme, host present)
- [ ] Buffer sizes checked for overflow
- [ ] No assumptions about input format

**Error Handling**:
- [ ] All fallible operations use `ErrorOr<T>`
- [ ] Errors propagated with `TRY()`
- [ ] No `VERIFY()` on untrusted input
- [ ] No crashes on malformed input
- [ ] Graceful degradation for security features

**Memory Safety**:
- [ ] No raw pointers for ownership
- [ ] Use `NonnullOwnPtr`/`RefPtr` for ownership
- [ ] Use `StringView` for non-owning string refs
- [ ] Vector/HashMap used instead of arrays
- [ ] No manual memory management (new/delete)

**Integer Safety**:
- [ ] Use `Checked<T>` for overflow-prone arithmetic
- [ ] Check for overflow before allocation
- [ ] Check for underflow in length calculations
- [ ] Use appropriate types (size_t for sizes, not int)

**Testing**:
- [ ] Unit tests for valid inputs
- [ ] Unit tests for invalid inputs
- [ ] Unit tests for boundary conditions
- [ ] Fuzz target created (if IPC/parsing)
- [ ] Sanitizer testing performed (no issues)

### 5.2 Peer Review Process

**Reviewer focus areas**:

1. **Trust boundary analysis**
   - Is untrusted data properly validated?
   - Are assumptions about input format documented?
   - Is validation complete (all parameters checked)?

2. **Privilege escalation**
   - Can WebContent gain more privileges?
   - Can attacker access resources they shouldn't?
   - Are file paths properly sandboxed?

3. **Denial of service**
   - Can attacker cause crash/hang?
   - Are resource limits enforced (memory, CPU)?
   - Is there rate limiting on expensive operations?

4. **Information disclosure**
   - Can attacker extract sensitive data?
   - Are error messages too verbose?
   - Is timing information leaked?

### 5.3 Security-Specific Checks

**IPC Message Handler Review**:
```cpp
// Template for reviewing IPC handlers

void ConnectionFromClient::handle_message(Params...)
{
    // 1. VALIDATE ALL PARAMETERS
    // Q: Are all parameters validated?
    // Q: Are string lengths checked?
    // Q: Are integer ranges checked?
    // Q: Are enums validated?

    // 2. CHECK RESOURCE EXISTENCE
    // Q: Does the resource ID exist?
    // Q: Does caller have permission to access it?

    // 3. PREVENT INTEGER OVERFLOW
    // Q: Are arithmetic operations checked for overflow?
    // Q: Are buffer allocations protected?

    // 4. PREVENT RESOURCE EXHAUSTION
    // Q: Is there a limit on memory usage?
    // Q: Is there a limit on CPU time?
    // Q: Is there rate limiting?

    // 5. ERROR HANDLING
    // Q: Are errors handled gracefully?
    // Q: Does invalid input cause crash?
    // Q: Are error messages too verbose?
}
```

**PolicyGraph Operation Review**:
```cpp
// Template for reviewing PolicyGraph operations

ErrorOr<Policy> PolicyGraph::evaluate_policy(...)
{
    // 1. SQL INJECTION PREVENTION
    // Q: Are all SQL queries parameterized?
    // Q: Are user strings escaped?

    // 2. CACHE POISONING
    // Q: Are cache keys unique?
    // Q: Can attacker pollute cache?

    // 3. RACE CONDITIONS
    // Q: Are database operations atomic?
    // Q: Is there proper locking?

    // 4. POLICY BYPASS
    // Q: Is default policy secure (fail-closed)?
    // Q: Can attacker bypass policy check?
}
```

## Phase 6: Documentation

### 6.1 Security Implications

Document security properties of your change:

```markdown
## Security Implications

### Threat Model
This IPC handler receives untrusted input from WebContent process and
enforces security policies in RequestServer (trusted process).

### Mitigations
1. **Input Validation**: All parameters validated before use
   - request_id checked for positive range
   - action string validated against enum (allow/block/quarantine)
   - String length limited to 64 bytes

2. **Integer Overflow**: SafeMath used for buffer size calculations

3. **Resource Exhaustion**: Rate limiting applied (max 10 policy checks/second)

4. **Fail-Safe**: Default policy is Block (fail-closed)

### Attack Surface
- **Input**: IPC message from WebContent (untrusted)
- **Output**: PolicyGraph database update (trusted)
- **Side Effects**: None (idempotent operation)

### Testing
- Unit tests: 15 test cases (valid/invalid/boundary)
- Fuzz testing: 1M inputs with ASan/UBSan (no crashes)
- Integration tests: End-to-end IPC flow validated
```

### 6.2 Threat Model Updates

If adding new attack surface, update threat model documentation:

```markdown
## New Attack Surface: enforce_security_policy IPC

**Added**: 2025-11-01

**Description**: IPC message allowing WebContent to request policy enforcement

**Trust Boundary**: WebContent (untrusted) → RequestServer (trusted)

**Threats**:
1. Malicious WebContent sends invalid request_id → Validated, error returned
2. Malicious WebContent sends huge action string → Length limited to 64 bytes
3. Malicious WebContent floods policy checks → Rate limited to 10/sec

**Mitigations**: See Phase 3.2 implementation
```

### 6.3 User Guidance

For user-facing security features, document usage:

```markdown
## Credential Exfiltration Detection

**What it does**: Detects when a website tries to send your password to
an unexpected domain.

**How it works**:
1. Monitors form submissions containing password fields
2. Checks if form action URL matches page origin
3. If mismatch detected, shows alert to user

**User Actions**:
- **Trust**: Allow this form → future submissions to same domain allowed
- **Block**: Block this submission → future submissions blocked
- **Allow Once**: Allow this time → re-check on next submission

**Privacy**: Form relationships stored locally in PolicyGraph database
```

## Example: Adding IPC Handler

Complete walkthrough of adding a secure IPC handler.

### Step 1: Threat Model

```
Feature: Add IPC message to rotate Tor circuit for a specific tab

IPC Definition:
  rotate_tor_circuit(u64 page_id) =|

Attack Surface:
- Input: page_id (untrusted, from WebContent)
- Operation: Rotate Tor circuit (expensive, rate-limited)
- Privilege: Network configuration change

Threats:
1. DoS: Malicious page rapidly rotates circuit → network instability
2. Tracking: Attacker rotates circuit to fingerprint user
3. Resource exhaustion: Too many circuits → memory exhaustion

Mitigations:
1. Rate limit: Max 1 rotation per 10 seconds per page
2. Validation: page_id must exist and belong to caller
3. Circuit limit: Max 10 circuits per RequestServer instance
```

### Step 2: Design

```cpp
// Services/RequestServer/RequestServer.ipc
endpoint RequestServer {
    rotate_tor_circuit(u64 page_id) =|
}
```

**Validation Rules**:
- `page_id` must be > 0
- `page_id` must correspond to existing connection
- Rate limit: 1 call per 10 seconds per page_id
- Global limit: 10 active circuits max

### Step 3: Implementation

```cpp
// Services/RequestServer/ConnectionFromClient.cpp

void ConnectionFromClient::rotate_tor_circuit(u64 page_id)
{
    // Phase 1: Validate page_id
    if (page_id == 0) {
        dbgln("Invalid page_id: 0");
        return;
    }

    // Phase 2: Check rate limit
    auto now = UnixDateTime::now();
    if (m_last_circuit_rotation.contains(page_id)) {
        auto elapsed = now.seconds() - m_last_circuit_rotation.get(page_id)->seconds();
        if (elapsed < 10) {
            dbgln("Rate limit: circuit rotation too frequent ({}s elapsed)", elapsed);
            return;
        }
    }

    // Phase 3: Check global circuit limit
    if (m_tor_circuits.size() >= 10) {
        dbgln("Global circuit limit reached: {}", m_tor_circuits.size());
        return;
    }

    // Phase 4: Rotate circuit
    if (m_tor_manager) {
        auto result = m_tor_manager->rotate_circuit(page_id);
        if (result.is_error()) {
            dbgln("Failed to rotate circuit: {}", result.error());
            return;
        }
    }

    // Phase 5: Update rate limit tracking
    m_last_circuit_rotation.set(page_id, now);
}
```

### Step 4: Testing

```cpp
// Tests/RequestServer/TestTorCircuitRotation.cpp

TEST_CASE(rotate_circuit_valid_page_id)
{
    auto server = TRY_OR_FAIL(RequestServer::create());
    u64 page_id = 42;

    // Should succeed
    server->rotate_tor_circuit(page_id);
    EXPECT(server->has_circuit(page_id));
}

TEST_CASE(rotate_circuit_invalid_page_id)
{
    auto server = TRY_OR_FAIL(RequestServer::create());

    // Should fail silently (not crash)
    server->rotate_tor_circuit(0);
    EXPECT(!server->has_circuit(0));
}

TEST_CASE(rotate_circuit_rate_limit)
{
    auto server = TRY_OR_FAIL(RequestServer::create());
    u64 page_id = 42;

    // First rotation succeeds
    server->rotate_tor_circuit(page_id);
    auto circuit1 = server->get_circuit(page_id);

    // Second rotation within 10s is ignored
    server->rotate_tor_circuit(page_id);
    auto circuit2 = server->get_circuit(page_id);

    EXPECT_EQ(circuit1, circuit2); // Same circuit
}

TEST_CASE(rotate_circuit_global_limit)
{
    auto server = TRY_OR_FAIL(RequestServer::create());

    // Create 10 circuits (max)
    for (u64 i = 1; i <= 10; ++i) {
        server->rotate_tor_circuit(i);
    }

    // 11th circuit should fail
    server->rotate_tor_circuit(11);
    EXPECT(!server->has_circuit(11));
}
```

### Step 5: Security Review

**Self-review checklist**:
- [x] page_id validated (> 0)
- [x] Rate limiting implemented (10s per page)
- [x] Global limit enforced (10 circuits max)
- [x] Error handling graceful (no crashes)
- [x] Unit tests cover valid/invalid/limits
- [x] No integer overflow (u64 safe for page_id)

**Peer review questions**:
- Can attacker bypass rate limit? → No, keyed by page_id + timestamp
- Can attacker exhaust resources? → No, 10 circuit limit enforced
- Does validation cover all inputs? → Yes, page_id is only parameter

### Step 6: Documentation

```markdown
## IPC: rotate_tor_circuit

**Added**: 2025-11-01

**Purpose**: Allows WebContent to request new Tor circuit for privacy

**Security Properties**:
- Rate limited: 1 rotation per 10 seconds per page
- Global limit: 10 active circuits maximum
- Input validation: page_id must be non-zero
- Fail-safe: Silently ignores invalid requests (no crash)

**Usage**:
```cpp
client().async_rotate_tor_circuit(page_id);
```

**Testing**: See Tests/RequestServer/TestTorCircuitRotation.cpp
```

## Pre-Commit Security Checklist

Before committing security-critical changes:

**Code Review**:
- [ ] All IPC parameters validated
- [ ] No crashes on invalid input
- [ ] Integer overflow checks added
- [ ] String length limits enforced
- [ ] Rate limiting applied (if applicable)
- [ ] Graceful error handling
- [ ] No raw pointers for ownership
- [ ] No manual memory management

**Testing**:
- [ ] Unit tests for valid inputs
- [ ] Unit tests for invalid inputs
- [ ] Unit tests for boundary conditions
- [ ] Fuzz testing performed (if IPC/parsing)
- [ ] Sanitizer testing clean (ASan/UBSan)
- [ ] Integration tests pass

**Documentation**:
- [ ] Security implications documented
- [ ] Threat model updated (if new attack surface)
- [ ] User guidance added (if user-facing)
- [ ] Code comments explain security rationale

**Architecture**:
- [ ] Follows existing security patterns
- [ ] Uses PolicyGraph for persistent decisions
- [ ] Graceful degradation on failure
- [ ] Fail-safe defaults (deny by default)
- [ ] Defense in depth (multiple validation layers)

**Build**:
- [ ] Builds with Release preset
- [ ] Builds with Sanitizers preset
- [ ] No new compiler warnings
- [ ] clang-format applied

---

## References

**Ladybird Documentation**:
- `Documentation/ProcessArchitecture.md` - Multi-process design
- `Documentation/IPC.md` - IPC system
- `CLAUDE.md` - Fork-specific security features
- `docs/SENTINEL_ARCHITECTURE.md` - Sentinel system overview

**Security Resources**:
- OWASP Secure Coding Practices
- CWE Top 25 Most Dangerous Software Weaknesses
- STRIDE Threat Modeling

**Testing Resources**:
- AFL++ Fuzzing Guide
- ASan/UBSan Documentation
- LibTest Framework (AK/Test.h)

---

*Workflow version: 1.0*
*Created: 2025-11-01*
*Ladybird Security-Critical Changes Workflow*
