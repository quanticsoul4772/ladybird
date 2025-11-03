/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/Time.h>
#include <LibCore/Timer.h>

namespace Sentinel::ThreatIntelligence {

using AK::Duration;

// Background update scheduler for threat intelligence feeds
// Schedules periodic updates (daily by default) and executes update callbacks
class UpdateScheduler {
public:
    using UpdateCallback = Function<ErrorOr<void>()>;

    static ErrorOr<NonnullOwnPtr<UpdateScheduler>> create(Duration update_interval = Duration::from_seconds(86400));

    // Schedule a daily update callback
    ErrorOr<void> schedule_update(UpdateCallback callback);

    // Manually trigger an immediate update
    ErrorOr<void> trigger_update_now();

    // Stop scheduled updates
    void stop();

    // Check if scheduler is running
    bool is_running() const { return m_running; }

    // Get time until next update
    Duration time_until_next_update() const;

private:
    UpdateScheduler(Duration update_interval);

    void on_timer_fire();

    Duration m_update_interval;
    UpdateCallback m_callback;
    RefPtr<Core::Timer> m_timer;
    UnixDateTime m_last_update;
    bool m_running { false };
};

}
