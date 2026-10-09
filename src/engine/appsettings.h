// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// General app options, in plasmamailrc, group [General].
struct AppSettings {
    static constexpr int minCheckMinutes = 1;
    static constexpr int maxCheckMinutes = 60;

    // Closing the window hides it to the tray icon instead of quitting.
    bool keepInTray = false;
    // How often the accounts are checked for new mail in the background.
    int checkMinutes = 5;

    // Missing or out-of-range values fall back to the defaults / the nearest allowed value.
    static AppSettings load();
    // Saves with checkMinutes limited to the allowed range.
    void save() const;

    bool operator==(const AppSettings &) const = default;
};
