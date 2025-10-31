/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/Time.h>
#include <LibCore/Socket.h>

namespace IPC {

// BufferedIPCReader: Handles partial IPC message reads with proper framing
//
// Problem: Socket read operations may return partial messages, leading to:
// - JSON parsing failures on incomplete data
// - Corrupted state from fragmented messages
// - Security vulnerabilities from truncated policy updates
//
// Solution: Message framing with length-prefixed protocol:
// - 4-byte length header (u32, network byte order)
// - Message payload (up to 10MB)
// - Buffering across multiple read() calls
// - Timeout protection (5 seconds default)
//
// Protocol Format:
// [Length: 4 bytes][Payload: Length bytes]
//
// Example Usage:
//   BufferedIPCReader reader;
//   auto message = TRY(reader.read_complete_message(socket));
//   // Process complete message...

class BufferedIPCReader {
public:
    BufferedIPCReader() = default;
    ~BufferedIPCReader() = default;

    // Read a complete IPC message from the socket, blocking until full message received
    // Returns: Complete message payload (without length header)
    // Errors:
    // - "Message too large" if length > MAX_MESSAGE_SIZE
    // - "Invalid message length" if length == 0 or corrupted
    // - "Read timeout" if message not complete within timeout
    // - Socket I/O errors
    ErrorOr<ByteBuffer> read_complete_message(Core::Socket& socket, AK::Duration timeout = AK::Duration::from_seconds(5));

    // Check if a complete message is ready in the buffer (non-blocking)
    bool has_complete_message() const;

    // Reset reader state (clears buffer, useful after errors)
    void reset();

    // Configuration
    static constexpr u32 MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10MB
    static constexpr u32 MIN_MESSAGE_SIZE = 1;                 // 1 byte minimum
    static constexpr size_t HEADER_SIZE = sizeof(u32);         // 4 bytes for length

private:
    enum class ReadState {
        ReadingHeader,    // Reading 4-byte length prefix
        ReadingPayload    // Reading message body
    };

    ErrorOr<void> read_header(Core::Socket& socket);
    ErrorOr<void> read_payload(Core::Socket& socket);

    // Internal state
    ByteBuffer m_buffer {};           // Accumulation buffer for partial reads
    u32 m_expected_length { 0 };      // Expected message length from header
    ReadState m_state { ReadState::ReadingHeader };
    Optional<MonotonicTime> m_read_start_time; // For timeout tracking
};

}
