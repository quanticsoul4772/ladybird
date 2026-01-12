---
name: tor-integration
description: Per-tab Tor circuit integration for privacy-focused browsing in Ladybird, including stream isolation, circuit management, and leak prevention
use-when: Implementing Tor features, debugging connection issues, or ensuring privacy guarantees
allowed-tools:
  - Read
  - Bash
  - Grep
tags:
  - privacy
  - networking
  - tor
---

# Tor Integration Patterns

## Architecture
```
┌──────────────────┐
│   Tab 1          │
│   (WebContent 1) │
└────────┬─────────┘
         │ IPC: RequestResource(url)
         ↓
┌────────┴────────────────────┐
│    RequestServer            │
│  ┌───────────────────────┐  │
│  │ Tor Circuit Manager   │  │
│  │  - Tab 1 → Circuit A  │  │
│  │  - Tab 2 → Circuit B  │  │
│  │  - Tab 3 → Circuit C  │  │
│  └───────────────────────┘  │
└────────┬────────────────────┘
         │ SOCKS5 (isolated stream)
         ↓
┌────────┴────────┐
│   Tor Daemon    │
│   (127.0.0.1:   │
│    9050)        │
└─────────────────┘
```

## Core Concepts

### 1. Circuit Isolation

**Each tab gets its own Tor circuit to prevent correlation**
```cpp
class TorCircuitManager {
public:
    ErrorOr<CircuitID> get_or_create_circuit(TabID tab_id)
    {
        // Check if tab already has circuit
        if (auto circuit = m_tab_circuits.get(tab_id))
            return *circuit;

        // Create new isolated circuit
        auto circuit_id = TRY(create_new_circuit());

        // Map tab to circuit
        m_tab_circuits.set(tab_id, circuit_id);
        m_circuit_created_at.set(circuit_id, MonotonicTime::now());

        dbgln("Created Tor circuit {} for tab {}", circuit_id, tab_id);
        return circuit_id;
    }

private:
    HashMap<TabID, CircuitID> m_tab_circuits;
    HashMap<CircuitID, MonotonicTime> m_circuit_created_at;
};
```

### 2. SOCKS5 Stream Isolation
```cpp
ErrorOr<Response> make_tor_request(URL const& url, CircuitID circuit_id)
{
    // Connect to Tor SOCKS5 proxy
    auto socket = TRY(Core::TCPSocket::connect("127.0.0.1", 9050));

    // SOCKS5 handshake
    TRY(socks5_handshake(socket));

    // Use circuit-specific authentication
    // This ensures streams use different circuits
    String username = String::formatted("circuit_{}", circuit_id);
    String password = generate_random_password();

    TRY(socks5_authenticate(socket, username, password));

    // Connect through Tor
    TRY(socks5_connect(socket, url.host(), url.port()));

    // Make HTTP request over established connection
    return make_http_request(socket, url);
}
```

### 3. Circuit Rotation Policy
```cpp
void check_circuit_rotation()
{
    auto now = MonotonicTime::now();

    for (auto& [circuit_id, created_at] : m_circuit_created_at) {
        auto age = now - created_at;

        // Rotate circuits after 10 minutes
        if (age > Duration::from_minutes(10)) {
            dbgln("Rotating circuit {} (age: {})", circuit_id, age);
            TRY(rotate_circuit(circuit_id));
        }
    }
}

ErrorOr<void> rotate_circuit(CircuitID old_circuit)
{
    // Find tab using this circuit
    auto tab_id = TRY(find_tab_for_circuit(old_circuit));

    // Create new circuit
    auto new_circuit = TRY(create_new_circuit());

    // Update mapping
    m_tab_circuits.set(tab_id, new_circuit);

    // Close old circuit
    TRY(close_circuit(old_circuit));

    return {};
}
```

## Privacy Guarantees

### 1. DNS Leak Prevention
```cpp
// ❌ WRONG - DNS query leaks real IP
auto ip = gethostbyname(url.host());  // Goes to system DNS!

// ✅ CORRECT - DNS through Tor
ErrorOr<IPAddress> resolve_through_tor(String const& hostname, CircuitID circuit)
{
    // Use Tor's SOCKS5 remote DNS resolution
    // This sends hostname to Tor, not local DNS
    return socks5_remote_resolve(hostname, circuit);
}
```

### 2. WebRTC Leak Prevention
```cpp
// Disable WebRTC ICE candidates that reveal local IP
class WebRTCConfiguration {
    RTCConfiguration get_tor_safe_config()
    {
        RTCConfiguration config;

        // Disable all local IP exposure
        config.ice_transport_policy = "relay";  // Only use TURN
        config.bundle_policy = "max-bundle";

        // Disable mDNS that can leak hostname
        config.sdp_semantics = "unified-plan";

        return config;
    }
};
```

### 3. User-Agent Normalization
```cpp
String get_tor_user_agent()
{
    // Use Tor Browser's user agent for uniformity
    // Don't reveal OS, system details
    return "Mozilla/5.0 (Windows NT 10.0; rv:109.0) Gecko/20100101 Firefox/115.0"sv;
}
```

### 4. HTTP Header Normalization
```cpp
void add_tor_safe_headers(HTTP::Request& request)
{
    // Standard headers only
    request.set_header("User-Agent", get_tor_user_agent());
    request.set_header("Accept-Language", "en-US,en;q=0.5");
    request.set_header("Accept-Encoding", "gzip, deflate, br");

    // Remove identifying headers
    request.remove_header("X-Forwarded-For");
    request.remove_header("Via");
    request.remove_header("Referer");  // Optional: breaks some sites

    // Prevent cache timing attacks
    request.set_header("Cache-Control", "no-cache");
}
```

## Connection Management

### Circuit Status Monitoring
```cpp
enum class CircuitStatus {
    Building,
    Ready,
    Failed,
    Closed
};

class TorCircuit {
    void monitor_status()
    {
        // Check circuit health
        auto status = TRY(query_circuit_status(m_circuit_id));

        switch (status) {
        case CircuitStatus::Building:
            // Show "Connecting through Tor..." UI
            break;

        case CircuitStatus::Ready:
            // Allow requests
            m_is_ready = true;
            break;

        case CircuitStatus::Failed:
            // Retry with new circuit
            dbgln("Circuit {} failed, creating new one", m_circuit_id);
            TRY(rotate_circuit(m_circuit_id));
            break;

        case CircuitStatus::Closed:
            // Clean up
            m_is_ready = false;
            break;
        }
    }

private:
    CircuitID m_circuit_id;
    bool m_is_ready { false };
};
```

### Fallback to Direct Connection
```cpp
ErrorOr<Response> make_request(URL const& url, TabID tab_id)
{
    // Try Tor first if enabled
    if (m_tor_enabled) {
        auto circuit = TRY(m_circuit_manager->get_or_create_circuit(tab_id));
        auto result = make_tor_request(url, circuit);

        if (result.has_value())
            return result.value();

        // Tor failed - ask user
        if (should_fallback_to_direct()) {
            dbgln("Tor connection failed, falling back to direct");
            return make_direct_request(url);
        }

        return result.error();
    }

    // Direct connection
    return make_direct_request(url);
}
```

## Security Considerations

### 1. Circuit Fingerprinting Prevention
```cpp
// Randomize timing to prevent fingerprinting
void add_random_delay()
{
    // Random delay between 0-100ms
    auto delay_ms = arc4random_uniform(100);
    usleep(delay_ms * 1000);
}
```

### 2. Circuit Isolation Verification
```cpp
TEST_CASE(circuit_isolation_enforced)
{
    auto manager = TorCircuitManager::create();

    auto circuit1 = manager->get_or_create_circuit(TabID(1));
    auto circuit2 = manager->get_or_create_circuit(TabID(2));

    // Different tabs MUST have different circuits
    EXPECT_NE(circuit1, circuit2);
}
```

### 3. Exit Node Selection
```cpp
ErrorOr<void> configure_exit_node_restrictions()
{
    // Torrc configuration
    String config = R"(
        # Exclude known malicious exit nodes
        ExcludeExitNodes {badexitnode1},{badexitnode2}

        # Optionally select exit nodes by country
        # ExitNodes {us},{ca},{uk}

        # Strict circuit requirements
        StrictNodes 1
    )"sv;

    return write_torrc(config);
}
```

## UI Integration

### Connection Status Indicator
```cpp
class TorStatusIndicator {
    void update_ui()
    {
        switch (m_status) {
        case TorStatus::Disconnected:
            set_icon("tor-disabled.png");
            set_tooltip("Tor is disabled");
            break;

        case TorStatus::Connecting:
            set_icon("tor-connecting.png");
            set_tooltip("Connecting through Tor...");
            break;

        case TorStatus::Connected:
            set_icon("tor-enabled.png");
            auto circuit = m_manager->get_circuit_for_tab(m_tab_id);
            set_tooltip(String::formatted("Connected via Tor (Circuit {})", circuit));
            break;
        }
    }

private:
    TorStatus m_status;
    TabID m_tab_id;
    TorCircuitManager* m_manager;
};
```

## Testing

### Tor Connection Test
```bash
#!/bin/bash
# Test if Tor connection works

# Check Tor daemon
if ! pgrep tor > /dev/null; then
    echo "❌ Tor daemon not running"
    exit 1
fi

# Test SOCKS proxy
if ! nc -z 127.0.0.1 9050; then
    echo "❌ Tor SOCKS proxy not accessible"
    exit 1
fi

# Test IP anonymization
DIRECT_IP=$(curl -s https://api.ipify.org)
TOR_IP=$(curl -s --socks5 127.0.0.1:9050 https://api.ipify.org)

if [ "$DIRECT_IP" = "$TOR_IP" ]; then
    echo "❌ Tor not anonymizing IP"
    exit 1
fi

echo "✅ Tor integration working"
echo "Direct IP: $DIRECT_IP"
echo "Tor IP: $TOR_IP"
```

### Circuit Isolation Test
```cpp
TEST_CASE(different_tabs_different_circuits)
{
    auto manager = TorCircuitManager::create();

    // Create circuits for 10 different tabs
    Vector<CircuitID> circuits;
    for (TabID i = 1; i <= 10; ++i) {
        circuits.append(manager->get_or_create_circuit(i));
    }

    // All circuits should be unique
    HashTable<CircuitID> unique_circuits;
    for (auto circuit : circuits)
        unique_circuits.set(circuit);

    EXPECT_EQ(unique_circuits.size(), 10u);
}
```

## Checklist

- [ ] Each tab gets isolated Tor circuit
- [ ] DNS queries go through Tor
- [ ] WebRTC disabled or relay-only
- [ ] User-Agent normalized
- [ ] HTTP headers sanitized
- [ ] Circuit rotation policy implemented
- [ ] Fallback to direct connection (optional)
- [ ] Connection status UI
- [ ] Leak prevention tested
- [ ] Circuit isolation verified

## Related Skills

### Architecture Dependencies
- **[multi-process-architecture](../multi-process-architecture/SKILL.md)**: Per-tab Tor circuits leverage process isolation. Each WebContent process can have independent Tor streams. Understand process boundaries before implementing circuit isolation.

### Security Integration
- **[ipc-security](../ipc-security/SKILL.md)**: Secure Tor configuration and circuit management IPC. Validate SOCKS proxy settings and prevent circuit correlation attacks via IPC.

### Implementation Skills
- **[ladybird-cpp-patterns](../ladybird-cpp-patterns/SKILL.md)**: Implement Tor integration using ErrorOr<T> for connection failures and RefPtr for circuit management. Follow east const style and error handling patterns.
- **[cmake-build-system](../cmake-build-system/SKILL.md)**: Link tor library and configure SOCKS proxy support. Add tor dependency to vcpkg.json or build system.

### Testing and Debugging
- **[memory-safety-debugging](../memory-safety-debugging/SKILL.md)**: Debug Tor connection failures and circuit management bugs using GDB/LLDB. Attach to RequestServer process to inspect SOCKS connections.
- **[browser-performance](../browser-performance/SKILL.md)**: Profile Tor circuit creation overhead and connection latency. Optimize circuit reuse and stream multiplexing.

### Documentation
- **[documentation-generation](../documentation-generation/SKILL.md)**: Document Tor privacy guarantees and configuration options. Create user guides for per-tab circuit isolation.
