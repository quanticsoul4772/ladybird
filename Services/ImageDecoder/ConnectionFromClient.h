/*
 * Copyright (c) 2020, Andreas Kling <andreas@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/SourceLocation.h>
#include <ImageDecoder/Forward.h>
#include <ImageDecoder/ImageDecoderClientEndpoint.h>
#include <ImageDecoder/ImageDecoderServerEndpoint.h>
#include <LibCore/AnonymousBuffer.h>
#include <LibGfx/BitmapSequence.h>
#include <LibGfx/ColorSpace.h>
#include <LibGfx/ImageFormats/ImageDecoder.h>
#include <LibIPC/ConnectionFromClient.h>
#include <LibIPC/Limits.h>
#include <LibIPC/RateLimiter.h>
#include <LibThreading/BackgroundAction.h>

namespace ImageDecoder {

class ConnectionFromClient final
    : public IPC::ConnectionFromClient<ImageDecoderClientEndpoint, ImageDecoderServerEndpoint> {
    C_OBJECT(ConnectionFromClient);

public:
    ~ConnectionFromClient() override = default;

    virtual void die() override;

    struct DecodeResult {
        bool is_animated = false;
        u32 loop_count = 0;
        u32 frame_count = 0;
        Gfx::FloatPoint scale { 1, 1 };
        Gfx::BitmapSequence bitmaps;
        Vector<u32> durations;
        Gfx::ColorSpace color_profile;

        // Non-null for streaming animated sessions:
        RefPtr<Gfx::ImageDecoder> decoder;
        Core::AnonymousBuffer encoded_data;
    };

    struct AnimationSession {
        Core::AnonymousBuffer encoded_data;
        RefPtr<Gfx::ImageDecoder> decoder;
        u32 frame_count { 0 };
    };

private:
    using Job = Threading::BackgroundAction<DecodeResult>;
    using FrameDecodeResult = Vector<Gfx::ImageFrameDescriptor>;
    using FrameDecodeJob = Threading::BackgroundAction<FrameDecodeResult>;

    explicit ConnectionFromClient(NonnullOwnPtr<IPC::Transport>);

    virtual void decode_image(Core::AnonymousBuffer, Optional<Gfx::IntSize> ideal_size, Optional<ByteString> mime_type, i64 request_id) override;
    virtual void cancel_decoding(i64 request_id) override;
    virtual void request_animation_frames(i64 session_id, u32 start_frame_index, u32 count) override;
    virtual void stop_animation_decode(i64 session_id) override;
    virtual Messages::ImageDecoderServer::ConnectNewClientsResponse connect_new_clients(size_t count) override;
    virtual Messages::ImageDecoderServer::InitTransportResponse init_transport(int peer_pid) override;

    ErrorOr<IPC::TransportHandle> connect_new_client();

    NonnullRefPtr<Job> make_decode_image_job(i64 request_id, Core::AnonymousBuffer, Optional<Gfx::IntSize> ideal_size, Optional<ByteString> mime_type);

    i64 m_next_session_id { 1 };
    HashMap<i64, NonnullRefPtr<Job>> m_pending_jobs;
    HashMap<i64, NonnullOwnPtr<AnimationSession>> m_animation_sessions;
    HashMap<i64, NonnullRefPtr<FrameDecodeJob>> m_pending_frame_jobs;

    // Security validation helpers
    [[nodiscard]] bool validate_buffer_size(size_t size, SourceLocation location = SourceLocation::current())
    {
        // 100MB maximum for image buffers
        static constexpr size_t MaxImageBufferSize = 100 * 1024 * 1024;
        if (size > MaxImageBufferSize) {
            dbgln("Security: ImageDecoder sent oversized image buffer ({} bytes, max {}) at {}:{}",
                size, MaxImageBufferSize,
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool validate_dimensions(Optional<Gfx::IntSize> const& size, SourceLocation location = SourceLocation::current())
    {
        if (!size.has_value())
            return true;

        // Maximum 32768x32768 to prevent integer overflow
        static constexpr int MaxDimension = 32768;
        if (size->width() > MaxDimension || size->height() > MaxDimension) {
            dbgln("Security: ImageDecoder sent invalid ideal_size ({}x{}, max {}) at {}:{}",
                size->width(), size->height(), MaxDimension,
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }

        // Prevent zero/negative dimensions
        if (size->width() <= 0 || size->height() <= 0) {
            dbgln("Security: ImageDecoder sent invalid ideal_size ({}x{}) at {}:{}",
                size->width(), size->height(),
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }

        return true;
    }

    [[nodiscard]] bool validate_mime_type(Optional<ByteString> const& mime_type, SourceLocation location = SourceLocation::current())
    {
        if (!mime_type.has_value())
            return true;

        // Maximum 256 bytes for MIME type
        if (mime_type->length() > 256) {
            dbgln("Security: ImageDecoder sent oversized MIME type ({} bytes, max 256) at {}:{}",
                mime_type->length(),
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }

        return true;
    }

    [[nodiscard]] bool validate_count(size_t count, size_t max_count, StringView field_name, SourceLocation location = SourceLocation::current())
    {
        if (count > max_count) {
            dbgln("Security: ImageDecoder sent excessive {} ({}, max {}) at {}:{}",
                field_name, count, max_count,
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool check_rate_limit(SourceLocation location = SourceLocation::current())
    {
        if (!m_rate_limiter.try_consume()) {
            dbgln("Security: ImageDecoder exceeded rate limit at {}:{}",
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }
        return true;
    }

    [[nodiscard]] bool check_concurrent_decode_limit(SourceLocation location = SourceLocation::current())
    {
        // Maximum 100 concurrent decode jobs per client
        static constexpr size_t MaxConcurrentDecodes = 100;
        if (m_pending_jobs.size() >= MaxConcurrentDecodes) {
            dbgln("Security: ImageDecoder exceeded concurrent decode limit ({}, max {}) at {}:{}",
                m_pending_jobs.size(), MaxConcurrentDecodes,
                location.filename(), location.line_number());
            track_validation_failure();
            return false;
        }
        return true;
    }

    void track_validation_failure()
    {
        m_validation_failures++;
        if (m_validation_failures >= s_max_validation_failures) {
            dbgln("Security: ImageDecoder exceeded validation failure limit ({}), terminating connection",
                s_max_validation_failures);
            die();
        }
    }

    // Security infrastructure
    IPC::RateLimiter m_rate_limiter { 1000, AK::Duration::from_milliseconds(10) }; // 1000 messages/second
    size_t m_validation_failures { 0 };
    static constexpr size_t s_max_validation_failures = 100;
};

}
