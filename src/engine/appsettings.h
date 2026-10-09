// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// General app options, in plasmamailrc, group [General].
struct AppSettings {
    static constexpr int minCheckMinutes = 1;
    static constexpr int maxCheckMinutes = 60;
    // Panel widths in logical pixels: the folder sidebar and the message list column.
    static constexpr int defaultSidebarWidth = 252;
    static constexpr int minSidebarWidth = 160;
    static constexpr int maxSidebarWidth = 420;
    static constexpr int defaultListWidth = 360;
    static constexpr int minListWidth = 280;
    static constexpr int maxListWidth = 800;

    // Closing the window hides it to the tray icon instead of quitting.
    bool keepInTray = false;
    // How often the accounts are checked for new mail in the background.
    int checkMinutes = 5;
    // Use IMAP IDLE (push) where the server supports it; off = poll everyone every checkMinutes.
    bool pushEnabled = true;
    // The widths the user dragged the panels to.
    int sidebarWidth = defaultSidebarWidth;
    int listWidth = defaultListWidth;

    // Missing or out-of-range values fall back to the defaults / the nearest allowed value.
    static AppSettings load();
    // Saves with checkMinutes and the widths limited to the allowed ranges.
    void save() const;

    bool operator==(const AppSettings &) const = default;
};
