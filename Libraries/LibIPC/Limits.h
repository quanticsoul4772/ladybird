/*
 * Copyright (c) 2025, the Ladybird Browser Contributors
 * Copyright (c) 2026, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace IPC {

// Maximum size of an IPC message payload (64 MiB should be more than enough)
static constexpr size_t MAX_MESSAGE_PAYLOAD_SIZE = 64 * MiB;

// Maximum number of file descriptors per message
static constexpr size_t MAX_MESSAGE_FD_COUNT = 128;

// Fork-specific: Additional security limits for IPC DoS prevention
namespace Limits {

// String length limit (1 MiB)
// Rationale: Covers long page titles, URLs, and text content
static constexpr size_t MaxStringLength = 1024 * 1024;

// Vector size limit (1M elements)
// Rationale: Allows large arrays while preventing memory exhaustion
static constexpr size_t MaxVectorSize = 1024 * 1024;

// ByteBuffer size limit (16 MiB)
// Rationale: Matches reasonable payload sizes
static constexpr size_t MaxByteBufferSize = 16 * 1024 * 1024;

// HashMap size limit (100K entries)
// Rationale: Covers HTTP headers, cookies, localStorage
static constexpr size_t MaxHashMapSize = 100 * 1024;

// Nesting depth limit (recursion protection)
// Rationale: Prevents stack overflow in recursive deserialization
static constexpr size_t MaxNestingDepth = 32;

// URL length limit (per RFC 7230)
// Rationale: Most servers/browsers use 8KB limit
static constexpr size_t MaxURLLength = 8192;

// Cookie size limit (per RFC 6265)
// Rationale: Standard cookie size limit
static constexpr size_t MaxCookieSize = 4096;

// HTTP header count limit
// Rationale: Prevents header bombing attacks
static constexpr size_t MaxHTTPHeaderCount = 100;

// HTTP header value size limit
// Rationale: Reasonable size for header values
static constexpr size_t MaxHTTPHeaderValueSize = 8192;

// Image dimension limits
// Rationale: 16K x 16K is larger than any reasonable display
static constexpr u32 MaxImageWidth = 16384;
static constexpr u32 MaxImageHeight = 16384;

// File size limit for uploads (100 MiB)
// Rationale: Balance between functionality and DoS prevention
static constexpr size_t MaxFileUploadSize = 100 * 1024 * 1024;

// Proxy/Network security limits
// Hostname length limit (per RFC 1035)
static constexpr size_t MaxHostnameLength = 255;

// Port number limit (well-known valid range)
static constexpr u16 MinPortNumber = 1;
static constexpr u16 MaxPortNumber = 65535;

// Authentication credential limits
static constexpr size_t MaxUsernameLength = 256;
static constexpr size_t MaxPasswordLength = 1024;

// Tor circuit ID limit
static constexpr size_t MaxCircuitIDLength = 128;

// Rate limiting defaults
// Rationale: Prevent IPC message flooding from compromised processes
static constexpr size_t DefaultRateLimit = 1000; // messages per second
static constexpr size_t MaxRateLimit = 10000;    // maximum configurable rate

// Proxy validation timeout (milliseconds)
static constexpr u32 ProxyValidationTimeoutMS = 2000; // 2 seconds

} // namespace Limits

}
