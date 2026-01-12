/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <LibCore/System.h>
#include <LibIPC/BufferedIPCReader.h>

namespace IPC {

ErrorOr<ByteBuffer> BufferedIPCReader::read_complete_message(Core::Socket& socket, AK::Duration timeout)
{
    // Record start time for timeout tracking
    if (!m_read_start_time.has_value())
        m_read_start_time = MonotonicTime::now();

    // Read until we have a complete message
    while (true) {
        // Check timeout
        auto elapsed = MonotonicTime::now() - m_read_start_time.value();
        if (elapsed > timeout) {
            reset();
            return Error::from_string_literal("Read timeout - incomplete message");
        }

        switch (m_state) {
        case ReadState::ReadingHeader:
            TRY(read_header(socket));
            break;

        case ReadState::ReadingPayload:
            TRY(read_payload(socket));

            // Check if we have complete message
            if (m_buffer.size() >= m_expected_length) {
                // Extract complete message
                auto message = TRY(ByteBuffer::copy(m_buffer.span().trim(m_expected_length)));

                // Reset state for next message
                reset();

                return message;
            }
            break;
        }
    }
}

ErrorOr<void> BufferedIPCReader::read_header(Core::Socket& socket)
{
    // Read header bytes until we have 4 bytes
    while (m_buffer.size() < HEADER_SIZE) {
        auto temp_buffer = TRY(ByteBuffer::create_uninitialized(HEADER_SIZE - m_buffer.size()));
        auto bytes_read = TRY(socket.read_some(temp_buffer));

        if (bytes_read.is_empty()) {
            // Connection closed while reading header
            return Error::from_string_literal("Connection closed while reading message header");
        }

        TRY(m_buffer.try_append(bytes_read));
    }

    // We have complete header - parse length
    VERIFY(m_buffer.size() >= HEADER_SIZE);

    // Read length as u32 in network byte order (big endian)
    u32 length_network;
    memcpy(&length_network, m_buffer.data(), HEADER_SIZE);
    m_expected_length = AK::convert_between_host_and_network_endian(length_network);

    // Validate length
    if (m_expected_length == 0)
        return Error::from_string_literal("Invalid message length: zero");

    if (m_expected_length < MIN_MESSAGE_SIZE)
        return Error::from_string_literal("Invalid message length: too small");

    if (m_expected_length > MAX_MESSAGE_SIZE)
        return Error::from_string_literal("Message too large");

    // Clear header from buffer, prepare for payload
    m_buffer.clear();
    m_state = ReadState::ReadingPayload;

    return {};
}

ErrorOr<void> BufferedIPCReader::read_payload(Core::Socket& socket)
{
    // Read payload bytes until we have expected_length bytes
    size_t remaining = m_expected_length - m_buffer.size();

    if (remaining == 0)
        return {}; // Already have complete payload

    // Read in chunks to avoid large allocations
    constexpr size_t CHUNK_SIZE = 4096;
    size_t to_read = min(remaining, CHUNK_SIZE);

    auto temp_buffer = TRY(ByteBuffer::create_uninitialized(to_read));
    auto bytes_read = TRY(socket.read_some(temp_buffer));

    if (bytes_read.is_empty()) {
        // Connection closed while reading payload
        return Error::from_string_literal("Connection closed while reading message payload");
    }

    TRY(m_buffer.try_append(bytes_read));

    return {};
}

bool BufferedIPCReader::has_complete_message() const
{
    if (m_state == ReadState::ReadingHeader)
        return false;

    return m_buffer.size() >= m_expected_length;
}

void BufferedIPCReader::reset()
{
    m_buffer.clear();
    m_expected_length = 0;
    m_state = ReadState::ReadingHeader;
    m_read_start_time.clear();
}

}
