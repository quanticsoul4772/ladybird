# Ladybird IPC Architecture Reference

Comprehensive technical reference for Ladybird's Inter-Process Communication system, covering message format, code generation, routing, and security.

## Table of Contents
1. [Overview](#1-overview)
2. [IPC Message Format](#2-ipc-message-format)
3. [Code Generation from .ipc Files](#3-code-generation-from-ipc-files)
4. [Endpoint Structure](#4-endpoint-structure)
5. [Connection Lifecycle](#5-connection-lifecycle)
6. [Message Routing](#6-message-routing)
7. [Error Handling](#7-error-handling)
8. [Security Architecture](#8-security-architecture)

---

## 1. Overview

### Multi-Process Architecture

Ladybird uses a **sandboxed multi-process architecture** to isolate untrusted code:

```
┌──────────────────┐
│   UI Process     │  (Privileged, trusted)
│   (Ladybird)     │
└────────┬─────────┘
         │
         ├─────────────────────┬─────────────────────┬──────────────────
         │                     │                     │
         v                     v                     v
┌────────────────┐    ┌────────────────┐    ┌────────────────┐
│  WebContent    │    │ RequestServer  │    │ ImageDecoder   │
│  (sandboxed)   │    │ (per WebCon)   │    │  (sandboxed)   │
└────────────────┘    └────────────────┘    └────────────────┘
  Untrusted              Semi-trusted         Untrusted
  (Web code)             (Network I/O)        (Image parsing)
```

**Trust Boundaries:**
- **Untrusted → Trusted**: WebContent → UI, ImageDecoder → UI
- **Untrusted → Semi-Trusted**: WebContent → RequestServer
- **Critical Security Requirement**: All messages crossing trust boundaries MUST be validated

### IPC Transport Mechanisms

**Primary Transport:** Unix Domain Sockets
- Location: `/tmp/ladybird-*.sock` or abstract namespace
- Bidirectional, message-oriented
- File descriptor passing (SCM_RIGHTS)

**Alternative Transports:**
- Windows: Named pipes (experimental)
- macOS: Mach ports (future)

---

## 2. IPC Message Format

### Wire Format (Binary)

```
┌────────────────────────────────────────────────────────────┐
│                       Message Header                        │
├──────────────┬──────────────┬──────────────┬───────────────┤
│ Magic (u32)  │ MessageID    │ Payload Size │  Reserved     │
│              │ (u32)        │ (u32)        │  (u32)        │
├──────────────┴──────────────┴──────────────┴───────────────┤
│                       Message Payload                       │
│              (Serialized parameters)                        │
│                                                             │
│  [Parameter 1 Type][Parameter 1 Data]                      │
│  [Parameter 2 Type][Parameter 2 Data]                      │
│  ...                                                        │
└─────────────────────────────────────────────────────────────┘
```

**Header Fields:**
- **Magic** (4 bytes): Endpoint identifier (e.g., `0x12345678` for RequestServer)
- **MessageID** (4 bytes): Unique ID for message type within endpoint
- **Payload Size** (4 bytes): Length of payload in bytes
- **Reserved** (4 bytes): For future use, currently 0

### Parameter Encoding

**Primitive Types:**
```cpp
// Integers (little-endian)
u8, u16, u32, u64   -> Direct bytes
i8, i16, i32, i64   -> Direct bytes (two's complement)

// Floats (IEEE 754)
float  -> 4 bytes
double -> 8 bytes

// Boolean
bool -> u8 (0 or 1)
```

**Strings:**
```cpp
// String encoding
[u64: length][UTF-8 bytes]

// Example: "Hello" (5 bytes)
0x05 0x00 0x00 0x00 0x00 0x00 0x00 0x00  // length = 5
0x48 0x65 0x6C 0x6C 0x6F                  // "Hello"
```

**Vectors:**
```cpp
// Vector<T> encoding
[u64: count][T][T][T]...

// Example: Vector<u32>{1, 2, 3}
0x03 0x00 0x00 0x00 0x00 0x00 0x00 0x00  // count = 3
0x01 0x00 0x00 0x00                      // element 1
0x02 0x00 0x00 0x00                      // element 2
0x03 0x00 0x00 0x00                      // element 3
```

**Complex Types (URL::URL):**
```cpp
// URL encoding (simplified)
[String: scheme][String: host][u16: port][String: path][...]

// Example: "https://example.com:443/path"
[6 bytes: "https"][11 bytes: "example.com"][443][5 bytes: "/path"]
```

### File Descriptor Passing

**Mechanism:** SCM_RIGHTS ancillary data on Unix sockets

```cpp
// Sender side
IPC::File file(fd);  // Wraps file descriptor
encoder.append_file_descriptor(file.fd());
// FD sent as ancillary data alongside message

// Receiver side
Queue<IPC::File> received_fds;
auto file = received_fds.dequeue();  // Receives duplicated FD
// Receiver owns new FD (sender's FD still valid)
```

**Security Consideration:** File descriptors cross trust boundaries
- Receiver must validate FD is expected type (regular file, socket, etc.)
- Use `fstat()` on received FD, not path-based checks (avoid TOCTOU)

---

## 3. Code Generation from .ipc Files

### .ipc File Format

**Example:** `Services/RequestServer/RequestServer.ipc`

```cpp
#include <LibURL/URL.h>
#include <LibHTTP/HeaderMap.h>

endpoint RequestServer
{
    // Synchronous message (=>) - waits for response
    is_supported_protocol(ByteString protocol) => (bool supported)

    // Asynchronous message (=|) - no response expected
    start_request(i32 request_id, ByteString method, URL::URL url,
                  HTTP::HeaderMap headers, ByteBuffer body,
                  Core::ProxyData proxy_data, u64 page_id) =|

    // Synchronous with multiple return values
    stop_request(i32 request_id) => (bool success)
}
```

**Syntax:**
- `endpoint Name`: Defines service endpoint
- `message_name(params) =|`: Asynchronous (fire-and-forget)
- `message_name(params) => (return_types)`: Synchronous (request-response)
- `#include`: Include necessary headers for parameter types

### Generated Code Structure

**For each .ipc file, the compiler generates:**

1. **Endpoint Header** (`RequestServerEndpoint.h`)
   - Message class declarations
   - Endpoint magic number
   - `decode_message()` function

2. **Message Classes**
```cpp
// Generated from: start_request(...) =|
namespace Messages::RequestServer {
    class StartRequest final : public IPC::Message {
    public:
        static constexpr u32 static_message_id() { return 1; }

        StartRequest(i32 request_id, ByteString method, URL::URL url,
                     HTTP::HeaderMap headers, ByteBuffer body,
                     Core::ProxyData proxy_data, u64 page_id)
            : m_request_id(request_id)
            , m_method(move(method))
            , m_url(move(url))
            , m_headers(move(headers))
            , m_body(move(body))
            , m_proxy_data(move(proxy_data))
            , m_page_id(page_id)
        { }

        // Getters
        i32 request_id() const { return m_request_id; }
        ByteString const& method() const { return m_method; }
        // ... (more getters)

        // Encoding
        virtual ErrorOr<IPC::MessageBuffer> encode() const override;

        // Decoding
        static ErrorOr<NonnullOwnPtr<StartRequest>> decode(IPC::Decoder&);

    private:
        i32 m_request_id;
        ByteString m_method;
        URL::URL m_url;
        HTTP::HeaderMap m_headers;
        ByteBuffer m_body;
        Core::ProxyData m_proxy_data;
        u64 m_page_id;
    };
}
```

3. **Encoding Function**
```cpp
// Generated encode() implementation
ErrorOr<IPC::MessageBuffer> StartRequest::encode() const
{
    IPC::MessageBuffer buffer;
    IPC::Encoder encoder(buffer);

    TRY(encoder.encode(m_request_id));
    TRY(encoder.encode(m_method));
    TRY(encoder.encode(m_url));
    TRY(encoder.encode(m_headers));
    TRY(encoder.encode(m_body));
    TRY(encoder.encode(m_proxy_data));
    TRY(encoder.encode(m_page_id));

    return buffer;
}
```

4. **Decoding Function**
```cpp
// Generated decode() implementation
ErrorOr<NonnullOwnPtr<StartRequest>> StartRequest::decode(IPC::Decoder& decoder)
{
    auto request_id = TRY(decoder.decode<i32>());
    auto method = TRY(decoder.decode<ByteString>());
    auto url = TRY(decoder.decode<URL::URL>());
    auto headers = TRY(decoder.decode<HTTP::HeaderMap>());
    auto body = TRY(decoder.decode<ByteBuffer>());
    auto proxy_data = TRY(decoder.decode<Core::ProxyData>());
    auto page_id = TRY(decoder.decode<u64>());

    return make<StartRequest>(request_id, move(method), move(url),
                              move(headers), move(body),
                              move(proxy_data), page_id);
}
```

5. **Endpoint Decoder**
```cpp
// Generated endpoint-level decoder
ErrorOr<OwnPtr<IPC::Message>> RequestServerEndpoint::decode_message(
    ReadonlyBytes bytes,
    Queue<IPC::File>& fds)
{
    IPC::Decoder decoder(bytes, fds);

    u32 message_id = TRY(decoder.decode<u32>());

    switch (message_id) {
    case Messages::RequestServer::StartRequest::static_message_id():
        return TRY(Messages::RequestServer::StartRequest::decode(decoder));
    case Messages::RequestServer::StopRequest::static_message_id():
        return TRY(Messages::RequestServer::StopRequest::decode(decoder));
    // ... (more cases)
    default:
        return Error::from_string_literal("Unknown message ID");
    }
}
```

### Build Integration

**CMake Integration:**
```cmake
# Meta/CMake/lagom_compile_ipc.cmake

function(compile_ipc source output)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${IPC_COMPILER} ${source} -o ${output}
        DEPENDS ${source} ${IPC_COMPILER}
    )
endfunction()

# Usage in Services/RequestServer/CMakeLists.txt
compile_ipc(RequestServer.ipc ${CMAKE_CURRENT_BINARY_DIR}/RequestServerEndpoint.h)
```

---

## 4. Endpoint Structure

### Endpoint Definition

**Server Endpoint (RequestServerEndpoint):**
```cpp
class RequestServerEndpoint {
public:
    static constexpr u32 static_magic() { return 0x12345678; }

    // Message dispatch
    static ErrorOr<OwnPtr<IPC::Message>> decode_message(ReadonlyBytes, Queue<IPC::File>&);

    // Message handler interface (implemented by ConnectionFromClient)
    virtual void start_request(i32 request_id, ByteString method, URL::URL url,
                               HTTP::HeaderMap headers, ByteBuffer body,
                               Core::ProxyData proxy_data, u64 page_id) = 0;

    virtual Messages::RequestServer::StopRequestResponse stop_request(i32 request_id) = 0;
};
```

**Client Endpoint (RequestClientEndpoint):**
```cpp
class RequestClientEndpoint {
public:
    static constexpr u32 static_magic() { return 0x87654321; }

    static ErrorOr<OwnPtr<IPC::Message>> decode_message(ReadonlyBytes, Queue<IPC::File>&);

    // Callbacks from server
    virtual void request_started(i32 request_id, IPC::File fd) = 0;
    virtual void request_finished(i32 request_id, u64 total_size, /* ... */) = 0;
};
```

### Connection Implementation

**ConnectionFromClient Pattern:**
```cpp
// Services/RequestServer/ConnectionFromClient.h

class ConnectionFromClient final
    : public IPC::ConnectionFromClient<RequestClientEndpoint, RequestServerEndpoint>
{
public:
    explicit ConnectionFromClient(NonnullOwnPtr<IPC::Transport> transport)
        : IPC::ConnectionFromClient<RequestClientEndpoint, RequestServerEndpoint>(
              *this, move(transport))
    { }

    // Implement RequestServerEndpoint handlers
    virtual void start_request(i32 request_id, ByteString method, URL::URL url,
                               HTTP::HeaderMap headers, ByteBuffer body,
                               Core::ProxyData proxy_data, u64 page_id) override
    {
        // Validation
        if (!check_rate_limit())
            return;
        if (!validate_url(url))
            return;
        // ... more validation

        // Business logic
        issue_network_request(request_id, method, url, headers, body, proxy_data, page_id);
    }

    virtual Messages::RequestServer::StopRequestResponse stop_request(i32 request_id) override
    {
        if (!validate_request_id(request_id))
            return { false };

        auto& request = *m_active_requests.get(request_id).value();
        request.cancel();
        m_active_requests.remove(request_id);

        return { true };
    }

private:
    HashMap<i32, NonnullOwnPtr<Request>> m_active_requests;
};
```

---

## 5. Connection Lifecycle

### 1. Connection Establishment

```cpp
// Server side (RequestServer main.cpp)
auto server = IPC::MultiServer<RequestServer::ConnectionFromClient>::create().release_value_but_fixme_should_propagate_errors();

// Client side (WebContent)
auto socket = IPC::connect_to_server<RequestServer::RequestClientEndpoint, RequestServer::RequestServerEndpoint>("/tmp/request-server.sock");
auto connection = RequestServer::ConnectionFromClient::create(move(socket));
```

### 2. Handshake

```cpp
// .ipc file defines handshake message
endpoint RequestServer {
    init_transport(int peer_pid) => (int peer_pid)
}

// Client sends handshake
auto response = connection->send_sync<Messages::RequestServer::InitTransport>(getpid());
// Server responds with its PID
```

### 3. Message Exchange

**Asynchronous Message:**
```cpp
// Client sends fire-and-forget
connection->async_start_request(1, "GET", url, {}, {}, {}, 0);

// No response expected, client continues immediately
```

**Synchronous Message:**
```cpp
// Client sends and waits for response
auto response = connection->send_sync<Messages::RequestServer::StopRequest>(request_id);
bool success = response->success();
```

### 4. Connection Termination

```cpp
// Graceful shutdown
endpoint RequestServer {
    close_server() =|
}

connection->async_close_server();

// Connection dies
virtual void die() override {
    // Cleanup resources
    m_active_requests.clear();
    // Close socket
    m_transport->shutdown();
}
```

---

## 6. Message Routing

### Dispatch Flow

```
1. Message arrives on socket
   ↓
2. ConnectionBase::drain_messages_from_peer()
   - Reads message header
   - Extracts magic, message_id, payload_size
   ↓
3. ConnectionBase::try_parse_message()
   - Routes to correct endpoint decoder based on magic
   - LocalEndpoint::decode_message() or PeerEndpoint::decode_message()
   ↓
4. Endpoint decoder decodes message
   - Switches on message_id
   - Calls specific Message::decode()
   ↓
5. Message object created
   ↓
6. ConnectionBase::handle_messages()
   - Dispatches to handler
   - Calls virtual method on ConnectionFromClient
   ↓
7. Handler executes
   - Validates input
   - Performs operation
   - Sends response (if synchronous)
```

### Code Example

**Message Reception:**
```cpp
// Libraries/LibIPC/Connection.h
PeerEOF ConnectionBase::drain_messages_from_peer()
{
    while (true) {
        // Read message header
        MessageHeader header;
        auto bytes_read = m_transport->read_bytes(&header, sizeof(header));
        if (bytes_read == 0)
            return PeerEOF::Yes;

        // Validate magic
        if (header.magic != m_local_endpoint_magic &&
            header.magic != m_peer_endpoint_magic) {
            dbgln("IPC: Invalid magic: {:#x}", header.magic);
            return PeerEOF::Yes;
        }

        // Read payload
        ByteBuffer payload;
        payload.resize(header.payload_size);
        m_transport->read_bytes(payload.data(), header.payload_size);

        // Parse message
        Queue<IPC::File> fds;
        auto message = try_parse_message(payload.bytes(), fds);
        if (!message) {
            dbgln("IPC: Failed to parse message");
            continue;
        }

        m_unprocessed_messages.append(message.release_nonnull());
    }
}

// Message dispatch
void ConnectionBase::handle_messages()
{
    for (auto& message : m_unprocessed_messages) {
        // Call generated dispatcher
        message->handle(*this);
    }
    m_unprocessed_messages.clear();
}
```

---

## 7. Error Handling

### Error Propagation

**ErrorOr<T> Pattern:**
```cpp
// Handler returns ErrorOr<void> or specific response type
virtual void handle_message(Messages::Foo const& msg)
{
    auto result = process_message(msg);
    if (result.is_error()) {
        dbgln("Error handling message: {}", result.error());
        // Don't crash, log and continue
        return;
    }
}

ErrorOr<void> process_message(Messages::Foo const& msg)
{
    // Validation
    if (!validate_input(msg.data()))
        return Error::from_string_literal("Validation failed");

    // Processing
    auto buffer = TRY(allocate_buffer(msg.size()));
    TRY(write_data(buffer, msg.data()));

    return {};
}
```

### IPC Error Responses

**Synchronous Message Errors:**
```cpp
// .ipc definition with error return
endpoint RequestServer {
    stop_request(i32 request_id) => (bool success)
}

// Handler returns error state
virtual Messages::RequestServer::StopRequestResponse stop_request(i32 request_id) override
{
    if (!validate_request_id(request_id))
        return { false };  // Error: invalid ID

    // Success
    return { true };
}
```

### Connection Errors

**Fatal Errors (die):**
```cpp
virtual void die() override
{
    dbgln("RequestServer connection died");

    // Cleanup all resources
    m_active_requests.clear();
    m_websockets.clear();

    // Notify parent process
    if (on_death)
        on_death();
}

// Triggered by:
// - Socket disconnect
// - Too many validation failures
// - Unrecoverable error
```

---

## 8. Security Architecture

### Trust Boundary Enforcement

**Validation at Trust Boundaries:**
```cpp
// WebContent (untrusted) -> RequestServer (semi-trusted)
void ConnectionFromClient::start_request(/* params */)
{
    // 1. Rate limiting
    if (!check_rate_limit())
        return;

    // 2. Input validation
    if (!validate_url(url))
        return;
    if (!validate_header_map(headers))
        return;
    if (!validate_buffer_size(body.size(), "body"sv))
        return;

    // 3. Business logic (inputs now trusted)
    issue_network_request(/* ... */);
}
```

### Rate Limiting

**Implementation:**
```cpp
// Libraries/LibIPC/RateLimiter.h
class RateLimiter {
public:
    RateLimiter(size_t max_tokens, Duration window)
        : m_max_tokens(max_tokens)
        , m_window(window)
    { }

    bool try_consume()
    {
        auto now = MonotonicTime::now();

        // Reset if window expired
        if (now - m_window_start > m_window) {
            m_tokens = m_max_tokens;
            m_window_start = now;
        }

        // Check tokens available
        if (m_tokens == 0)
            return false;

        m_tokens--;
        return true;
    }

private:
    size_t m_max_tokens;
    size_t m_tokens { 0 };
    Duration m_window;
    MonotonicTime m_window_start;
};

// Usage
IPC::RateLimiter m_rate_limiter { 1000, AK::Duration::from_milliseconds(10) };
```

### Validation Failure Tracking

**Disconnect on Repeated Violations:**
```cpp
void ConnectionFromClient::track_validation_failure()
{
    m_validation_failures++;

    if (m_validation_failures >= s_max_validation_failures) {
        dbgln("Security: Exceeded validation failure limit ({}), terminating",
              s_max_validation_failures);
        die();  // Disconnect malicious client
    }
}

static constexpr size_t s_max_validation_failures = 100;
```

### Logging Security Events

**Pattern:**
```cpp
bool validate_url(URL::URL const& url, SourceLocation location = SourceLocation::current())
{
    if (!url.scheme().is_one_of("http"sv, "https"sv)) {
        dbgln("Security: Disallowed URL scheme '{}' at {}:{}",
              url.scheme(), location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}

// Logs include:
// - "Security:" prefix for filtering
// - Exact validation failure (oversized, invalid, etc.)
// - Source location (file:line)
```

---

## Key Takeaways

### Security Best Practices

1. **Never trust IPC input**: All data from untrusted processes is hostile
2. **Validate at boundaries**: Check all inputs before processing
3. **Use generated code**: Don't manually parse IPC messages
4. **Rate limit everything**: Prevent DoS through message flooding
5. **Track failures**: Disconnect on repeated validation violations
6. **Log security events**: Audit trail for attacks
7. **Fail closed**: Reject on validation failure, don't proceed

### Common Pitfalls

1. **Trusting IPC flags**: Don't trust `is_privileged` from untrusted source
2. **Missing validation**: Every handler must validate all inputs
3. **Type confusion**: Use strong types, not void* or C-style casts
4. **Integer overflow**: Use checked arithmetic for size calculations
5. **TOCTOU races**: Use FD-based operations, not path-based

---

## References

### Source Files
- `Libraries/LibIPC/Connection.h` - Connection base classes
- `Libraries/LibIPC/Encoder.h` - Message encoding
- `Libraries/LibIPC/Decoder.h` - Message decoding
- `Libraries/LibIPC/Limits.h` - Size limits (MaxStringLength, MaxVectorSize, etc.)
- `Meta/Lagom/Tools/CodeGenerators/IPCCompiler/` - IPC code generator

### Example Implementations
- `Services/RequestServer/ConnectionFromClient.h` - Full validation pattern
- `Services/WebContent/ConnectionFromClient.h` - WebContent IPC handler
- `Services/RequestServer/RequestServer.ipc` - .ipc file example

### Documentation
- `.claude/skills/ipc-security/SKILL.md` - IPC security patterns
- `.claude/skills/ipc-security/references/known-vulnerabilities.md` - Vulnerability catalog
- `.claude/skills/ipc-security/references/validation-patterns.md` - Input validation reference
- `Documentation/ProcessArchitecture.md` - Multi-process architecture overview
