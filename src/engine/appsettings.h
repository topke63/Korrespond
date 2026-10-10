// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

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
    // The folder sidebar is hidden on wide windows (a narrow window's sliding sidebar is not kept).
    bool sidebarCollapsed = false;
    // Accounts (by email) whose folder tree is folded in the sidebar; the others are open.
    QStringList collapsedAccounts;
    // Folder paths hidden from the sidebar, by account email (display only). Absent = shown.
    QMap<QString, QStringList> hiddenFolders;
    // Show the hidden folders anyway (dimmed).
    bool showHiddenFolders = false;
    // Seconds a sent message waits (undoable) before it is handed to SMTP; 0 = off. One of
    // 0, 5, 10, 20, 30: other values snap to the nearest choice on load.
    int undoSendSeconds = 10;
    // The message list shows one row per conversation (T221). Off until it has been tried on real
    // mail (T226).
    bool conversationView = false;

    // The undo-send delay snapped to the nearest allowed choice (negative -> 0 = off).
    static int snapUndoSend(int seconds);

    // Missing or out-of-range values fall back to the defaults / the nearest allowed value.
    static AppSettings load();
    // Saves with checkMinutes and the widths limited to the allowed ranges.
    void save() const;

    bool operator==(const AppSettings &) const = default;
};
