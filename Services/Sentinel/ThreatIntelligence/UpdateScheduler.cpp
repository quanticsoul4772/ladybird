/*
 * Copyright (c) 2025, Ladybird contributors
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "UpdateScheduler.h"

namespace Sentinel::ThreatIntelligence {

ErrorOr<NonnullOwnPtr<UpdateScheduler>> UpdateScheduler::create(Duration update_interval)
{
    return adopt_nonnull_own_or_enomem(new (nothrow) UpdateScheduler(update_interval));
}

UpdateScheduler::UpdateScheduler(Duration update_interval)
    : m_update_interval(update_interval)
{
    m_last_update = UnixDateTime::now();
}

ErrorOr<void> UpdateScheduler::schedule_update(UpdateCallback callback)
{
    m_callback = move(callback);

    // Create timer for periodic updates
    m_timer = Core::Timer::create_repeating(
        static_cast<int>(m_update_interval.to_milliseconds()),
        [this] { on_timer_fire(); }
    );

    m_timer->start();
    m_running = true;

    dbgln("UpdateScheduler: Scheduled updates every {} seconds", m_update_interval.to_seconds());
    return {};
}

ErrorOr<void> UpdateScheduler::trigger_update_now()
{
    if (!m_callback) {
        return Error::from_string_literal("No update callback registered");
    }

    dbgln("UpdateScheduler: Triggering immediate update");

    auto result = m_callback();
    if (result.is_error()) {
        dbgln("UpdateScheduler: Update failed: {}", result.error());
        return result.release_error();
    }

    m_last_update = UnixDateTime::now();
    dbgln("UpdateScheduler: Update completed successfully");

    return {};
}

void UpdateScheduler::stop()
{
    if (m_timer) {
        m_timer->stop();
    }
    m_running = false;
    dbgln("UpdateScheduler: Stopped scheduled updates");
}

Duration UpdateScheduler::time_until_next_update() const
{
    if (!m_running)
        return Duration::zero();

    auto now = UnixDateTime::now();
    auto elapsed = Duration::from_seconds(now.seconds_since_epoch() - m_last_update.seconds_since_epoch());

    if (elapsed >= m_update_interval)
        return Duration::zero();

    return m_update_interval - elapsed;
}

void UpdateScheduler::on_timer_fire()
{
    dbgln("UpdateScheduler: Timer fired, executing update");

    auto result = trigger_update_now();
    if (result.is_error()) {
        dbgln("UpdateScheduler: Scheduled update failed: {}", result.error());
    }
}

}
