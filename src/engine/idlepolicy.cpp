// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "idlepolicy.h"

#include <algorithm>
#include <iterator>

using namespace std::chrono;

std::chrono::minutes IdlePolicy::pollInterval(bool idling, int checkMinutes)
{
    return idling ? minutes(15) : minutes(std::max(1, checkMinutes));
}

void IdlePolicy::reportConnected(steady_clock::time_point now)
{
    m_connected = true;
    m_connectedAt = now;
}

void IdlePolicy::reportFailure(steady_clock::time_point now)
{
    if (m_connected && now - m_connectedAt >= stableDuration()) {
        m_step = 0;
    }
    m_connected = false;
}

seconds IdlePolicy::nextReconnectDelay()
{
    static constexpr seconds delays[] = {seconds(30), seconds(60), seconds(120), seconds(300)};
    const int last = int(std::size(delays)) - 1;
    const seconds delay = delays[std::min(m_step, last)];
    if (m_step < last) {
        ++m_step;
    }
    return delay;
}
