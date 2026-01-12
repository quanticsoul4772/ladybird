/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <LibIPC/BufferedIPCWriter.h>

namespace IPC {

ErrorOr<void> BufferedIPCWriter::write_message(Core::Socket& socket, ReadonlyBytes payload)
{
    // Validate message size
    if (payload.size() > MAX_MESSAGE_SIZE)
        return Error::from_string_literal("Message too large to send");

    if (payload.size() == 0)
        return Error::from_string_literal("Cannot send empty message");

    // Build framed message: [length][payload]
    u32 length = static_cast<u32>(payload.size());
    u32 length_network = AK::convert_between_host_and_network_endian(length);

    // Write length header (4 bytes, network byte order)
    auto header_bytes = ReadonlyBytes { reinterpret_cast<u8 const*>(&length_network), sizeof(u32) };
    TRY(socket.write_until_depleted(header_bytes));

    // Write payload
    TRY(socket.write_until_depleted(payload));

    return {};
}

}
