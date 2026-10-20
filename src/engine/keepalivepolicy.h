// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>

// When the main IMAP connection of an account is probed to keep it alive. Pure policy: no timers,
// time is passed in. Google and Yahoo close a connection that has been idle for a few minutes (and
// some networks drop it without telling), so a quiet connection is probed before that happens, and a
// dead one is noticed, and replaced, before the user clicks anything.
namespace KeepAlivePolicy
{
// How long a connection may sit unused before it is probed.
constexpr std::chrono::minutes probeInterval()
{
    return std::chrono::minutes(4);
}

// `connected`: the account is logged in with an open session. `busy`: something is going on
// (a request, a check, a job in flight, an account being set up, a sign-in that is needed), so
// the connection is in use or must be left alone. `idleFor`: time since the last traffic.
constexpr bool shouldProbe(bool connected, bool busy, std::chrono::steady_clock::duration idleFor)
{
    return connected && !busy && idleFor >= probeInterval();
}
}
