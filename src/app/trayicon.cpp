// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "trayicon.h"

#include "mailcontroller.h"

#include <KLocalizedString>
#include <KStatusNotifierItem>

#include <QAction>
#include <QGuiApplication>
#include <QIcon>
#include <QMenu>

using namespace Qt::StringLiterals;

TrayIcon::TrayIcon(MailController *controller, QWindow *window, QObject *parent)
    : QObject(parent)
    , m_item(new KStatusNotifierItem(u"plasma-mail"_s, this))
    , m_controller(controller)
{
    m_item->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_item->setTitle(i18nc("@title", "Plasma Mail"));
    // The themed icons exist once the app is installed; before that (a run from the build
    // folder) the copies inside the executable are sent as pictures.
    if (QIcon::hasThemeIcon(u"org.topke.plasmamail"_s) && QIcon::hasThemeIcon(u"org.topke.plasmamail.unread"_s)) {
        m_item->setIconByName(u"org.topke.plasmamail"_s);
        m_item->setAttentionIconByName(u"org.topke.plasmamail.unread"_s);
    } else {
        m_item->setIconByPixmap(QIcon(u":/icons/sc-apps-org.topke.plasmamail.svg"_s));
        m_item->setAttentionIconByPixmap(QIcon(u":/icons/sc-apps-org.topke.plasmamail.unread.svg"_s));
    }
    m_item->setAssociatedWindow(window); // a click shows or hides it

    // "Check for Mail" goes above the standard entries (Restore/Minimize, Quit).
    QMenu *menu = m_item->contextMenu();
    auto check = new QAction(QIcon::fromTheme(u"view-refresh"_s), i18nc("@action:inmenu", "Check for Mail"), menu);
    connect(check, &QAction::triggered, m_controller, &MailController::refresh);
    const QList<QAction *> actions = menu->actions();
    menu->insertAction(actions.value(1), check); // after the title entry, if there is one
    menu->insertSeparator(actions.value(1));

    // The standard Quit entry goes through the controller, which sends pending messages first.
    connect(m_item, &KStatusNotifierItem::quitRequested, m_controller, [this] {
        m_item->abortQuit();
        m_controller->requestQuit();
    });

    connect(m_controller, &MailController::unreadCountChanged, this, &TrayIcon::update);
    update();
}

void TrayIcon::update()
{
    const int unread = m_controller->unreadCount();
    m_item->setStatus(unread > 0 ? KStatusNotifierItem::NeedsAttention : KStatusNotifierItem::Active);
    m_item->setToolTip(u"org.topke.plasmamail"_s,
                       i18nc("@title", "Plasma Mail"),
                       unread > 0 ? i18ncp("@info:tooltip", "%1 unread message", "%1 unread messages", unread) : i18nc("@info:tooltip", "No unread mail"));
}
