/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/ByteBuffer.h>
#include <AK/Error.h>
#include <AK/StringView.h>
#include <LibCore/Socket.h>

namespace IPC {

// BufferedIPCWriter: Writes length-prefixed IPC messages
//
// Companion to BufferedIPCReader, ensures messages are properly framed
// with length headers for reliable transmission.
//
// Protocol Format:
// [Length: 4 bytes][Payload: Length bytes]
//
// Example Usage:
//   BufferedIPCWriter writer;
//   TRY(writer.write_message(socket, message_payload));

class BufferedIPCWriter {
public:
    BufferedIPCWriter() = default;
    ~BufferedIPCWriter() = default;

    // Write a complete message with length prefix
    // Automatically prepends 4-byte length header
    ErrorOr<void> write_message(Core::Socket& socket, ReadonlyBytes payload);

    // Write a string message (convenience wrapper)
    ErrorOr<void> write_message(Core::Socket& socket, StringView payload)
    {
        return write_message(socket, payload.bytes());
    }

    // Configuration (matches BufferedIPCReader)
    static constexpr u32 MAX_MESSAGE_SIZE = 10 * 1024 * 1024; // 10MB
};

}
