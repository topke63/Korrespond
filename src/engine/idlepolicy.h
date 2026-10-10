// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

// Pure timing policy for IMAP IDLE: no timers, time is passed in explicitly.
//
// - IDLE is re-issued every restartInterval() (20 min).
// - After a failed connection, nextReconnectDelay() returns 30 s, 60 s, 120 s,
//   300 s, 300 s, ... Each call advances the sequence.
// - reportConnected(now) records when the connection came up.
// - reportFailure(now) must be called when the connection drops or fails. If the
//   connection had been up for at least stableDuration() (2 min) since the last
//   reportConnected, the sequence is reset so the next delay is 30 s again.
//   Otherwise (never connected, or dropped sooner) the sequence is left as is.
//   The connected state is cleared by reportFailure.
class IdlePolicy
{
public:
    static constexpr std::chrono::minutes restartInterval()
    {
        return std::chrono::minutes(20);
    }
    static constexpr std::chrono::minutes stableDuration()
    {
        return std::chrono::minutes(2);
    }

    // 15 minutes while idling (safety net), else checkMinutes clamped to >= 1.
    static std::chrono::minutes pollInterval(bool idling, int checkMinutes);

    void reportConnected(std::chrono::steady_clock::time_point now);
    void reportFailure(std::chrono::steady_clock::time_point now);
    std::chrono::seconds nextReconnectDelay();

private:
    int m_step = 0;
    bool m_connected = false;
    std::chrono::steady_clock::time_point m_connectedAt;
};
