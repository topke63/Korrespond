// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailnotifier.h"

#include "mailcontroller.h"

#include <KLocalizedString>
#include <KNotification>

#include <QIcon>
#include <QWindow>

using namespace Qt::StringLiterals;

// "Name <address>" → "Name"; without a name the address.
static QString senderName(const QString &from)
{
    const qsizetype open = from.indexOf(u'<');
    const QString name = (open > 0 ? from.left(open) : QString()).trimmed().remove(u'"');
    if (!name.isEmpty()) {
        return name;
    }
    const qsizetype close = from.indexOf(u'>');
    return open >= 0 && close > open ? from.mid(open + 1, close - open - 1) : from.trimmed();
}

MailNotifier::MailNotifier(MailController *controller, QWindow *window, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_window(window)
{
    connect(controller, &MailController::newMailArrived, this, &MailNotifier::onNewMail);
}

void MailNotifier::onNewMail(const QString &account, int count, const QString &sender, const QString &subject, qint64 uid)
{
    // Someone looking at the window sees the mail arrive; no need to interrupt.
    if (m_window && m_window->isActive()) {
        return;
    }
    auto notification = new KNotification(u"newmail"_s);
    notification->setComponentName(u"plasma-mail"_s);
    if (QIcon::hasThemeIcon(u"plasma-mail"_s)) {
        notification->setIconName(u"plasma-mail"_s);
    } else {
        notification->setPixmap(QIcon(u":/icons/sc-apps-plasma-mail.svg"_s).pixmap(64, 64)); // not installed yet
    }
    if (count == 1) {
        notification->setTitle(senderName(sender));
        notification->setText(subject.isEmpty() ? i18nc("@info notification text for mail without a subject", "(No subject)") : subject);
    } else {
        notification->setTitle(i18ncp("@title notification", "%1 new message", "%1 new messages", count));
        notification->setText(account);
    }
    KNotificationAction *open = notification->addDefaultAction(i18nc("@action:button", "Open"));
    connect(open, &KNotificationAction::activated, this, [this, account, uid] {
        m_controller->openNewMail(account, uid);
        if (m_window) {
            m_window->show();
            m_window->raise();
            m_window->requestActivate();
        }
    });
    notification->sendEvent();
}
