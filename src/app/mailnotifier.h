// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

class MailController;
class QWindow;

// Shows a desktop notification when a background check finds new mail (and the window is not the
// active one). Clicking it shows the window with that message open.
class MailNotifier : public QObject
{
    Q_OBJECT
public:
    MailNotifier(MailController *controller, QWindow *window, QObject *parent = nullptr);

private:
    void onNewMail(const QString &account, int count, const QString &sender, const QString &subject, qint64 uid);

    MailController *m_controller;
    QWindow *m_window;
};
