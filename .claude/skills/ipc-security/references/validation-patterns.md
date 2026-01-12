# Input Validation Reference

Comprehensive patterns and code templates for validating IPC input in Ladybird's multi-process architecture.

## Table of Contents
1. [Integer Validation](#1-integer-validation)
2. [String Validation](#2-string-validation)
3. [Buffer Validation](#3-buffer-validation)
4. [URL Validation](#4-url-validation)
5. [File Path Validation](#5-file-path-validation)
6. [Enum Validation](#6-enum-validation)
7. [Composite Validation](#7-composite-validation)

---

## 1. Integer Validation

### 1.1 Checked Arithmetic (Overflow Prevention)

**Use Case:** Prevent integer overflow in size calculations, array indexing.

**Pattern:**
```cpp
#include <AK/Checked.h>

// Addition with overflow check
ErrorOr<size_t> safe_add(size_t a, size_t b)
{
    auto result = AK::checked_add(a, b);
    if (result.has_overflow())
        return Error::from_string_literal("Addition overflow");
    return result.value();
}

// Multiplication with overflow check
ErrorOr<size_t> safe_multiply(size_t a, size_t b)
{
    auto result = AK::checked_mul(a, b);
    if (result.has_overflow())
        return Error::from_string_literal("Multiplication overflow");
    return result.value();
}

// Subtraction with overflow check
ErrorOr<size_t> safe_subtract(size_t a, size_t b)
{
    auto result = AK::checked_sub(a, b);
    if (result.has_overflow())
        return Error::from_string_literal("Subtraction overflow");
    return result.value();
}
```

**Example Usage:**
```cpp
// Buffer allocation based on IPC dimensions
ErrorOr<ByteBuffer> allocate_image_buffer(u32 width, u32 height, u32 bpp)
{
    // Validate dimensions
    if (width > 16384 || height > 16384)
        return Error::from_string_literal("Dimensions too large");

    // Calculate size with overflow checking
    auto width_checked = AK::checked_cast<size_t>(width);
    auto height_checked = AK::checked_cast<size_t>(height);
    auto bpp_checked = AK::checked_cast<size_t>(bpp);

    auto bytes_per_row = AK::checked_mul(width_checked, bpp_checked);
    if (bytes_per_row.has_overflow())
        return Error::from_errno(EOVERFLOW);

    auto total_size = AK::checked_mul(bytes_per_row.value(), height_checked);
    if (total_size.has_overflow())
        return Error::from_errno(EOVERFLOW);

    // Check against absolute limit
    if (total_size.value() > 100 * 1024 * 1024)  // 100MB
        return Error::from_string_literal("Image too large");

    return ByteBuffer::create_uninitialized(total_size.value());
}
```

**Test Cases:**
```cpp
TEST_CASE(checked_arithmetic_detects_overflow)
{
    // u32 overflow
    auto result = allocate_image_buffer(0x10000, 0x10000, 4);
    EXPECT(result.is_error());  // 0x10000 * 0x10000 * 4 overflows

    // Valid allocation
    result = allocate_image_buffer(1024, 768, 4);
    EXPECT(!result.is_error());  // 3MB, valid

    // SIZE_MAX overflow
    result = allocate_image_buffer(0xFFFFFFFF, 0xFFFFFFFF, 4);
    EXPECT(result.is_error());
}
```

---

### 1.2 Range Validation

**Use Case:** Ensure numeric values are within acceptable ranges.

**Pattern:**
```cpp
template<typename T>
[[nodiscard]] bool validate_range(T value, T min, T max,
                                  StringView field_name,
                                  SourceLocation location = SourceLocation::current())
{
    if (value < min || value > max) {
        dbgln("Security: {} out of range ({}, valid {}-{}) at {}:{}",
              field_name, value, min, max,
              location.filename(), location.line_number());
        return false;
    }
    return true;
}

// Specialized for common types
[[nodiscard]] bool validate_percentage(u8 value,
                                       StringView field_name,
                                       SourceLocation location = SourceLocation::current())
{
    return validate_range(value, 0, 100, field_name, location);
}

[[nodiscard]] bool validate_port(u16 port,
                                 SourceLocation location = SourceLocation::current())
{
    if (port == 0) {
        dbgln("Security: Invalid port 0 at {}:{}",
              location.filename(), location.line_number());
        return false;
    }
    // Ports 1-1023 are privileged, 1024-65535 are valid
    return validate_range(port, 1, 65535, "port"sv, location);
}

[[nodiscard]] bool validate_fd(int fd,
                               SourceLocation location = SourceLocation::current())
{
    if (fd < 0) {
        dbgln("Security: Invalid file descriptor {} at {}:{}",
              fd, location.filename(), location.line_number());
        return false;
    }
    // FDs up to ulimit -n (typically 1024-4096)
    if (fd > 4096) {
        dbgln("Security: Suspicious file descriptor {} at {}:{}",
              fd, location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
void handle_set_volume(u8 volume)
{
    if (!validate_percentage(volume, "volume"sv))
        return;

    m_audio_player->set_volume(volume);
}

void handle_connect(String host, u16 port)
{
    if (!validate_port(port))
        return;

    m_connection->connect(host, port);
}
```

---

### 1.3 Sign Validation

**Use Case:** Prevent negative values where only positive makes sense.

**Pattern:**
```cpp
template<typename T>
    requires std::is_signed_v<T>
[[nodiscard]] bool validate_positive(T value,
                                     StringView field_name,
                                     SourceLocation location = SourceLocation::current())
{
    if (value <= 0) {
        dbgln("Security: {} must be positive (got {}) at {}:{}",
              field_name, value, location.filename(), location.line_number());
        return false;
    }
    return true;
}

template<typename T>
    requires std::is_signed_v<T>
[[nodiscard]] bool validate_non_negative(T value,
                                         StringView field_name,
                                         SourceLocation location = SourceLocation::current())
{
    if (value < 0) {
        dbgln("Security: {} must be non-negative (got {}) at {}:{}",
              field_name, value, location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
void handle_allocate_buffer(i32 size)
{
    if (!validate_positive(size, "size"sv))
        return;

    auto buffer = ByteBuffer::create_uninitialized(size);
}

void handle_set_position(i32 x, i32 y)
{
    if (!validate_non_negative(x, "x"sv) || !validate_non_negative(y, "y"sv))
        return;

    m_position = { x, y };
}
```

---

## 2. String Validation

### 2.1 Length Validation

**Use Case:** Prevent oversized strings that could exhaust memory or cause buffer overflows.

**Pattern:**
```cpp
#include <LibIPC/Limits.h>

[[nodiscard]] bool validate_string_length(StringView string,
                                          StringView field_name,
                                          SourceLocation location = SourceLocation::current())
{
    if (string.length() > IPC::Limits::MaxStringLength) {  // 10,000,000 (10MB)
        dbgln("Security: Oversized {} ({} bytes, max {}) at {}:{}",
              field_name, string.length(), IPC::Limits::MaxStringLength,
              location.filename(), location.line_number());
        return false;
    }
    return true;
}

// Specialized validators for common string types
[[nodiscard]] bool validate_header_name(StringView name,
                                        SourceLocation location = SourceLocation::current())
{
    // HTTP header names: max 256 chars
    if (name.length() > 256) {
        dbgln("Security: Oversized header name ({} bytes, max 256) at {}:{}",
              name.length(), location.filename(), location.line_number());
        return false;
    }
    return true;
}

[[nodiscard]] bool validate_header_value(StringView value,
                                         SourceLocation location = SourceLocation::current())
{
    // HTTP header values: max 8KB
    if (value.length() > 8192) {
        dbgln("Security: Oversized header value ({} bytes, max 8192) at {}:{}",
              value.length(), location.filename(), location.line_number());
        return false;
    }
    return true;
}

[[nodiscard]] bool validate_filename(StringView filename,
                                     SourceLocation location = SourceLocation::current())
{
    // Filenames: max 255 chars (Linux NAME_MAX)
    if (filename.length() > 255) {
        dbgln("Security: Oversized filename ({} chars, max 255) at {}:{}",
              filename.length(), location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
void handle_set_name(String name)
{
    if (!validate_string_length(name, "name"sv))
        return;

    m_name = move(name);
}

void handle_add_header(String name, String value)
{
    if (!validate_header_name(name) || !validate_header_value(value))
        return;

    m_headers.set(move(name), move(value));
}
```

**Ladybird Reference:**
```cpp
// Services/RequestServer/ConnectionFromClient.h:242-252
bool validate_string_length(StringView string, StringView field_name,
                            SourceLocation location = SourceLocation::current())
{
    if (string.length() > IPC::Limits::MaxStringLength) {
        dbgln("Security: RequestServer sent oversized {} ({} bytes, max {}) at {}:{}",
              field_name, string.length(), IPC::Limits::MaxStringLength,
              location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}
```

---

### 2.2 Content Validation

**Use Case:** Prevent injection attacks, ensure strings contain only allowed characters.

**Pattern:**
```cpp
[[nodiscard]] bool validate_no_crlf(StringView string,
                                    StringView field_name,
                                    SourceLocation location = SourceLocation::current())
{
    if (string.contains('\r') || string.contains('\n')) {
        dbgln("Security: CRLF in {} at {}:{}",
              field_name, location.filename(), location.line_number());
        return false;
    }
    return true;
}

[[nodiscard]] bool validate_no_null_bytes(StringView string,
                                          StringView field_name,
                                          SourceLocation location = SourceLocation::current())
{
    if (string.contains('\0')) {
        dbgln("Security: Null byte in {} at {}:{}",
              field_name, location.filename(), location.line_number());
        return false;
    }
    return true;
}

[[nodiscard]] bool validate_alphanumeric(StringView string,
                                         StringView field_name,
                                         SourceLocation location = SourceLocation::current())
{
    for (auto ch : string) {
        if (!is_ascii_alphanumeric(ch) && ch != '_' && ch != '-') {
            dbgln("Security: Invalid character in {} at {}:{}",
                  field_name, location.filename(), location.line_number());
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool validate_no_shell_metacharacters(StringView string,
                                                     StringView field_name,
                                                     SourceLocation location = SourceLocation::current())
{
    // Dangerous shell characters: ; | & $ ` ( ) < > \n \r
    constexpr auto dangerous = ";|&$`()<>\n\r\\"sv;
    if (string.contains_any_of(dangerous)) {
        dbgln("Security: Shell metacharacter in {} at {}:{}",
              field_name, location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
// HTTP header validation
void handle_add_header(String name, String value)
{
    if (!validate_no_crlf(name, "header name"sv) ||
        !validate_no_crlf(value, "header value"sv))
        return;

    m_headers.set(move(name), move(value));
}

// File operation
void handle_execute_command(String command)
{
    if (!validate_no_shell_metacharacters(command, "command"sv))
        return;

    // Still use array-based spawn, not system()
    Core::Process::spawn({ command });
}
```

**Ladybird Reference:**
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
```

---

## 3. Buffer Validation

### 3.1 Size Validation

**Use Case:** Prevent oversized buffers that could exhaust memory.

**Pattern:**
```cpp
[[nodiscard]] bool validate_buffer_size(size_t size,
                                        StringView field_name,
                                        SourceLocation location = SourceLocation::current())
{
    // 100MB maximum for request bodies, WebSocket data, etc.
    static constexpr size_t MaxBufferSize = 100 * 1024 * 1024;
    if (size > MaxBufferSize) {
        dbgln("Security: Oversized {} ({} bytes, max {}) at {}:{}",
              field_name, size, MaxBufferSize,
              location.filename(), location.line_number());
        return false;
    }
    return true;
}

// Specialized validators
[[nodiscard]] bool validate_request_body_size(size_t size,
                                              SourceLocation location = SourceLocation::current())
{
    return validate_buffer_size(size, "request body"sv, location);
}

[[nodiscard]] bool validate_websocket_message_size(size_t size,
                                                    SourceLocation location = SourceLocation::current())
{
    // WebSocket messages: smaller limit (10MB)
    if (size > 10 * 1024 * 1024) {
        dbgln("Security: Oversized WebSocket message ({} bytes, max 10MB) at {}:{}",
              size, location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
void handle_send_data(ByteBuffer data)
{
    if (!validate_buffer_size(data.size(), "data"sv))
        return;

    m_connection->send(move(data));
}
```

**Ladybird Reference:**
```cpp
// Services/RequestServer/ConnectionFromClient.h:254-266
bool validate_buffer_size(size_t size, StringView field_name,
                          SourceLocation location = SourceLocation::current())
{
    static constexpr size_t MaxRequestBodySize = 100 * 1024 * 1024;
    if (size > MaxRequestBodySize) {
        dbgln("Security: RequestServer sent oversized {} ({} bytes, max {}) at {}:{}",
              field_name, size, MaxRequestBodySize,
              location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}
```

---

### 3.2 Bounds Checking

**Use Case:** Verify buffer access is within bounds.

**Pattern:**
```cpp
template<typename T>
[[nodiscard]] ErrorOr<T> safe_read_at(ByteBuffer const& buffer, size_t offset)
{
    if (offset + sizeof(T) > buffer.size())
        return Error::from_string_literal("Read out of bounds");

    T value;
    memcpy(&value, buffer.data() + offset, sizeof(T));
    return value;
}

[[nodiscard]] ErrorOr<void> safe_write_at(ByteBuffer& buffer, size_t offset,
                                          ReadonlyBytes data)
{
    auto end_offset = AK::checked_add(offset, data.size());
    if (end_offset.has_overflow())
        return Error::from_errno(EOVERFLOW);

    if (end_offset.value() > buffer.size())
        return Error::from_string_literal("Write out of bounds");

    memcpy(buffer.data() + offset, data.data(), data.size());
    return {};
}
```

**Example Usage:**
```cpp
ErrorOr<u32> read_header(ByteBuffer const& buffer)
{
    // Read 4-byte header at offset 0
    return safe_read_at<u32>(buffer, 0);
}

ErrorOr<void> write_payload(ByteBuffer& buffer, size_t offset, ByteBuffer const& payload)
{
    return safe_write_at(buffer, offset, payload.bytes());
}
```

---

## 4. URL Validation

### 4.1 Scheme Validation

**Use Case:** Restrict URLs to allowed schemes, prevent file:// access, etc.

**Pattern:**
```cpp
[[nodiscard]] bool validate_url_scheme(URL::URL const& url,
                                       SourceLocation location = SourceLocation::current())
{
    // Allow only http, https, ipfs, ipns
    if (!url.scheme().is_one_of("http"sv, "https"sv, "ipfs"sv, "ipns"sv)) {
        dbgln("Security: Disallowed URL scheme '{}' at {}:{}",
              url.scheme(), location.filename(), location.line_number());
        return false;
    }
    return true;
}

[[nodiscard]] bool validate_http_url(URL::URL const& url,
                                     SourceLocation location = SourceLocation::current())
{
    if (!url.scheme().is_one_of("http"sv, "https"sv)) {
        dbgln("Security: Non-HTTP URL scheme '{}' at {}:{}",
              url.scheme(), location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
void handle_navigate(URL::URL url)
{
    if (!validate_url_scheme(url))
        return;

    m_page->load(move(url));
}
```

**Ladybird Reference:**
```cpp
// Services/RequestServer/ConnectionFromClient.h:219-240
bool validate_url(URL::URL const& url, SourceLocation location = SourceLocation::current())
{
    // Length validation
    auto url_string = url.to_string();
    if (url_string.bytes_as_string_view().length() > IPC::Limits::MaxURLLength) {
        dbgln("Security: Oversized URL ({} bytes, max {}) at {}:{}",
              url_string.bytes_as_string_view().length(),
              IPC::Limits::MaxURLLength, location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }

    // Scheme validation
    if (!url.scheme().is_one_of("http"sv, "https"sv, "ipfs"sv, "ipns"sv)) {
        dbgln("Security: Disallowed URL scheme '{}' at {}:{}",
              url.scheme(), location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }

    return true;
}
```

---

### 4.2 Host Validation (SSRF Prevention)

**Use Case:** Prevent Server-Side Request Forgery by blocking internal IPs.

**Pattern:**
```cpp
[[nodiscard]] bool is_private_ip(StringView host)
{
    // Localhost
    if (host == "127.0.0.1" || host == "localhost" || host == "::1")
        return true;

    // Private IPv4 ranges
    if (host.starts_with("10."))
        return true;
    if (host.starts_with("192.168."))
        return true;
    if (host.starts_with("172.")) {
        // 172.16.0.0 - 172.31.255.255
        auto parts = host.split_view('.');
        if (parts.size() >= 2) {
            auto second_octet = parts[1].to_number<int>();
            if (second_octet.has_value() && second_octet.value() >= 16 && second_octet.value() <= 31)
                return true;
        }
    }

    // Link-local (AWS metadata, etc.)
    if (host.starts_with("169.254."))
        return true;

    // IPv6 link-local
    if (host.starts_with("fe80:"))
        return true;

    return false;
}

[[nodiscard]] bool validate_url_not_internal(URL::URL const& url,
                                             SourceLocation location = SourceLocation::current())
{
    if (is_private_ip(url.host())) {
        dbgln("Security: Attempt to access private IP '{}' at {}:{}",
              url.host(), location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
void handle_fetch(URL::URL url)
{
    if (!validate_url_scheme(url))
        return;

    // Prevent SSRF
    if (!validate_url_not_internal(url))
        return;

    m_loader->load(move(url));
}
```

---

### 4.3 URL Length Validation

**Use Case:** Prevent oversized URLs that could exhaust memory.

**Pattern:**
```cpp
[[nodiscard]] bool validate_url_length(URL::URL const& url,
                                       SourceLocation location = SourceLocation::current())
{
    auto url_string = url.to_string();
    if (url_string.bytes_as_string_view().length() > IPC::Limits::MaxURLLength) {  // 10000
        dbgln("Security: Oversized URL ({} chars, max {}) at {}:{}",
              url_string.bytes_as_string_view().length(),
              IPC::Limits::MaxURLLength,
              location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

---

## 5. File Path Validation

### 5.1 Path Traversal Prevention

**Use Case:** Prevent `../` attacks that escape allowed directories.

**Pattern:**
```cpp
ErrorOr<String> validate_and_canonicalize_path(String const& path,
                                                String const& allowed_base_dir)
{
    // 1. Reject absolute paths from untrusted source
    if (path.starts_with('/') || path.starts_with('\\')) {
        return Error::from_string_literal("Absolute paths not allowed");
    }

    // 2. Reject paths with .. component
    if (path.contains(".."sv)) {
        return Error::from_string_literal("Path traversal not allowed");
    }

    // 3. Combine with allowed base directory
    auto full_path = String::formatted("{}/{}", allowed_base_dir, path);

    // 4. Canonicalize (resolve .., symlinks, etc.)
    auto canonical = TRY(FileSystem::real_path(full_path));

    // 5. Verify still within allowed directory
    if (!canonical.starts_with(allowed_base_dir)) {
        return Error::from_string_literal("Path escapes allowed directory");
    }

    return canonical;
}

ErrorOr<String> safe_resolve_path(String const& user_path, String const& base_dir)
{
    auto canonical = TRY(validate_and_canonicalize_path(user_path, base_dir));

    // Additional checks
    auto stat = TRY(FileSystem::stat(canonical));

    // Only regular files (no devices, sockets, symlinks)
    if (!FileSystem::is_regular_file(stat))
        return Error::from_string_literal("Not a regular file");

    // Check it's not a sensitive system file
    if (is_sensitive_path(canonical))
        return Error::from_string_literal("Access to sensitive file denied");

    return canonical;
}

bool is_sensitive_path(StringView path)
{
    // System files
    if (path.starts_with("/etc/"sv) ||
        path.starts_with("/proc/"sv) ||
        path.starts_with("/sys/"sv) ||
        path.starts_with("/dev/"sv))
        return true;

    // User dotfiles
    if (path.contains("/.ssh/"sv) ||
        path.contains("/.gnupg/"sv) ||
        path.ends_with("/.bashrc"sv) ||
        path.ends_with("/.bash_profile"sv))
        return true;

    return false;
}
```

**Example Usage:**
```cpp
void handle_read_file(String path)
{
    auto canonical = TRY(safe_resolve_path(path, m_download_directory));

    auto file = TRY(Core::File::open(canonical, Core::File::OpenMode::Read));
    auto contents = TRY(file->read_until_eof());
    send_file_contents(move(contents));
}
```

---

### 5.2 Filename Validation

**Use Case:** Ensure filenames are safe, don't contain path separators.

**Pattern:**
```cpp
[[nodiscard]] bool validate_filename(StringView filename,
                                     SourceLocation location = SourceLocation::current())
{
    // Length check (Linux NAME_MAX = 255)
    if (filename.length() > 255) {
        dbgln("Security: Oversized filename ({} chars, max 255) at {}:{}",
              filename.length(), location.filename(), location.line_number());
        return false;
    }

    // No path separators
    if (filename.contains('/') || filename.contains('\\')) {
        dbgln("Security: Path separator in filename at {}:{}",
              location.filename(), location.line_number());
        return false;
    }

    // No . or .. (hidden or traversal)
    if (filename == "."sv || filename == ".."sv) {
        dbgln("Security: Invalid filename '{}' at {}:{}",
              filename, location.filename(), location.line_number());
        return false;
    }

    // No null bytes
    if (filename.contains('\0')) {
        dbgln("Security: Null byte in filename at {}:{}",
              location.filename(), location.line_number());
        return false;
    }

    return true;
}

[[nodiscard]] bool validate_extension(StringView filename,
                                      Span<StringView const> allowed_extensions,
                                      SourceLocation location = SourceLocation::current())
{
    for (auto ext : allowed_extensions) {
        if (filename.ends_with(ext, CaseSensitivity::CaseInsensitive))
            return true;
    }

    dbgln("Security: Disallowed file extension for '{}' at {}:{}",
          filename, location.filename(), location.line_number());
    return false;
}
```

**Example Usage:**
```cpp
void handle_save_file(String filename, ByteBuffer data)
{
    if (!validate_filename(filename))
        return;

    // Only allow certain extensions
    static constexpr StringView allowed[] = { ".txt"sv, ".pdf"sv, ".png"sv, ".jpg"sv };
    if (!validate_extension(filename, allowed))
        return;

    auto full_path = String::formatted("{}/{}", m_download_dir, filename);
    auto file = TRY(Core::File::open(full_path, Core::File::OpenMode::Write));
    TRY(file->write_until_depleted(data));
}
```

---

## 6. Enum Validation

### 6.1 Explicit Value Checking

**Use Case:** Prevent invalid enum values that could cause incorrect behavior.

**Pattern:**
```cpp
enum class FileMode : u32 {
    Read = 0,
    Write = 1,
    ReadWrite = 2,
    // No Append = 3 (gap to demonstrate validation)
};

[[nodiscard]] bool validate_file_mode(FileMode mode,
                                      SourceLocation location = SourceLocation::current())
{
    switch (mode) {
    case FileMode::Read:
    case FileMode::Write:
    case FileMode::ReadWrite:
        return true;
    default:
        dbgln("Security: Invalid FileMode {} at {}:{}",
              to_underlying(mode), location.filename(), location.line_number());
        return false;
    }
}

// Generic enum validator
template<typename EnumType>
[[nodiscard]] bool validate_enum_in_set(EnumType value,
                                        std::initializer_list<EnumType> valid_values,
                                        StringView enum_name,
                                        SourceLocation location = SourceLocation::current())
{
    for (auto valid : valid_values) {
        if (value == valid)
            return true;
    }

    dbgln("Security: Invalid {} value {} at {}:{}",
          enum_name, to_underlying(value),
          location.filename(), location.line_number());
    return false;
}
```

**Example Usage:**
```cpp
void handle_open_file(String path, FileMode mode)
{
    if (!validate_file_mode(mode))
        return;

    // Safe to use enum
    auto file = open_file(path, mode);
}

enum class ColorScheme : u8 {
    Light = 0,
    Dark = 1,
    Auto = 2
};

void handle_set_color_scheme(ColorScheme scheme)
{
    if (!validate_enum_in_set(scheme, { ColorScheme::Light, ColorScheme::Dark, ColorScheme::Auto },
                              "ColorScheme"sv))
        return;

    m_color_scheme = scheme;
}
```

---

### 6.2 Range-Based Enum Validation

**Use Case:** Validate contiguous enum values.

**Pattern:**
```cpp
template<typename EnumType>
[[nodiscard]] bool validate_enum_range(EnumType value,
                                       EnumType min,
                                       EnumType max,
                                       StringView enum_name,
                                       SourceLocation location = SourceLocation::current())
{
    if (value < min || value > max) {
        dbgln("Security: {} out of range ({}, valid {}-{}) at {}:{}",
              enum_name, to_underlying(value),
              to_underlying(min), to_underlying(max),
              location.filename(), location.line_number());
        return false;
    }
    return true;
}
```

**Example Usage:**
```cpp
enum class Priority : u8 {
    Low = 0,
    Medium = 1,
    High = 2,
    Critical = 3
};

void handle_set_priority(Priority priority)
{
    if (!validate_enum_range(priority, Priority::Low, Priority::Critical, "Priority"sv))
        return;

    m_priority = priority;
}
```

---

## 7. Composite Validation

### 7.1 Vector Validation

**Use Case:** Validate vector size and element contents.

**Pattern:**
```cpp
template<typename T>
[[nodiscard]] bool validate_vector_size(Vector<T> const& vector,
                                        StringView field_name,
                                        SourceLocation location = SourceLocation::current())
{
    if (vector.size() > IPC::Limits::MaxVectorSize) {  // 10000
        dbgln("Security: Oversized {} ({} elements, max {}) at {}:{}",
              field_name, vector.size(), IPC::Limits::MaxVectorSize,
              location.filename(), location.line_number());
        return false;
    }
    return true;
}

// Validate vector with element validator
template<typename T, typename Validator>
[[nodiscard]] bool validate_vector_elements(Vector<T> const& vector,
                                            Validator validator,
                                            StringView field_name,
                                            SourceLocation location = SourceLocation::current())
{
    if (!validate_vector_size(vector, field_name, location))
        return false;

    for (size_t i = 0; i < vector.size(); ++i) {
        if (!validator(vector[i])) {
            dbgln("Security: Invalid element {} in {} at {}:{}",
                  i, field_name, location.filename(), location.line_number());
            return false;
        }
    }

    return true;
}
```

**Example Usage:**
```cpp
void handle_set_names(Vector<String> names)
{
    // Validate with element validator
    auto validator = [](String const& name) {
        return validate_string_length(name, "name"sv) &&
               !name.is_empty();
    };

    if (!validate_vector_elements(names, validator, "names"sv))
        return;

    m_names = move(names);
}
```

**Ladybird Reference:**
```cpp
// Services/RequestServer/ConnectionFromClient.h:268-280
template<typename T>
bool validate_vector_size(Vector<T> const& vector, StringView field_name,
                          SourceLocation location = SourceLocation::current())
{
    if (vector.size() > IPC::Limits::MaxVectorSize) {
        dbgln("Security: RequestServer sent oversized {} ({} elements, max {}) at {}:{}",
              field_name, vector.size(), IPC::Limits::MaxVectorSize,
              location.filename(), location.line_number());
        track_validation_failure();
        return false;
    }
    return true;
}
```

---

### 7.2 HashMap Validation

**Use Case:** Validate map size and key/value constraints.

**Pattern:**
```cpp
template<typename K, typename V>
[[nodiscard]] bool validate_map_size(HashMap<K, V> const& map,
                                     StringView field_name,
                                     SourceLocation location = SourceLocation::current())
{
    if (map.size() > IPC::Limits::MaxVectorSize) {
        dbgln("Security: Oversized {} ({} entries, max {}) at {}:{}",
              field_name, map.size(), IPC::Limits::MaxVectorSize,
              location.filename(), location.line_number());
        return false;
    }
    return true;
}

// HTTP::HeaderMap validation
[[nodiscard]] bool validate_header_map(HTTP::HeaderMap const& headers,
                                       SourceLocation location = SourceLocation::current())
{
    if (headers.headers().size() > IPC::Limits::MaxVectorSize) {
        dbgln("Security: Too many headers ({}, max {}) at {}:{}",
              headers.headers().size(), IPC::Limits::MaxVectorSize,
              location.filename(), location.line_number());
        return false;
    }

    for (auto const& header : headers.headers()) {
        if (!validate_header_name(header.name, location))
            return false;
        if (!validate_header_value(header.value, location))
            return false;
        if (!validate_no_crlf(header.name, "header name"sv, location))
            return false;
        if (!validate_no_crlf(header.value, "header value"sv, location))
            return false;
    }

    return true;
}
```

---

## Complete IPC Handler Template

**Full validation pattern for IPC message handler:**

```cpp
// Services/RequestServer/ConnectionFromClient.cpp

void ConnectionFromClient::handle_example_operation(
    i32 request_id,
    String name,
    URL::URL url,
    Vector<u32> data,
    HTTP::HeaderMap headers,
    ByteBuffer body,
    FileMode mode)
{
    // 1. Rate limiting (first line of defense)
    if (!check_rate_limit())
        return;

    // 2. Object ID validation
    if (!validate_request_id(request_id))
        return;

    // 3. String validation
    if (!validate_string_length(name, "name"sv))
        return;
    if (name.is_empty()) {
        dbgln("Security: Empty name in example_operation");
        track_validation_failure();
        return;
    }

    // 4. URL validation
    if (!validate_url(url))
        return;
    if (!validate_url_not_internal(url))
        return;

    // 5. Vector validation
    if (!validate_vector_size(data, "data"sv))
        return;

    // 6. Header validation
    if (!validate_header_map(headers))
        return;

    // 7. Buffer validation
    if (!validate_buffer_size(body.size(), "body"sv))
        return;

    // 8. Enum validation
    if (!validate_file_mode(mode))
        return;

    // 9. Business logic (all inputs validated)
    auto& request = *m_active_requests.get(request_id).value();
    auto result = request.process(name, url, data, headers, body, mode);

    if (result.is_error()) {
        dbgln("Error processing operation: {}", result.error());
        return;
    }
}
```

---

## Validation Test Template

```cpp
// Tests/Services/RequestServer/TestValidation.cpp

TEST_CASE(request_server_validates_all_inputs)
{
    // String length
    {
        auto huge_string = String::repeated('a', 20000000);  // > 10MB
        auto result = connection.example_operation(1, huge_string, /* ... */);
        EXPECT(connection.validation_failures() > 0);
    }

    // URL scheme
    {
        auto result = connection.example_operation(1, "test", URL::URL("file:///etc/passwd"), /* ... */);
        EXPECT(connection.validation_failures() > 0);
    }

    // Vector size
    {
        Vector<u32> huge_vec;
        for (int i = 0; i < 20000; ++i)  // > 10000 limit
            huge_vec.append(i);
        auto result = connection.example_operation(1, "test", url, huge_vec, /* ... */);
        EXPECT(connection.validation_failures() > 0);
    }

    // Buffer size
    {
        auto huge_buffer = ByteBuffer::create_uninitialized(200 * 1024 * 1024);  // > 100MB
        auto result = connection.example_operation(1, "test", url, {}, {}, huge_buffer, /* ... */);
        EXPECT(connection.validation_failures() > 0);
    }

    // Enum value
    {
        auto invalid_mode = static_cast<FileMode>(255);
        auto result = connection.example_operation(1, "test", url, {}, {}, {}, invalid_mode);
        EXPECT(connection.validation_failures() > 0);
    }
}
```

---

## References

- `Libraries/LibIPC/Limits.h` - Size limit constants
- `Services/RequestServer/ConnectionFromClient.h:197-348` - Validation implementation
- `.claude/skills/ipc-security/SKILL.md` - Security patterns
- `.claude/skills/ipc-security/references/known-vulnerabilities.md` - Vulnerability catalog
