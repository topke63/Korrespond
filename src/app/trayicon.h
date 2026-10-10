// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

class KStatusNotifierItem;
class MailController;
class QWindow;

// The system tray icon: shows the logo, with an orange dot and a tooltip with the number of
// unread messages when there are some. A click shows or hides the main window; the menu has
// Check for Mail and Quit.
class TrayIcon : public QObject
{
    Q_OBJECT
public:
    TrayIcon(MailController *controller, QWindow *window, QObject *parent = nullptr);

private:
    void update();

    KStatusNotifierItem *m_item;
    MailController *m_controller;
};
