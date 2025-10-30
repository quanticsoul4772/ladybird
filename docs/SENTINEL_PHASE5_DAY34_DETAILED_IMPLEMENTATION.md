# Sentinel Phase 5 Day 34: Production Readiness - Detailed Implementation Plan

**Date**: 2025-10-30
**Status**: Implementation Ready
**Focus**: Configuration management, monitoring, diagnostic tools, and deployment preparation

---

## Overview

Day 34 addresses **critical configuration inflexibility** identified in the technical debt report (Section 6: 57+ hardcoded values, 8 critical/high severity issues). This day focuses on making Sentinel production-ready through comprehensive configuration management, observability, and operational tooling.

**Key Priorities**:
1. Fix 8 critical/high severity configuration issues (hardcoded socket path, YARA path, timeouts)
2. Add environment variable support for container/CI deployments
3. Implement configuration validation and reload capability
4. Add metrics collection for observability
5. Build diagnostic CLI tools for operations

---

## Morning Session (4 hours): Configuration System

### Task 1: Create Comprehensive Configuration Structure (CRITICAL)
**Time**: 1.5 hours
**Files**:
- `Libraries/LibWebView/SentinelConfig.h` (expand existing)
- `Libraries/LibWebView/SentinelConfig.cpp` (expand existing)

**Current Problem**:
- 57+ hardcoded values across codebase
- No environment variable support
- Socket path hardcoded to `/tmp/sentinel.sock` (security exposure)
- YARA rules path hardcoded with username `/home/rbsmith4/...` (deployment blocker)
- No timeout configuration (FIXME at line 27 of SecurityTap.cpp)
- Scan size limits hardcoded (100MB in SecurityTap.cpp:101,248)

**Solution**: Comprehensive configuration structure with validation

**Changes Required**:

1. **Expand SentinelConfig struct** in `Libraries/LibWebView/SentinelConfig.h`:
```cpp
namespace WebView {

struct NetworkConfig {
    String socket_path;                    // IPC socket path
    u32 socket_timeout_ms { 5000 };        // Socket timeout (fail-fast)
    u32 connect_retry_count { 3 };         // Connection retry attempts
    u32 connect_retry_delay_ms { 100 };    // Delay between retries

    static ErrorOr<String> default_socket_path();  // Use XDG_RUNTIME_DIR
};

struct ScanConfig {
    size_t max_scan_size_bytes { 100 * 1024 * 1024 };  // 100MB default
    size_t min_scan_size_bytes { 0 };                   // No minimum
    bool fail_open_on_error { true };                   // Fail-open vs fail-closed
    bool fail_open_on_timeout { true };                 // Allow on timeout
    bool fail_open_on_large_file { true };              // Allow oversized files
    bool async_scan_enabled { true };                   // Use async for large files
    size_t async_scan_threshold_bytes { 1 * 1024 * 1024 };  // 1MB threshold
};

struct DatabaseConfig {
    String database_path;
    size_t max_policies { 10000 };              // Hard limit on policies
    size_t max_threats { 100000 };              // Hard limit on threats
    u32 vacuum_threshold_mb { 100 };            // Auto-vacuum trigger
    bool enable_wal_mode { true };              // SQLite WAL for concurrency
    u32 busy_timeout_ms { 5000 };               // Database lock timeout
};

struct QuarantineConfig {
    String quarantine_path;
    size_t max_file_size_bytes { 200 * 1024 * 1024 };  // 200MB per file
    size_t max_total_size_bytes { 5ULL * 1024 * 1024 * 1024 };  // 5GB total
    u32 max_file_count { 1000 };                        // Max quarantined files
    u32 retention_days { 90 };                          // Auto-delete after 90d
};

struct YaraConfig {
    String rules_directory;
    Vector<String> rule_files;                  // Specific files to load
    u32 scan_timeout_seconds { 60 };            // YARA scan timeout
    size_t max_rules { 1000 };                  // Hard limit on rules
    bool enable_fast_mode { false };            // YARA fast scanning
};

struct LoggingConfig {
    enum class Level {
        Debug,
        Info,
        Warning,
        Error
    };

    Level log_level { Level::Info };
    bool log_to_file { false };
    String log_file_path;
    size_t max_log_size_mb { 100 };
    u32 max_log_files { 5 };                    // Log rotation
};

struct PerformanceConfig {
    size_t policy_cache_size { 1000 };
    size_t thread_pool_size { 4 };              // Async scan workers
    u32 cache_ttl_seconds { 3600 };             // Cache entry TTL
    bool enable_cache_persistence { false };    // Persist cache to disk
};

struct SentinelConfig {
    bool enabled { true };

    NetworkConfig network;
    ScanConfig scan;
    DatabaseConfig database;
    QuarantineConfig quarantine;
    YaraConfig yara;
    LoggingConfig logging;
    PerformanceConfig performance;

    // Existing fields
    size_t threat_retention_days { 30 };
    RateLimitConfig rate_limit;
    NotificationConfig notifications;

    // Static factory methods
    static ErrorOr<SentinelConfig> load_from_file(String const& path);
    static ErrorOr<SentinelConfig> load_from_env();        // NEW: Environment vars
    static ErrorOr<SentinelConfig> load_default();
    static SentinelConfig create_default();

    // Validation
    ErrorOr<void> validate() const;                        // NEW: Config validation

    // Serialization
    [[nodiscard]] String to_json() const;
    static ErrorOr<SentinelConfig> from_json(String const& json_string);
    ErrorOr<void> save_to_file(String const& path) const;
};

}
```

2. **Implement default path functions** in `Libraries/LibWebView/SentinelConfig.cpp`:
```cpp
ErrorOr<String> NetworkConfig::default_socket_path()
{
    // Use XDG_RUNTIME_DIR if available, fallback to /tmp
    if (auto runtime_dir = Core::System::getenv("XDG_RUNTIME_DIR"sv); runtime_dir.has_value()) {
        StringBuilder path_builder;
        path_builder.append(runtime_dir.value());
        path_builder.append("/ladybird/sentinel.sock"sv);
        return path_builder.to_string();
    }

    // Fallback: use /tmp but warn about security
    dbgln("Warning: XDG_RUNTIME_DIR not set, using /tmp/sentinel.sock (security risk)");
    return "/tmp/sentinel.sock"_string;
}
```

3. **Implement environment variable loading** in `Libraries/LibWebView/SentinelConfig.cpp`:
```cpp
ErrorOr<SentinelConfig> SentinelConfig::load_from_env()
{
    auto config = create_default();

    // Core settings
    if (auto val = Core::System::getenv("SENTINEL_ENABLED"sv); val.has_value()) {
        config.enabled = val.value() == "true"sv || val.value() == "1"sv;
    }

    // Network configuration
    if (auto val = Core::System::getenv("SENTINEL_SOCKET_PATH"sv); val.has_value()) {
        config.network.socket_path = TRY(String::from_byte_string(val.value()));
    }
    if (auto val = Core::System::getenv("SENTINEL_SOCKET_TIMEOUT_MS"sv); val.has_value()) {
        config.network.socket_timeout_ms = val.value().to_number<u32>().value_or(5000);
    }

    // Scan configuration
    if (auto val = Core::System::getenv("SENTINEL_MAX_SCAN_SIZE_MB"sv); val.has_value()) {
        auto mb = val.value().to_number<u32>().value_or(100);
        config.scan.max_scan_size_bytes = mb * 1024 * 1024;
    }
    if (auto val = Core::System::getenv("SENTINEL_FAIL_OPEN"sv); val.has_value()) {
        config.scan.fail_open_on_error = val.value() == "true"sv || val.value() == "1"sv;
    }

    // YARA configuration
    if (auto val = Core::System::getenv("SENTINEL_YARA_RULES_DIR"sv); val.has_value()) {
        config.yara.rules_directory = TRY(String::from_byte_string(val.value()));
    }
    if (auto val = Core::System::getenv("SENTINEL_YARA_TIMEOUT_SEC"sv); val.has_value()) {
        config.yara.scan_timeout_seconds = val.value().to_number<u32>().value_or(60);
    }

    // Database configuration
    if (auto val = Core::System::getenv("SENTINEL_DATABASE_PATH"sv); val.has_value()) {
        config.database.database_path = TRY(String::from_byte_string(val.value()));
    }
    if (auto val = Core::System::getenv("SENTINEL_MAX_POLICIES"sv); val.has_value()) {
        config.database.max_policies = val.value().to_number<size_t>().value_or(10000);
    }

    // Quarantine configuration
    if (auto val = Core::System::getenv("SENTINEL_QUARANTINE_PATH"sv); val.has_value()) {
        config.quarantine.quarantine_path = TRY(String::from_byte_string(val.value()));
    }
    if (auto val = Core::System::getenv("SENTINEL_QUARANTINE_MAX_SIZE_GB"sv); val.has_value()) {
        auto gb = val.value().to_number<u32>().value_or(5);
        config.quarantine.max_total_size_bytes = static_cast<size_t>(gb) * 1024 * 1024 * 1024;
    }

    // Performance configuration
    if (auto val = Core::System::getenv("SENTINEL_CACHE_SIZE"sv); val.has_value()) {
        config.performance.policy_cache_size = val.value().to_number<size_t>().value_or(1000);
    }
    if (auto val = Core::System::getenv("SENTINEL_THREAD_POOL_SIZE"sv); val.has_value()) {
        config.performance.thread_pool_size = val.value().to_number<size_t>().value_or(4);
    }

    // Logging configuration
    if (auto val = Core::System::getenv("SENTINEL_LOG_LEVEL"sv); val.has_value()) {
        if (val.value() == "debug"sv)
            config.logging.log_level = LoggingConfig::Level::Debug;
        else if (val.value() == "info"sv)
            config.logging.log_level = LoggingConfig::Level::Info;
        else if (val.value() == "warning"sv)
            config.logging.log_level = LoggingConfig::Level::Warning;
        else if (val.value() == "error"sv)
            config.logging.log_level = LoggingConfig::Level::Error;
    }
    if (auto val = Core::System::getenv("SENTINEL_LOG_FILE"sv); val.has_value()) {
        config.logging.log_to_file = true;
        config.logging.log_file_path = TRY(String::from_byte_string(val.value()));
    }

    // Validate configuration
    TRY(config.validate());

    return config;
}
```

4. **Implement configuration validation** in `Libraries/LibWebView/SentinelConfig.cpp`:
```cpp
ErrorOr<void> SentinelConfig::validate() const
{
    // Network validation
    if (network.socket_path.is_empty())
        return Error::from_string_literal("socket_path cannot be empty");
    if (network.socket_timeout_ms < 100 || network.socket_timeout_ms > 60000)
        return Error::from_string_literal("socket_timeout_ms must be between 100 and 60000");
    if (network.connect_retry_count > 10)
        return Error::from_string_literal("connect_retry_count must be <= 10");

    // Scan validation
    if (scan.max_scan_size_bytes < 1024 * 1024)  // Min 1MB
        return Error::from_string_literal("max_scan_size_bytes must be >= 1MB");
    if (scan.max_scan_size_bytes > 1024ULL * 1024 * 1024)  // Max 1GB
        return Error::from_string_literal("max_scan_size_bytes must be <= 1GB");
    if (scan.async_scan_threshold_bytes > scan.max_scan_size_bytes)
        return Error::from_string_literal("async_scan_threshold must be <= max_scan_size");

    // Database validation
    if (database.database_path.is_empty())
        return Error::from_string_literal("database_path cannot be empty");
    if (database.max_policies < 100 || database.max_policies > 1000000)
        return Error::from_string_literal("max_policies must be between 100 and 1000000");
    if (database.max_threats < 1000 || database.max_threats > 10000000)
        return Error::from_string_literal("max_threats must be between 1000 and 10000000");

    // Quarantine validation
    if (quarantine.quarantine_path.is_empty())
        return Error::from_string_literal("quarantine_path cannot be empty");
    if (quarantine.max_file_size_bytes > 1024ULL * 1024 * 1024)  // Max 1GB per file
        return Error::from_string_literal("max_file_size_bytes must be <= 1GB");
    if (quarantine.max_file_count < 10 || quarantine.max_file_count > 100000)
        return Error::from_string_literal("max_file_count must be between 10 and 100000");
    if (quarantine.retention_days < 1 || quarantine.retention_days > 3650)
        return Error::from_string_literal("retention_days must be between 1 and 3650");

    // YARA validation
    if (yara.rules_directory.is_empty())
        return Error::from_string_literal("yara_rules_directory cannot be empty");
    if (yara.scan_timeout_seconds < 1 || yara.scan_timeout_seconds > 600)
        return Error::from_string_literal("scan_timeout_seconds must be between 1 and 600");
    if (yara.max_rules < 10 || yara.max_rules > 10000)
        return Error::from_string_literal("max_rules must be between 10 and 10000");

    // Performance validation
    if (performance.policy_cache_size < 100 || performance.policy_cache_size > 100000)
        return Error::from_string_literal("policy_cache_size must be between 100 and 100000");
    if (performance.thread_pool_size < 1 || performance.thread_pool_size > 32)
        return Error::from_string_literal("thread_pool_size must be between 1 and 32");
    if (performance.cache_ttl_seconds < 60 || performance.cache_ttl_seconds > 86400)
        return Error::from_string_literal("cache_ttl_seconds must be between 60 and 86400");

    // Rate limit validation
    if (rate_limit.policies_per_minute < 1 || rate_limit.policies_per_minute > 1000)
        return Error::from_string_literal("policies_per_minute must be between 1 and 1000");
    if (rate_limit.rate_window_seconds < 10 || rate_limit.rate_window_seconds > 3600)
        return Error::from_string_literal("rate_window_seconds must be between 10 and 3600");

    // Notification validation
    if (notifications.auto_dismiss_seconds > 300)
        return Error::from_string_literal("auto_dismiss_seconds must be <= 300");
    if (notifications.max_queue_size < 1 || notifications.max_queue_size > 100)
        return Error::from_string_literal("max_queue_size must be between 1 and 100");

    return {};
}
```

**Configuration File Format** (`~/.config/ladybird/sentinel/config.json`):
```json
{
  "enabled": true,

  "network": {
    "socket_path": "/run/user/1000/ladybird/sentinel.sock",
    "socket_timeout_ms": 5000,
    "connect_retry_count": 3,
    "connect_retry_delay_ms": 100
  },

  "scan": {
    "max_scan_size_bytes": 104857600,
    "min_scan_size_bytes": 0,
    "fail_open_on_error": true,
    "fail_open_on_timeout": true,
    "fail_open_on_large_file": true,
    "async_scan_enabled": true,
    "async_scan_threshold_bytes": 1048576
  },

  "database": {
    "database_path": "~/.local/share/Ladybird/PolicyGraph/policies.db",
    "max_policies": 10000,
    "max_threats": 100000,
    "vacuum_threshold_mb": 100,
    "enable_wal_mode": true,
    "busy_timeout_ms": 5000
  },

  "quarantine": {
    "quarantine_path": "~/.local/share/Ladybird/Quarantine",
    "max_file_size_bytes": 209715200,
    "max_total_size_bytes": 5368709120,
    "max_file_count": 1000,
    "retention_days": 90
  },

  "yara": {
    "rules_directory": "~/.config/ladybird/sentinel/rules",
    "rule_files": ["default.yar", "web-threats.yar", "archives.yar"],
    "scan_timeout_seconds": 60,
    "max_rules": 1000,
    "enable_fast_mode": false
  },

  "logging": {
    "log_level": "info",
    "log_to_file": false,
    "log_file_path": "",
    "max_log_size_mb": 100,
    "max_log_files": 5
  },

  "performance": {
    "policy_cache_size": 1000,
    "thread_pool_size": 4,
    "cache_ttl_seconds": 3600,
    "enable_cache_persistence": false
  },

  "threat_retention_days": 30,

  "rate_limit": {
    "policies_per_minute": 5,
    "rate_window_seconds": 60
  },

  "notifications": {
    "enabled": true,
    "auto_dismiss_seconds": 5,
    "max_queue_size": 10
  }
}
```

**Testing**:
```cpp
// Test default config creation
auto config = SentinelConfig::create_default();
EXPECT(!config.validate().is_error());

// Test environment variable loading
Core::System::setenv("SENTINEL_MAX_SCAN_SIZE_MB"sv, "200"sv);
Core::System::setenv("SENTINEL_SOCKET_TIMEOUT_MS"sv, "10000"sv);
auto env_config = TRY(SentinelConfig::load_from_env());
EXPECT_EQ(env_config.scan.max_scan_size_bytes, 200 * 1024 * 1024);
EXPECT_EQ(env_config.network.socket_timeout_ms, 10000u);

// Test validation failures
config.network.socket_timeout_ms = 100000;  // Too high
EXPECT(config.validate().is_error());

config.scan.max_scan_size_bytes = 500;  // Too low
EXPECT(config.validate().is_error());

// Test JSON serialization roundtrip
auto json = config.to_json();
auto parsed = TRY(SentinelConfig::from_json(json));
EXPECT_EQ(parsed.scan.max_scan_size_bytes, config.scan.max_scan_size_bytes);
```

---

### Task 2: Update SecurityTap to Use Configuration (CRITICAL)
**Time**: 45 minutes
**File**: `Services/RequestServer/SecurityTap.cpp`

**Current Problem**:
- Line 24: Hardcoded socket path `/tmp/sentinel.sock`
- Line 27: FIXME comment about timeout configuration
- Line 51: Duplicate hardcoded socket path
- Line 101: Hardcoded MAX_SCAN_SIZE = 100MB
- Line 248: Duplicate MAX_SCAN_SIZE = 100MB

**Solution**: Replace all hardcoded values with configuration

**Changes Required**:

1. **Add config member** to `Services/RequestServer/SecurityTap.h`:
```cpp
#include <LibWebView/SentinelConfig.h>

class SecurityTap {
public:
    static ErrorOr<NonnullOwnPtr<SecurityTap>> create();
    static ErrorOr<NonnullOwnPtr<SecurityTap>> create(WebView::SentinelConfig const& config);

private:
    SecurityTap(NonnullOwnPtr<Core::LocalSocket> socket, WebView::SentinelConfig config);

    NonnullOwnPtr<Core::LocalSocket> m_sentinel_socket;
    WebView::SentinelConfig m_config;  // NEW: Configuration
    bool m_connection_failed { false };
};
```

2. **Update SecurityTap::create()** in `Services/RequestServer/SecurityTap.cpp`:
```cpp
ErrorOr<NonnullOwnPtr<SecurityTap>> SecurityTap::create()
{
    // Load configuration from environment, then file, then defaults
    auto config_result = WebView::SentinelConfig::load_from_env();
    if (config_result.is_error()) {
        dbgln("SecurityTap: Failed to load config from environment: {}", config_result.error());
        config_result = WebView::SentinelConfig::load_default();
    }

    if (config_result.is_error()) {
        dbgln("SecurityTap: Failed to load default config: {}", config_result.error());
        return config_result.release_error();
    }

    auto config = config_result.release_value();
    return create(config);
}

ErrorOr<NonnullOwnPtr<SecurityTap>> SecurityTap::create(WebView::SentinelConfig const& config)
{
    // Validate configuration
    TRY(config.validate());

    // Connect to Sentinel daemon using configured socket path
    auto socket = TRY(Core::LocalSocket::connect(config.network.socket_path));

    // Set socket timeout
    struct timeval timeout;
    timeout.tv_sec = config.network.socket_timeout_ms / 1000;
    timeout.tv_usec = (config.network.socket_timeout_ms % 1000) * 1000;

    auto fd = socket->fd();
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        dbgln("SecurityTap: Warning - failed to set socket timeout: {}", strerror(errno));
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        dbgln("SecurityTap: Warning - failed to set socket timeout: {}", strerror(errno));
    }

    auto security_tap = adopt_own(*new SecurityTap(move(socket), config));
    dbgln("SecurityTap: Connected to Sentinel at {}", config.network.socket_path);

    return security_tap;
}

SecurityTap::SecurityTap(NonnullOwnPtr<Core::LocalSocket> socket, WebView::SentinelConfig config)
    : m_sentinel_socket(move(socket))
    , m_config(move(config))
    , m_connection_failed(false)
{
}
```

3. **Update reconnect()** to use config:
```cpp
ErrorOr<void> SecurityTap::reconnect()
{
    dbgln("SecurityTap: Attempting to reconnect to Sentinel...");

    // Retry with configured retry count and delay
    for (u32 attempt = 0; attempt < m_config.network.connect_retry_count; ++attempt) {
        auto socket_result = Core::LocalSocket::connect(m_config.network.socket_path);

        if (!socket_result.is_error()) {
            m_sentinel_socket = socket_result.release_value();
            m_connection_failed = false;

            // Set socket timeout
            struct timeval timeout;
            timeout.tv_sec = m_config.network.socket_timeout_ms / 1000;
            timeout.tv_usec = (m_config.network.socket_timeout_ms % 1000) * 1000;

            auto fd = m_sentinel_socket->fd();
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

            dbgln("SecurityTap: Successfully reconnected to Sentinel (attempt {})", attempt + 1);
            return {};
        }

        dbgln("SecurityTap: Reconnection attempt {} failed: {}", attempt + 1, socket_result.error());

        if (attempt + 1 < m_config.network.connect_retry_count) {
            usleep(m_config.network.connect_retry_delay_ms * 1000);
        }
    }

    return Error::from_string_literal("Failed to reconnect after all retry attempts");
}
```

4. **Update inspect_download()** to use configured limits:
```cpp
ErrorOr<SecurityTap::ScanResult> SecurityTap::inspect_download(
    DownloadMetadata const& metadata,
    ReadonlyBytes content)
{
    // Check against configured maximum scan size
    if (content.size() > m_config.scan.max_scan_size_bytes) {
        dbgln("SecurityTap: File too large to scan ({}MB > {}MB)",
            content.size() / (1024 * 1024),
            m_config.scan.max_scan_size_bytes / (1024 * 1024));

        if (m_config.scan.fail_open_on_large_file) {
            // Create warning alert
            JsonObject alert;
            alert.set("type"sv, "file_too_large"sv);
            alert.set("filename"sv, metadata.filename);
            alert.set("size_mb"sv, static_cast<u64>(content.size() / (1024 * 1024)));
            alert.set("max_size_mb"sv, static_cast<u64>(m_config.scan.max_scan_size_bytes / (1024 * 1024)));
            alert.set("action"sv, "allowed"sv);
            alert.set("recommendation"sv, "Review file manually before opening"sv);

            return ScanResult {
                .is_threat = false,
                .alert_json = alert.serialized()
            };
        } else {
            // Fail-closed: Block large files
            return ScanResult {
                .is_threat = true,
                .alert_json = "File exceeds maximum scan size and fail-closed policy is enabled"_string
            };
        }
    }

    // Check against configured minimum scan size
    if (content.size() < m_config.scan.min_scan_size_bytes) {
        dbgln("SecurityTap: Skipping scan for small file ({}B < {}B)",
            content.size(), m_config.scan.min_scan_size_bytes);
        return ScanResult { .is_threat = false, .alert_json = {} };
    }

    // Send scan request to Sentinel (with error recovery)
    auto response_json_result = send_scan_request(metadata, content);

    // Handle Sentinel communication failures with configured fail policy
    if (response_json_result.is_error()) {
        dbgln("SecurityTap: Sentinel scan request failed: {}", response_json_result.error());

        if (m_config.scan.fail_open_on_error) {
            dbgln("SecurityTap: Allowing download without scanning (fail-open)");
            return ScanResult { .is_threat = false, .alert_json = {} };
        } else {
            dbgln("SecurityTap: Blocking download due to scan failure (fail-closed)");
            return ScanResult {
                .is_threat = true,
                .alert_json = "Scan failed and fail-closed policy is enabled"_string
            };
        }
    }

    // Rest of function unchanged...
}
```

5. **Update async_inspect_download()** to use async threshold:
```cpp
void SecurityTap::async_inspect_download(
    DownloadMetadata const& metadata,
    ReadonlyBytes content,
    ScanCallback callback)
{
    // Check if async scanning should be used based on configured threshold
    if (content.size() < m_config.scan.async_scan_threshold_bytes ||
        !m_config.scan.async_scan_enabled) {
        // Small file or async disabled - scan synchronously
        auto result = inspect_download(metadata, content);
        callback(move(result));
        return;
    }

    // Large file - scan asynchronously
    dbgln("SecurityTap: Using async scan for large file ({}MB)",
        content.size() / (1024 * 1024));

    // Rest of async implementation...
}
```

**Testing**:
```cpp
// Test with custom config
WebView::SentinelConfig config;
config.scan.max_scan_size_bytes = 50 * 1024 * 1024;  // 50MB
config.scan.fail_open_on_large_file = false;  // Fail-closed

auto tap = TRY(SecurityTap::create(config));

// Test large file with fail-closed
auto large_data = ByteBuffer::create_zeroed(60 * 1024 * 1024).release_value();
auto result = tap->inspect_download(metadata, large_data.bytes());
EXPECT(result.value().is_threat);  // Should block

// Test with environment variables
Core::System::setenv("SENTINEL_MAX_SCAN_SIZE_MB"sv, "200"sv);
Core::System::setenv("SENTINEL_FAIL_OPEN"sv, "false"sv);
auto tap2 = TRY(SecurityTap::create());
// Verify tap2 uses env var config
```

---

### Task 3: Update SentinelServer to Use Configuration (CRITICAL)
**Time**: 1 hour
**File**: `Services/Sentinel/SentinelServer.cpp`

**Current Problem**:
- Line 45: Hardcoded YARA rules path `/home/rbsmith4/ladybird/Services/Sentinel/rules/default.yar`
- Line 95: Hardcoded socket path `/tmp/sentinel.sock`
- No timeout configuration for YARA scans
- No configuration for max rules loaded

**Solution**: Load configuration and use throughout

**Changes Required**:

1. **Add config member** to `Services/Sentinel/SentinelServer.h`:
```cpp
#include <LibWebView/SentinelConfig.h>

class SentinelServer {
public:
    static ErrorOr<NonnullOwnPtr<SentinelServer>> create();
    static ErrorOr<NonnullOwnPtr<SentinelServer>> create(WebView::SentinelConfig const& config);

private:
    SentinelServer(NonnullRefPtr<Core::LocalServer>, WebView::SentinelConfig config);

    NonnullRefPtr<Core::LocalServer> m_server;
    WebView::SentinelConfig m_config;  // NEW
    Vector<NonnullOwnPtr<Core::LocalSocket>> m_clients;
};
```

2. **Update initialize_yara()** to use config:
```cpp
static ErrorOr<void> initialize_yara(WebView::YaraConfig const& config)
{
    dbgln("Sentinel: Initializing YARA from {}", config.rules_directory);

    int result = yr_initialize();
    if (result != ERROR_SUCCESS)
        return Error::from_string_literal("Failed to initialize YARA");

    YR_COMPILER* compiler = nullptr;
    result = yr_compiler_create(&compiler);
    if (result != ERROR_SUCCESS) {
        yr_finalize();
        return Error::from_string_literal("Failed to create YARA compiler");
    }

    // Load rules from configured directory
    Vector<String> rule_files_to_load;

    if (!config.rule_files.is_empty()) {
        // Load specific rule files
        rule_files_to_load = config.rule_files;
    } else {
        // Load all .yar files from directory
        auto entries = TRY(Core::Directory::read_entries(config.rules_directory));
        for (auto const& entry : entries) {
            if (entry.name.ends_with(".yar"sv)) {
                rule_files_to_load.append(entry.name);
            }
        }
    }

    if (rule_files_to_load.is_empty()) {
        dbgln("Sentinel: Warning - no YARA rule files found in {}", config.rules_directory);
    }

    // Check against max_rules limit
    if (rule_files_to_load.size() > config.max_rules) {
        dbgln("Sentinel: Warning - found {} rule files, limiting to {} (max_rules)",
            rule_files_to_load.size(), config.max_rules);
        rule_files_to_load.shrink(config.max_rules);
    }

    // Load each rule file
    for (auto const& rule_file : rule_files_to_load) {
        StringBuilder rule_path_builder;
        rule_path_builder.append(config.rules_directory);
        if (!config.rules_directory.ends_with("/"sv))
            rule_path_builder.append('/');
        rule_path_builder.append(rule_file);
        auto rule_path = TRY(rule_path_builder.to_string());

        dbgln("Sentinel: Loading YARA rules from {}", rule_path);

        auto rules_file = Core::File::open(rule_path, Core::File::OpenMode::Read);
        if (rules_file.is_error()) {
            dbgln("Sentinel: Warning - failed to open {}: {}", rule_path, rules_file.error());
            continue;  // Skip this file, try others
        }

        auto rules_content = rules_file.value()->read_until_eof();
        if (rules_content.is_error()) {
            dbgln("Sentinel: Warning - failed to read {}: {}", rule_path, rules_content.error());
            continue;
        }

        // Add rules to compiler
        result = yr_compiler_add_string(compiler,
            reinterpret_cast<char const*>(rules_content.value().data()),
            nullptr);

        if (result != 0) {
            dbgln("Sentinel: Warning - failed to compile rules from {}", rule_path);
            continue;  // Try other files
        }

        dbgln("Sentinel: Loaded {} successfully", rule_path);
    }

    // Get compiled rules
    result = yr_compiler_get_rules(compiler, &s_yara_rules);
    if (result != ERROR_SUCCESS) {
        yr_compiler_destroy(compiler);
        yr_finalize();
        return Error::from_string_literal("Failed to get compiled YARA rules");
    }

    yr_compiler_destroy(compiler);

    dbgln("Sentinel: YARA initialized successfully with {} rule files", rule_files_to_load.size());
    return {};
}
```

3. **Update SentinelServer::create()** to use config:
```cpp
ErrorOr<NonnullOwnPtr<SentinelServer>> SentinelServer::create()
{
    // Load configuration (environment > file > defaults)
    auto config_result = WebView::SentinelConfig::load_from_env();
    if (config_result.is_error()) {
        config_result = WebView::SentinelConfig::load_default();
    }

    if (config_result.is_error()) {
        return Error::from_string_literal("Failed to load Sentinel configuration");
    }

    auto config = config_result.release_value();
    return create(config);
}

ErrorOr<NonnullOwnPtr<SentinelServer>> SentinelServer::create(WebView::SentinelConfig const& config)
{
    // Validate configuration
    TRY(config.validate());

    if (!config.enabled) {
        return Error::from_string_literal("Sentinel is disabled in configuration");
    }

    // Initialize YARA rules with configuration
    TRY(initialize_yara(config.yara));

    // Remove stale socket file if it exists
    if (FileSystem::exists(config.network.socket_path)) {
        auto remove_result = FileSystem::remove(config.network.socket_path,
            FileSystem::RecursionMode::Disallowed);
        if (remove_result.is_error()) {
            dbgln("Sentinel: Warning - could not remove stale socket: {}", remove_result.error());
        }
    }

    // Ensure parent directory exists for socket
    auto socket_dir = FileSystem::parent_directory(config.network.socket_path);
    if (socket_dir.has_value() && !FileSystem::exists(socket_dir.value())) {
        TRY(Core::Directory::create(socket_dir.value(), Core::Directory::CreateDirectories::Yes));
        dbgln("Sentinel: Created socket directory: {}", socket_dir.value());
    }

    // Create and bind socket server
    auto server = TRY(Core::LocalServer::try_create());
    TRY(server->take_over_from_system_server(config.network.socket_path));

    dbgln("Sentinel: Listening on {}", config.network.socket_path);

    auto sentinel_server = adopt_own(*new SentinelServer(move(server), config));

    return sentinel_server;
}

SentinelServer::SentinelServer(NonnullRefPtr<Core::LocalServer> server, WebView::SentinelConfig config)
    : m_server(move(server))
    , m_config(move(config))
{
    m_server->on_accept = [this](auto client) {
        handle_client(move(client));
    };
}
```

4. **Update scan_content()** to use YARA timeout:
```cpp
ErrorOr<ByteString> SentinelServer::scan_content(ReadonlyBytes content)
{
    if (!s_yara_rules) {
        return Error::from_string_literal("YARA rules not initialized");
    }

    // Check against configured max scan size
    if (content.size() > m_config.scan.max_scan_size_bytes) {
        dbgln("Sentinel: Content too large to scan ({}MB)", content.size() / (1024 * 1024));
        return "[]"_byte_string;  // No matches
    }

    // Set scan timeout
    YR_SCAN_CONTEXT context;
    context.timeout = m_config.yara.scan_timeout_seconds;

    Vector<ByteString> matched_rules;

    // Scan with configured timeout
    int result = yr_rules_scan_mem(
        s_yara_rules,
        reinterpret_cast<uint8_t const*>(content.data()),
        content.size(),
        SCAN_FLAGS_FAST_MODE * m_config.yara.enable_fast_mode,
        [](YR_SCAN_CONTEXT* context, int message, void* message_data, void* user_data) -> int {
            if (message == CALLBACK_MSG_RULE_MATCHING) {
                auto* rule = static_cast<YR_RULE*>(message_data);
                auto* matches = static_cast<Vector<ByteString>*>(user_data);
                matches->append(ByteString(rule->identifier));
            }
            return CALLBACK_CONTINUE;
        },
        &matched_rules);

    if (result == ERROR_SCAN_TIMEOUT) {
        dbgln("Sentinel: YARA scan timed out after {}s", m_config.yara.scan_timeout_seconds);

        if (m_config.scan.fail_open_on_timeout) {
            return "[]"_byte_string;  // Fail-open on timeout
        } else {
            return Error::from_string_literal("YARA scan timed out");
        }
    }

    if (result != ERROR_SUCCESS) {
        return Error::from_string_literal("YARA scan failed");
    }

    // Format results as JSON array
    JsonArray results;
    for (auto const& rule : matched_rules) {
        results.must_append(rule);
    }

    return results.serialized<StringBuilder>();
}
```

**Testing**:
```cpp
// Test with custom config
WebView::SentinelConfig config;
config.yara.rules_directory = "/path/to/test/rules"_string;
config.yara.scan_timeout_seconds = 10;
config.yara.enable_fast_mode = true;
config.network.socket_path = "/tmp/test_sentinel.sock"_string;

auto server = TRY(SentinelServer::create(config));

// Verify server using correct socket path
EXPECT(FileSystem::exists("/tmp/test_sentinel.sock"));

// Test with environment variables
Core::System::setenv("SENTINEL_YARA_RULES_DIR"sv, "/etc/sentinel/rules"sv);
Core::System::setenv("SENTINEL_SOCKET_PATH"sv, "/run/sentinel/sentinel.sock"sv);
auto server2 = TRY(SentinelServer::create());
// Verify server2 uses env var config
```

---

### Task 4: Update PolicyGraph to Use Configuration (HIGH)
**Time**: 45 minutes
**File**: `Services/Sentinel/PolicyGraph.cpp`

**Current Problem**:
- Line 38: Hardcoded cache size `PolicyGraphCache(1000)`
- No limits on max policies or threats in database
- No automatic cleanup configuration

**Solution**: Use configuration for cache and database limits

**Changes Required**:

1. **Update PolicyGraph::create()** in `Services/Sentinel/PolicyGraph.cpp`:
```cpp
ErrorOr<NonnullOwnPtr<PolicyGraph>> PolicyGraph::create(String const& db_path)
{
    // Load configuration
    auto config_result = WebView::SentinelConfig::load_from_env();
    if (config_result.is_error()) {
        config_result = WebView::SentinelConfig::load_default();
    }

    if (config_result.is_error()) {
        // Fallback to hardcoded defaults
        return create_with_config(db_path, WebView::SentinelConfig::create_default());
    }

    return create_with_config(db_path, config_result.release_value());
}

ErrorOr<NonnullOwnPtr<PolicyGraph>> PolicyGraph::create_with_config(
    String const& db_path,
    WebView::SentinelConfig const& config)
{
    // Initialize cache with configured size
    auto cache = PolicyGraphCache(config.performance.policy_cache_size);

    // Open database
    auto database = TRY(SQL::Database::try_create(db_path));

    // Configure SQLite based on config
    if (config.database.enable_wal_mode) {
        TRY(database->execute_statement("PRAGMA journal_mode=WAL"_string));
    }

    if (config.database.busy_timeout_ms > 0) {
        auto timeout_stmt = ByteString::formatted("PRAGMA busy_timeout={}",
            config.database.busy_timeout_ms);
        TRY(database->execute_statement(timeout_stmt));
    }

    // Initialize schema
    TRY(initialize_database(*database));

    // Check database size and vacuum if needed
    auto db_size = FileSystem::size_from_stat(db_path).value_or(0);
    size_t db_size_mb = db_size / (1024 * 1024);

    if (db_size_mb > config.database.vacuum_threshold_mb) {
        dbgln("PolicyGraph: Database size ({}MB) exceeds threshold ({}MB), running VACUUM",
            db_size_mb, config.database.vacuum_threshold_mb);
        TRY(database->execute_statement("VACUUM"_string));
    }

    auto policy_graph = adopt_own(*new PolicyGraph(move(database), move(cache), config));

    // Check policy/threat counts against limits
    auto policy_count = TRY(policy_graph->count_policies());
    auto threat_count = TRY(policy_graph->count_threats());

    if (policy_count > config.database.max_policies) {
        dbgln("PolicyGraph: Warning - policy count ({}) exceeds max_policies ({})",
            policy_count, config.database.max_policies);
        // TODO: Auto-cleanup oldest policies?
    }

    if (threat_count > config.database.max_threats) {
        dbgln("PolicyGraph: Warning - threat count ({}) exceeds max_threats ({})",
            threat_count, config.database.max_threats);
        // Auto-cleanup old threats
        TRY(policy_graph->cleanup_old_threats(config.threat_retention_days));
    }

    return policy_graph;
}

PolicyGraph::PolicyGraph(
    NonnullRefPtr<SQL::Database> database,
    PolicyGraphCache cache,
    WebView::SentinelConfig config)
    : m_database(move(database))
    , m_cache(move(cache))
    , m_config(move(config))
{
}
```

2. **Add helper methods** for database maintenance:
```cpp
ErrorOr<size_t> PolicyGraph::count_policies() const
{
    size_t count = 0;
    auto statement = TRY(m_database->prepare_statement("SELECT COUNT(*) FROM policies"_string));
    TRY(m_database->execute_statement(*statement, {}, [&](auto statement_id) {
        count = m_database->result_column<i64>(statement_id, 0);
    }));
    return count;
}

ErrorOr<size_t> PolicyGraph::count_threats() const
{
    size_t count = 0;
    auto statement = TRY(m_database->prepare_statement("SELECT COUNT(*) FROM threat_history"_string));
    TRY(m_database->execute_statement(*statement, {}, [&](auto statement_id) {
        count = m_database->result_column<i64>(statement_id, 0);
    }));
    return count;
}

ErrorOr<void> PolicyGraph::cleanup_old_threats(u32 retention_days)
{
    auto cutoff_time = UnixDateTime::now().seconds_since_epoch() - (retention_days * 86400);

    auto statement = TRY(m_database->prepare_statement(
        "DELETE FROM threat_history WHERE detected_at < ?"_string));

    TRY(m_database->execute_statement(*statement, {}, cutoff_time));

    dbgln("PolicyGraph: Cleaned up threats older than {} days", retention_days);
    return {};
}

ErrorOr<void> PolicyGraph::enforce_policy_limit()
{
    auto count = TRY(count_policies());

    if (count <= m_config.database.max_policies)
        return {};

    // Delete oldest policies to get under limit
    size_t to_delete = count - m_config.database.max_policies;

    auto statement = TRY(m_database->prepare_statement(R"(
        DELETE FROM policies
        WHERE id IN (
            SELECT id FROM policies
            ORDER BY created_at ASC
            LIMIT ?
        )
    )"_string));

    TRY(m_database->execute_statement(*statement, {}, static_cast<i64>(to_delete)));

    dbgln("PolicyGraph: Deleted {} old policies to enforce limit", to_delete);
    m_cache.invalidate();  // Invalidate cache after bulk delete

    return {};
}
```

3. **Update create_policy()** to check limits:
```cpp
ErrorOr<Policy> PolicyGraph::create_policy(Policy const& policy)
{
    // Check if we're at policy limit
    auto count = TRY(count_policies());
    if (count >= m_config.database.max_policies) {
        dbgln("PolicyGraph: At policy limit ({}), enforcing limit", m_config.database.max_policies);
        TRY(enforce_policy_limit());
    }

    // Rest of existing create_policy implementation...
}
```

**Testing**:
```cpp
// Test with custom config
WebView::SentinelConfig config;
config.performance.policy_cache_size = 500;
config.database.max_policies = 100;
config.database.max_threats = 1000;

auto pg = TRY(PolicyGraph::create_with_config(db_path, config));

// Create 101 policies - should auto-delete oldest
for (int i = 0; i < 101; i++) {
    Policy p;
    p.rule_name = ByteString::formatted("test_rule_{}", i);
    TRY(pg->create_policy(p));
}

auto count = TRY(pg->count_policies());
EXPECT_EQ(count, 100u);  // Should have enforced limit
```

---

## Afternoon Session (4 hours): Monitoring and Diagnostic Tools

### Task 5: Enhance Metrics Collection System (HIGH)
**Time**: 1.5 hours
**Files**:
- `Services/Sentinel/SentinelMetrics.h` (existing)
- `Services/Sentinel/SentinelMetrics.cpp` (existing)

**Current State**: Metrics system exists but needs enhancement for production monitoring

**Enhancements Required**:

1. **Add configuration-aware metrics** in `Services/Sentinel/SentinelMetrics.h`:
```cpp
struct ConfigurationMetrics {
    // Current configuration snapshot
    size_t policy_cache_size;
    size_t max_scan_size_mb;
    size_t socket_timeout_ms;
    String yara_rules_directory;
    size_t yara_rule_files_loaded;
    bool fail_open_on_error;
    bool async_scan_enabled;
    size_t thread_pool_size;

    // Configuration change tracking
    UnixDateTime last_config_reload;
    size_t config_reload_count;
    size_t config_validation_failures;
};

struct ResourceMetrics {
    // Memory usage
    size_t heap_size_bytes;
    size_t cache_memory_bytes;

    // Disk usage
    size_t database_size_bytes;
    size_t quarantine_size_bytes;
    size_t log_files_size_bytes;

    // File descriptors
    size_t open_file_descriptors;
    size_t max_file_descriptors;

    // Threads
    size_t active_threads;
    size_t thread_pool_size;
};

struct ErrorMetrics {
    // Error counts by category
    size_t scan_errors;
    size_t database_errors;
    size_t network_errors;
    size_t yara_errors;
    size_t config_errors;

    // Error rate (errors per minute)
    double error_rate;

    // Last error
    String last_error_message;
    UnixDateTime last_error_time;
};

struct SentinelMetrics {
    // Existing fields...

    // New sections
    ConfigurationMetrics configuration;
    ResourceMetrics resources;
    ErrorMetrics errors;

    // Operational metrics
    size_t failover_count;              // Fail-open activations
    size_t timeout_count;               // Operation timeouts
    size_t retry_count;                 // Connection retries

    // Computed metrics
    double scan_throughput_mbps() const;     // MB/s
    double query_latency_p50_ms() const;     // 50th percentile
    double query_latency_p95_ms() const;     // 95th percentile
    double query_latency_p99_ms() const;     // 99th percentile
};
```

2. **Add histogram for latency tracking**:
```cpp
class LatencyHistogram {
public:
    void record(Duration latency) {
        m_samples.append(latency.to_milliseconds());
        if (m_samples.size() > 10000) {
            m_samples.remove(0);  // Keep last 10k samples
        }
    }

    double percentile(double p) const {
        if (m_samples.is_empty())
            return 0.0;

        auto sorted = m_samples;
        quick_sort(sorted);

        size_t index = static_cast<size_t>(p * sorted.size());
        if (index >= sorted.size())
            index = sorted.size() - 1;

        return sorted[index];
    }

    void clear() {
        m_samples.clear();
    }

private:
    Vector<double> m_samples;
};

class MetricsCollector {
public:
    static MetricsCollector& the();

    // Enhanced recording methods
    void record_scan_latency(Duration latency, size_t bytes_scanned);
    void record_query_latency(Duration latency);
    void record_error(String const& category, String const& message);
    void record_failover();
    void record_timeout();
    void record_config_reload();

    // Resource tracking
    void update_resource_metrics();

    // Snapshot with computed metrics
    SentinelMetrics snapshot() const;

    // Metrics export formats
    String to_prometheus() const;      // Prometheus format
    String to_json() const;            // JSON format
    String to_human_readable() const;  // Pretty-printed text

private:
    MetricsCollector() = default;

    SentinelMetrics m_metrics;
    LatencyHistogram m_scan_latencies;
    LatencyHistogram m_query_latencies;
    mutable Mutex m_mutex;
};
```

3. **Implement enhanced metrics collection** in `Services/Sentinel/SentinelMetrics.cpp`:
```cpp
void MetricsCollector::record_scan_latency(Duration latency, size_t bytes_scanned)
{
    MutexLocker locker(m_mutex);

    m_scan_latencies.record(latency);
    m_metrics.total_scan_time = m_metrics.total_scan_time + latency;
    m_metrics.scan_count++;
    m_metrics.last_scan = UnixDateTime::now();

    // Track bytes scanned for throughput calculation
    m_total_bytes_scanned += bytes_scanned;
}

void MetricsCollector::record_query_latency(Duration latency)
{
    MutexLocker locker(m_mutex);

    m_query_latencies.record(latency);
    m_metrics.total_query_time = m_metrics.total_query_time + latency;
    m_metrics.query_count++;
}

void MetricsCollector::record_error(String const& category, String const& message)
{
    MutexLocker locker(m_mutex);

    if (category == "scan"sv)
        m_metrics.errors.scan_errors++;
    else if (category == "database"sv)
        m_metrics.errors.database_errors++;
    else if (category == "network"sv)
        m_metrics.errors.network_errors++;
    else if (category == "yara"sv)
        m_metrics.errors.yara_errors++;
    else if (category == "config"sv)
        m_metrics.errors.config_errors++;

    m_metrics.errors.last_error_message = message;
    m_metrics.errors.last_error_time = UnixDateTime::now();

    // Calculate error rate (errors per minute over last hour)
    auto now = UnixDateTime::now().seconds_since_epoch();
    auto hour_ago = now - 3600;
    // ... error rate calculation
}

void MetricsCollector::update_resource_metrics()
{
    MutexLocker locker(m_mutex);

    // Get memory usage (Linux-specific)
#ifdef __linux__
    auto status = Core::File::open("/proc/self/status"sv, Core::File::OpenMode::Read);
    if (!status.is_error()) {
        auto content = status.value()->read_until_eof().release_value();
        // Parse VmSize and VmRSS
        // ...
    }
#endif

    // Get file descriptor count
#ifdef __linux__
    auto fd_dir = Core::Directory::read_entries("/proc/self/fd"sv);
    if (!fd_dir.is_error()) {
        m_metrics.resources.open_file_descriptors = fd_dir.value().size();
    }
#endif

    // Get resource limits
    struct rlimit limit;
    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        m_metrics.resources.max_file_descriptors = limit.rlim_cur;
    }
}

String MetricsCollector::to_prometheus() const
{
    MutexLocker locker(m_mutex);

    StringBuilder builder;

    // Scan metrics
    builder.appendff("sentinel_downloads_scanned_total {}\n", m_metrics.total_downloads_scanned);
    builder.appendff("sentinel_threats_detected_total {}\n", m_metrics.threats_detected);
    builder.appendff("sentinel_threats_blocked_total {}\n", m_metrics.threats_blocked);
    builder.appendff("sentinel_threats_quarantined_total {}\n", m_metrics.threats_quarantined);

    // Policy metrics
    builder.appendff("sentinel_policies_total {}\n", m_metrics.total_policies);
    builder.appendff("sentinel_policies_enforced_total {}\n", m_metrics.policies_enforced);

    // Cache metrics
    builder.appendff("sentinel_cache_hits_total {}\n", m_metrics.cache_hits);
    builder.appendff("sentinel_cache_misses_total {}\n", m_metrics.cache_misses);
    builder.appendff("sentinel_cache_hit_rate {:.3f}\n",
        m_metrics.cache_hits * 100.0 / (m_metrics.cache_hits + m_metrics.cache_misses));

    // Latency metrics
    builder.appendff("sentinel_scan_latency_p50_ms {:.2f}\n", m_scan_latencies.percentile(0.50));
    builder.appendff("sentinel_scan_latency_p95_ms {:.2f}\n", m_scan_latencies.percentile(0.95));
    builder.appendff("sentinel_scan_latency_p99_ms {:.2f}\n", m_scan_latencies.percentile(0.99));

    builder.appendff("sentinel_query_latency_p50_ms {:.2f}\n", m_query_latencies.percentile(0.50));
    builder.appendff("sentinel_query_latency_p95_ms {:.2f}\n", m_query_latencies.percentile(0.95));
    builder.appendff("sentinel_query_latency_p99_ms {:.2f}\n", m_query_latencies.percentile(0.99));

    // Resource metrics
    builder.appendff("sentinel_memory_bytes {}\n", m_metrics.resources.heap_size_bytes);
    builder.appendff("sentinel_database_size_bytes {}\n", m_metrics.resources.database_size_bytes);
    builder.appendff("sentinel_quarantine_size_bytes {}\n", m_metrics.resources.quarantine_size_bytes);
    builder.appendff("sentinel_open_fds {}\n", m_metrics.resources.open_file_descriptors);

    // Error metrics
    builder.appendff("sentinel_scan_errors_total {}\n", m_metrics.errors.scan_errors);
    builder.appendff("sentinel_database_errors_total {}\n", m_metrics.errors.database_errors);
    builder.appendff("sentinel_network_errors_total {}\n", m_metrics.errors.network_errors);

    // Operational metrics
    builder.appendff("sentinel_failover_count_total {}\n", m_metrics.failover_count);
    builder.appendff("sentinel_timeout_count_total {}\n", m_metrics.timeout_count);

    // Uptime
    auto uptime = UnixDateTime::now().seconds_since_epoch() - m_metrics.started_at.seconds_since_epoch();
    builder.appendff("sentinel_uptime_seconds {}\n", uptime);

    return builder.to_string().release_value_but_fixme_should_propagate_errors();
}
```

**Integration**: Add metrics endpoint to SecurityUI for Prometheus scraping

**Testing**:
```cpp
// Test metrics collection
auto& metrics = MetricsCollector::the();

// Record some operations
metrics.record_scan_latency(Duration::from_milliseconds(25), 1024 * 1024);
metrics.record_query_latency(Duration::from_milliseconds(5));
metrics.record_error("scan"sv, "Test error"_string);

// Get snapshot
auto snapshot = metrics.snapshot();
EXPECT_EQ(snapshot.scan_count, 1u);
EXPECT_EQ(snapshot.query_count, 1u);
EXPECT_EQ(snapshot.errors.scan_errors, 1u);

// Test Prometheus format
auto prom = metrics.to_prometheus();
EXPECT(prom.contains("sentinel_downloads_scanned_total"sv));
EXPECT(prom.contains("sentinel_scan_latency_p95_ms"sv));
```

---

### Task 6: Enhance Diagnostic CLI Tool (MEDIUM)
**Time**: 1.5 hours
**File**: `Tools/sentinel-cli.cpp` (existing)

**Current State**: CLI tool exists with basic commands

**Enhancements Required**:

1. **Add config management commands**:
```cpp
// In Tools/sentinel-cli.cpp

static ErrorOr<int> cmd_config_show()
{
    outln("=== Sentinel Configuration ===\n");

    auto config = TRY(WebView::SentinelConfig::load_default());

    outln("Enabled: {}", config.enabled ? "YES" : "NO");
    outln("");

    outln("Network:");
    outln("  Socket Path: {}", config.network.socket_path);
    outln("  Socket Timeout: {}ms", config.network.socket_timeout_ms);
    outln("  Retry Count: {}", config.network.connect_retry_count);
    outln("");

    outln("Scanning:");
    outln("  Max Scan Size: {}MB", config.scan.max_scan_size_bytes / (1024 * 1024));
    outln("  Fail Open on Error: {}", config.scan.fail_open_on_error ? "YES" : "NO");
    outln("  Async Enabled: {}", config.scan.async_scan_enabled ? "YES" : "NO");
    outln("  Async Threshold: {}MB", config.scan.async_scan_threshold_bytes / (1024 * 1024));
    outln("");

    outln("Database:");
    outln("  Path: {}", config.database.database_path);
    outln("  Max Policies: {}", config.database.max_policies);
    outln("  Max Threats: {}", config.database.max_threats);
    outln("  WAL Mode: {}", config.database.enable_wal_mode ? "ENABLED" : "DISABLED");
    outln("");

    outln("YARA:");
    outln("  Rules Directory: {}", config.yara.rules_directory);
    outln("  Scan Timeout: {}s", config.yara.scan_timeout_seconds);
    outln("  Max Rules: {}", config.yara.max_rules);
    outln("  Fast Mode: {}", config.yara.enable_fast_mode ? "ENABLED" : "DISABLED");
    outln("");

    outln("Quarantine:");
    outln("  Path: {}", config.quarantine.quarantine_path);
    outln("  Max File Size: {}MB", config.quarantine.max_file_size_bytes / (1024 * 1024));
    outln("  Max Total Size: {}GB", config.quarantine.max_total_size_bytes / (1024 * 1024 * 1024));
    outln("  Max Files: {}", config.quarantine.max_file_count);
    outln("  Retention: {} days", config.quarantine.retention_days);

    return 0;
}

static ErrorOr<int> cmd_config_validate()
{
    outln("Validating configuration...");

    auto config = TRY(WebView::SentinelConfig::load_default());

    auto validation_result = config.validate();
    if (validation_result.is_error()) {
        outln("ERROR: Configuration validation failed:");
        outln("  {}", validation_result.error());
        return 1;
    }

    outln("Configuration is valid.");
    return 0;
}

static ErrorOr<int> cmd_config_generate_default()
{
    outln("Generating default configuration...");

    auto config = WebView::SentinelConfig::create_default();
    auto config_path = TRY(WebView::SentinelConfig::default_config_path());

    TRY(config.save_to_file(config_path));

    outln("Default configuration written to: {}", config_path);
    return 0;
}
```

2. **Add metrics command**:
```cpp
static ErrorOr<int> cmd_metrics()
{
    outln("=== Sentinel Metrics ===\n");

    // Connect to Sentinel to get metrics via IPC
    auto socket = TRY(Core::LocalSocket::connect("/tmp/sentinel.sock"sv));

    // Send metrics request
    JsonObject request;
    request.set("type"sv, "get_metrics"sv);
    auto request_json = TRY(request.to_string());
    TRY(socket->write_until_depleted(request_json.bytes()));
    TRY(socket->write_until_depleted("\n"sv.bytes()));

    // Read response
    Array<u8, 65536> buffer;
    auto bytes_read = TRY(socket->read_some(buffer));
    auto response_json = TRY(String::from_utf8(bytes_read));

    auto response = TRY(JsonValue::from_string(response_json));
    auto const& metrics = response.as_object();

    // Display metrics
    outln("Scan Statistics:");
    outln("  Total Downloads Scanned: {}", metrics.get_u64("total_downloads_scanned"sv).value_or(0));
    outln("  Threats Detected: {}", metrics.get_u64("threats_detected"sv).value_or(0));
    outln("  Threats Blocked: {}", metrics.get_u64("threats_blocked"sv).value_or(0));
    outln("  Threats Quarantined: {}", metrics.get_u64("threats_quarantined"sv).value_or(0));
    outln("");

    outln("Policy Statistics:");
    outln("  Total Policies: {}", metrics.get_u64("total_policies"sv).value_or(0));
    outln("  Policies Enforced: {}", metrics.get_u64("policies_enforced"sv).value_or(0));
    outln("");

    outln("Cache Statistics:");
    outln("  Cache Hits: {}", metrics.get_u64("cache_hits"sv).value_or(0));
    outln("  Cache Misses: {}", metrics.get_u64("cache_misses"sv).value_or(0));
    auto hits = metrics.get_u64("cache_hits"sv).value_or(0);
    auto misses = metrics.get_u64("cache_misses"sv).value_or(0);
    if (hits + misses > 0) {
        double hit_rate = (hits * 100.0) / (hits + misses);
        outln("  Hit Rate: {:.1f}%", hit_rate);
    }
    outln("");

    outln("Performance:");
    outln("  Avg Scan Time: {:.2f}ms", metrics.get_double("avg_scan_time_ms"sv).value_or(0.0));
    outln("  Avg Query Time: {:.2f}ms", metrics.get_double("avg_query_time_ms"sv).value_or(0.0));
    outln("  P95 Scan Latency: {:.2f}ms", metrics.get_double("scan_latency_p95_ms"sv).value_or(0.0));
    outln("  P99 Scan Latency: {:.2f}ms", metrics.get_double("scan_latency_p99_ms"sv).value_or(0.0));
    outln("");

    outln("Resource Usage:");
    outln("  Database Size: {}MB", metrics.get_u64("database_size_bytes"sv).value_or(0) / (1024 * 1024));
    outln("  Quarantine Size: {}MB", metrics.get_u64("quarantine_size_bytes"sv).value_or(0) / (1024 * 1024));
    outln("  Quarantine Files: {}", metrics.get_u64("quarantine_file_count"sv).value_or(0));
    outln("");

    outln("System Health:");
    auto uptime = metrics.get_u64("uptime_seconds"sv).value_or(0);
    outln("  Uptime: {}d {}h {}m", uptime / 86400, (uptime % 86400) / 3600, (uptime % 3600) / 60);

    return 0;
}
```

3. **Add diagnostics command**:
```cpp
static ErrorOr<int> cmd_diagnose()
{
    outln("=== Sentinel Diagnostics ===\n");

    bool all_checks_passed = true;

    // Check 1: Configuration validity
    outln("[1/8] Checking configuration...");
    auto config_result = WebView::SentinelConfig::load_default();
    if (config_result.is_error()) {
        outln("  ✗ FAIL: Cannot load configuration: {}", config_result.error());
        all_checks_passed = false;
    } else {
        auto validation = config_result.value().validate();
        if (validation.is_error()) {
            outln("  ✗ FAIL: Configuration invalid: {}", validation.error());
            all_checks_passed = false;
        } else {
            outln("  ✓ PASS");
        }
    }

    // Check 2: Socket accessibility
    outln("[2/8] Checking Sentinel socket...");
    auto socket_path = config_result.is_error() ?
        "/tmp/sentinel.sock"_string :
        config_result.value().network.socket_path;

    if (!FileSystem::exists(socket_path)) {
        outln("  ✗ FAIL: Socket does not exist at {}", socket_path);
        outln("         Sentinel daemon may not be running");
        all_checks_passed = false;
    } else {
        auto socket_result = Core::LocalSocket::connect(socket_path);
        if (socket_result.is_error()) {
            outln("  ✗ FAIL: Cannot connect to socket: {}", socket_result.error());
            all_checks_passed = false;
        } else {
            outln("  ✓ PASS");
        }
    }

    // Check 3: Database accessibility
    outln("[3/8] Checking database...");
    auto db_path = config_result.is_error() ?
        "~/.local/share/Ladybird/PolicyGraph/policies.db"_string :
        config_result.value().database.database_path;

    if (!FileSystem::exists(db_path)) {
        outln("  ✗ FAIL: Database does not exist at {}", db_path);
        all_checks_passed = false;
    } else {
        auto db_result = SQL::Database::try_create(db_path);
        if (db_result.is_error()) {
            outln("  ✗ FAIL: Cannot open database: {}", db_result.error());
            all_checks_passed = false;
        } else {
            outln("  ✓ PASS");
        }
    }

    // Check 4: YARA rules
    outln("[4/8] Checking YARA rules...");
    auto rules_dir = config_result.is_error() ?
        "~/.config/ladybird/sentinel/rules"_string :
        config_result.value().yara.rules_directory;

    if (!FileSystem::exists(rules_dir)) {
        outln("  ✗ FAIL: Rules directory does not exist at {}", rules_dir);
        all_checks_passed = false;
    } else {
        auto entries = Core::Directory::read_entries(rules_dir);
        if (entries.is_error()) {
            outln("  ✗ FAIL: Cannot read rules directory: {}", entries.error());
            all_checks_passed = false;
        } else {
            size_t yar_count = 0;
            for (auto const& entry : entries.value()) {
                if (entry.name.ends_with(".yar"sv))
                    yar_count++;
            }
            if (yar_count == 0) {
                outln("  ✗ FAIL: No .yar rule files found in {}", rules_dir);
                all_checks_passed = false;
            } else {
                outln("  ✓ PASS ({} rule files)", yar_count);
            }
        }
    }

    // Check 5: Quarantine directory
    outln("[5/8] Checking quarantine directory...");
    auto quarantine_path = config_result.is_error() ?
        "~/.local/share/Ladybird/Quarantine"_string :
        config_result.value().quarantine.quarantine_path;

    if (!FileSystem::exists(quarantine_path)) {
        outln("  ⚠ WARN: Quarantine directory does not exist (will be created on demand)");
    } else {
        // Check permissions
        auto stat_result = Core::System::stat(quarantine_path);
        if (stat_result.is_error()) {
            outln("  ✗ FAIL: Cannot stat quarantine directory: {}", stat_result.error());
            all_checks_passed = false;
        } else {
            auto mode = stat_result.value().st_mode & 0777;
            if (mode != 0700) {
                outln("  ⚠ WARN: Quarantine permissions are {:o} (should be 0700)", mode);
            } else {
                outln("  ✓ PASS");
            }
        }
    }

    // Check 6: Memory limits
    outln("[6/8] Checking resource limits...");
    struct rlimit limit;
    if (getrlimit(RLIMIT_AS, &limit) == 0) {
        if (limit.rlim_cur != RLIM_INFINITY && limit.rlim_cur < 512 * 1024 * 1024) {
            outln("  ⚠ WARN: Memory limit is low ({}MB)", limit.rlim_cur / (1024 * 1024));
        } else {
            outln("  ✓ PASS");
        }
    }

    // Check 7: File descriptor limits
    outln("[7/8] Checking file descriptor limits...");
    if (getrlimit(RLIMIT_NOFILE, &limit) == 0) {
        if (limit.rlim_cur < 1024) {
            outln("  ⚠ WARN: File descriptor limit is low ({})", limit.rlim_cur);
        } else {
            outln("  ✓ PASS ({})", limit.rlim_cur);
        }
    }

    // Check 8: Disk space
    outln("[8/8] Checking disk space...");
    struct statvfs stat;
    if (statvfs("/", &stat) == 0) {
        unsigned long available_mb = (stat.f_bavail * stat.f_bsize) / (1024 * 1024);
        if (available_mb < 1024) {  // Less than 1GB
            outln("  ⚠ WARN: Low disk space ({}MB available)", available_mb);
        } else {
            outln("  ✓ PASS ({}MB available)", available_mb);
        }
    }

    outln("");
    if (all_checks_passed) {
        outln("All critical checks passed.");
        return 0;
    } else {
        outln("Some checks failed. Review the output above.");
        return 1;
    }
}
```

4. **Update main() to add new commands**:
```cpp
int main(int argc, char** argv)
{
    // ... existing setup ...

    if (command == "status"sv) {
        return TRY(cmd_status());
    } else if (command == "list-policies"sv) {
        return TRY(cmd_list_policies());
    } else if (command == "config"sv) {
        if (argc < 3) {
            outln("Usage: sentinel-cli config <show|validate|generate-default>");
            return 1;
        }
        String subcommand = argv[2];
        if (subcommand == "show"sv)
            return TRY(cmd_config_show());
        else if (subcommand == "validate"sv)
            return TRY(cmd_config_validate());
        else if (subcommand == "generate-default"sv)
            return TRY(cmd_config_generate_default());
        else {
            outln("Unknown config subcommand: {}", subcommand);
            return 1;
        }
    } else if (command == "metrics"sv) {
        return TRY(cmd_metrics());
    } else if (command == "diagnose"sv) {
        return TRY(cmd_diagnose());
    }
    // ... existing commands ...
}
```

**Testing**:
```bash
# Test config commands
sentinel-cli config show
sentinel-cli config validate
sentinel-cli config generate-default

# Test metrics
sentinel-cli metrics

# Test diagnostics
sentinel-cli diagnose
```

---

### Task 7: Add Configuration Reload Support (MEDIUM)
**Time**: 1 hour
**Files**:
- `Services/Sentinel/SentinelServer.h`
- `Services/Sentinel/SentinelServer.cpp`

**Current Problem**: Configuration changes require full restart

**Solution**: Add IPC command for configuration reload + SIGHUP handler

**Changes Required**:

1. **Add reload method** to `Services/Sentinel/SentinelServer.h`:
```cpp
class SentinelServer {
public:
    // ... existing methods ...

    ErrorOr<void> reload_configuration();

private:
    // ... existing members ...

    void setup_signal_handlers();
    static void handle_sighup(int);
    static SentinelServer* s_instance;
};
```

2. **Implement configuration reload** in `Services/Sentinel/SentinelServer.cpp`:
```cpp
SentinelServer* SentinelServer::s_instance = nullptr;

ErrorOr<NonnullOwnPtr<SentinelServer>> SentinelServer::create(WebView::SentinelConfig const& config)
{
    // ... existing setup ...

    auto sentinel_server = adopt_own(*new SentinelServer(move(server), config));
    s_instance = sentinel_server.get();
    sentinel_server->setup_signal_handlers();

    return sentinel_server;
}

void SentinelServer::setup_signal_handlers()
{
    // Handle SIGHUP for configuration reload
    struct sigaction sa;
    sa.sa_handler = &SentinelServer::handle_sighup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, nullptr);

    dbgln("Sentinel: Registered SIGHUP handler for configuration reload");
}

void SentinelServer::handle_sighup(int)
{
    if (s_instance) {
        dbgln("Sentinel: Received SIGHUP, reloading configuration...");
        auto result = s_instance->reload_configuration();
        if (result.is_error()) {
            dbgln("Sentinel: Configuration reload failed: {}", result.error());
        }
    }
}

ErrorOr<void> SentinelServer::reload_configuration()
{
    dbgln("Sentinel: Reloading configuration...");

    // Load new configuration
    auto new_config_result = WebView::SentinelConfig::load_from_env();
    if (new_config_result.is_error()) {
        new_config_result = WebView::SentinelConfig::load_default();
    }

    if (new_config_result.is_error()) {
        return Error::from_string_literal("Failed to load new configuration");
    }

    auto new_config = new_config_result.release_value();

    // Validate new configuration
    TRY(new_config.validate());

    // Check for changes that require restart
    bool needs_restart = false;

    if (new_config.network.socket_path != m_config.network.socket_path) {
        dbgln("Sentinel: Warning - socket_path changed, requires restart");
        needs_restart = true;
    }

    if (new_config.yara.rules_directory != m_config.yara.rules_directory) {
        dbgln("Sentinel: YARA rules directory changed, reloading rules...");

        // Reload YARA rules
        if (s_yara_rules) {
            yr_rules_destroy(s_yara_rules);
            s_yara_rules = nullptr;
        }

        auto yara_result = initialize_yara(new_config.yara);
        if (yara_result.is_error()) {
            dbgln("Sentinel: Failed to reload YARA rules: {}", yara_result.error());
            return yara_result.release_error();
        }

        dbgln("Sentinel: YARA rules reloaded successfully");
    }

    // Apply new configuration
    m_config = move(new_config);

    // Update metrics
    MetricsCollector::the().record_config_reload();

    if (needs_restart) {
        dbgln("Sentinel: Some configuration changes require a restart to take effect");
    }

    dbgln("Sentinel: Configuration reloaded successfully");
    return {};
}
```

3. **Add IPC message for reload**:
```cpp
ErrorOr<void> SentinelServer::process_message(Core::LocalSocket& socket, String const& message)
{
    // ... existing message parsing ...

    if (type == "reload_config"sv) {
        auto reload_result = reload_configuration();

        JsonObject response;
        if (reload_result.is_error()) {
            response.set("status"sv, "error"sv);
            response.set("error"sv, ByteString(reload_result.error().string_literal()));
        } else {
            response.set("status"sv, "success"sv);
            response.set("message"sv, "Configuration reloaded"sv);
        }

        auto response_json = TRY(response.to_string());
        TRY(socket.write_until_depleted(response_json.bytes()));
        TRY(socket.write_until_depleted("\n"sv.bytes()));
        return {};
    }

    // ... rest of message handling ...
}
```

4. **Add CLI command for reload**:
```cpp
// In Tools/sentinel-cli.cpp

static ErrorOr<int> cmd_reload_config()
{
    outln("Reloading Sentinel configuration...");

    auto socket = TRY(Core::LocalSocket::connect("/tmp/sentinel.sock"sv));

    JsonObject request;
    request.set("type"sv, "reload_config"sv);
    auto request_json = TRY(request.to_string());

    TRY(socket->write_until_depleted(request_json.bytes()));
    TRY(socket->write_until_depleted("\n"sv.bytes()));

    // Read response
    Array<u8, 4096> buffer;
    auto bytes_read = TRY(socket->read_some(buffer));
    auto response_json = TRY(String::from_utf8(bytes_read));

    auto response = TRY(JsonValue::from_string(response_json));
    auto const& resp_obj = response.as_object();

    if (resp_obj.get_string("status"sv).value() == "success"sv) {
        outln("Configuration reloaded successfully.");
        return 0;
    } else {
        outln("ERROR: {}", resp_obj.get_string("error"sv).value());
        return 1;
    }
}

// Add to main()
else if (command == "reload"sv) {
    return TRY(cmd_reload_config());
}
```

**Testing**:
```bash
# Edit config file
vim ~/.config/ladybird/sentinel/config.json

# Reload without restart
sentinel-cli reload

# Or send SIGHUP
kill -HUP $(pidof Sentinel)

# Verify new config applied
sentinel-cli config show
```

---

## Summary

### Day 34 Completion Checklist

**Morning Session (Configuration)**:
- [x] Comprehensive configuration structure (8 sections, 50+ settings)
- [x] Environment variable support (SENTINEL_* prefix, 20+ variables)
- [x] Configuration validation with bounds checking
- [x] Updated SecurityTap to use configuration
- [x] Updated SentinelServer to use configuration
- [x] Updated PolicyGraph to use configuration

**Afternoon Session (Monitoring & Tools)**:
- [x] Enhanced metrics collection (resource, error, latency tracking)
- [x] Prometheus metrics export format
- [x] Enhanced CLI with config, metrics, diagnostics commands
- [x] Configuration reload support (SIGHUP + IPC)

**Configuration Issues Resolved**:
- Critical: Socket path now uses XDG_RUNTIME_DIR (was `/tmp`)
- Critical: Socket timeout now configurable (was FIXME)
- Critical: YARA rules path now configurable (was hardcoded with username)
- High: Environment variable support added (full coverage)
- High: Configuration reload implemented (no restart needed)
- High: Configuration validation prevents invalid configs
- Medium: Scan size limits now configurable (was 100MB hardcoded)

**New Configuration Options** (57+ hardcoded values → configured):
1. Network: socket_path, socket_timeout_ms, retry_count, retry_delay_ms
2. Scan: max_size, min_size, fail_open_on_error, fail_open_on_timeout, async_enabled, async_threshold
3. Database: path, max_policies, max_threats, vacuum_threshold, wal_mode, busy_timeout
4. Quarantine: path, max_file_size, max_total_size, max_file_count, retention_days
5. YARA: rules_directory, rule_files, scan_timeout, max_rules, fast_mode
6. Logging: level, log_to_file, log_file_path, max_log_size, max_log_files
7. Performance: cache_size, thread_pool_size, cache_ttl, cache_persistence
8. Rate Limiting: policies_per_minute, rate_window_seconds
9. Notifications: enabled, auto_dismiss_seconds, max_queue_size

**Environment Variable Support**:
```bash
# Example production deployment with env vars
export SENTINEL_SOCKET_PATH="/run/sentinel/sentinel.sock"
export SENTINEL_MAX_SCAN_SIZE_MB="200"
export SENTINEL_FAIL_OPEN="false"  # Fail-closed for production
export SENTINEL_YARA_RULES_DIR="/etc/sentinel/rules"
export SENTINEL_DATABASE_PATH="/var/lib/sentinel/policies.db"
export SENTINEL_CACHE_SIZE="5000"
export SENTINEL_LOG_LEVEL="info"
export SENTINEL_LOG_FILE="/var/log/sentinel/sentinel.log"
./Sentinel
```

**CLI Commands Added**:
- `sentinel-cli config show` - Display current configuration
- `sentinel-cli config validate` - Validate configuration file
- `sentinel-cli config generate-default` - Create default config
- `sentinel-cli metrics` - Display operational metrics
- `sentinel-cli diagnose` - Run system diagnostics (8 checks)
- `sentinel-cli reload` - Reload configuration without restart

**Metrics Enhancements**:
- Latency histograms (P50, P95, P99 percentiles)
- Resource tracking (memory, disk, file descriptors)
- Error categorization (scan, database, network, YARA, config)
- Configuration metrics (current settings, reload count)
- Prometheus export format for monitoring integration

---

**Testing Approach**:

1. **Configuration Tests**:
```cpp
// Test validation
config.network.socket_timeout_ms = 100000;
EXPECT(config.validate().is_error());

// Test env var loading
setenv("SENTINEL_MAX_SCAN_SIZE_MB", "200");
auto config = TRY(SentinelConfig::load_from_env());
EXPECT_EQ(config.scan.max_scan_size_bytes, 200 * 1024 * 1024);

// Test reload
auto server = TRY(SentinelServer::create());
// Modify config file
TRY(server->reload_configuration());
```

2. **Integration Tests**:
```bash
# Deploy with env vars
export SENTINEL_SOCKET_PATH="/tmp/test.sock"
./Sentinel

# Verify using custom socket
sentinel-cli status  # Should connect to /tmp/test.sock

# Test reload
echo '{"scan": {"max_scan_size_bytes": 209715200}}' > config.json
sentinel-cli reload
sentinel-cli config show  # Should show 200MB limit
```

3. **Production Validation**:
```bash
# Run diagnostics
sentinel-cli diagnose

# Check metrics
sentinel-cli metrics

# Validate config before deployment
sentinel-cli config validate
```

---

**Next**: Phase 5 Day 35 - Milestone 0.2 Foundation (Credential Exfiltration Detection)
