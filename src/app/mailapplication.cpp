// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailapplication.h"

#include "mailcontroller.h"

#include <KLocalizedString>

#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>

using namespace Qt::StringLiterals;

MailApplication::MailApplication(QObject *parent)
    : AbstractKirigamiApplication(parent)
{
    setupActions();
}

MailController *MailApplication::controller() const
{
    return m_controller;
}

void MailApplication::setController(MailController *controller)
{
    if (m_controller == controller) {
        return;
    }
    if (m_controller) {
        m_controller->disconnect(this);
    }
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &MailController::accountChanged, this, &MailApplication::updateEnabled);
        connect(m_controller, &MailController::currentMessageChanged, this, &MailApplication::updateEnabled);
        // canArchive, canDelete and canMove depend on the folder too.
        connect(m_controller, &MailController::currentFolderChanged, this, &MailApplication::updateEnabled);
        // Check for Mail is off while a refresh runs.
        connect(m_controller, &MailController::busyChanged, this, &MailApplication::updateEnabled);
    }
    updateEnabled();
    Q_EMIT controllerChanged();
}

QStringList MailApplication::actionNames() const
{
    QStringList names;
    const auto actions = mainCollection()->actions();
    for (const QAction *action : actions) {
        names.append(action->objectName());
    }
    return names;
}

QAction *MailApplication::addMailAction(const QString &id, const QString &text, const QString &icon)
{
    auto *action = new QAction(this);
    action->setText(text);
    action->setIcon(QIcon::fromTheme(icon));
    mainCollection()->addAction(id, action);
    return action;
}

void MailApplication::setupActions()
{
    AbstractKirigamiApplication::setupActions();

    // The default shortcuts live here, so the toolbar, menus and keys share one definition (the
    // rest of the set comes with T213). QML binds these actions with Kirigami.Action { fromQAction }.
    auto signalAction = [this](const QString &id, const QString &text, const QString &icon, auto signal, auto... args) {
        QAction *action = addMailAction(id, text, icon);
        connect(action, &QAction::triggered, this, [this, signal, args...] {
            Q_EMIT(this->*signal)(args...);
        });
        return action;
    };
    auto controllerAction = [this](const QString &id, const QString &text, const QString &icon, auto call) {
        QAction *action = addMailAction(id, text, icon);
        connect(action, &QAction::triggered, this, [this, call] {
            if (m_controller) {
                call(m_controller.data());
                // A checkable action has flipped by itself; show what the controller says.
                updateEnabled();
            }
        });
        return action;
    };

    signalAction(u"compose"_s, i18nc("@action", "New Message"), u"mail-message-new"_s, &MailApplication::composeRequested, QStringLiteral("new"));
    signalAction(u"reply"_s, i18nc("@action", "Reply"), u"mail-reply-sender"_s, &MailApplication::composeRequested, QStringLiteral("reply"));
    signalAction(u"reply_all"_s, i18nc("@action", "Reply to All"), u"mail-reply-all"_s, &MailApplication::composeRequested, QStringLiteral("replyAll"));
    signalAction(u"forward"_s, i18nc("@action", "Forward"), u"mail-forward"_s, &MailApplication::composeRequested, QStringLiteral("forward"));
    controllerAction(u"archive"_s, i18nc("@action", "Archive"), u"mail-archive"_s, [](MailController *c) {
        c->archiveMessage();
    });
    controllerAction(u"delete"_s, i18nc("@action", "Delete"), u"edit-delete"_s, [](MailController *c) {
        c->deleteMessage();
    });
    signalAction(u"move"_s, i18nc("@action", "Move to Folder…"), u"folder-move"_s, &MailApplication::moveRequested);
    controllerAction(u"mark_read"_s, i18nc("@action", "Mark as Read"), u"mail-mark-read"_s, [](MailController *c) {
        c->setUnread(c->currentMessageAccount(), c->currentUid(), false);
    });
    controllerAction(u"mark_unread"_s, i18nc("@action", "Mark as Unread"), u"mail-mark-unread"_s, [](MailController *c) {
        c->setUnread(c->currentMessageAccount(), c->currentUid(), true);
    });
    controllerAction(u"flag"_s, i18nc("@action", "Flag"), u"mail-mark-important"_s, [](MailController *c) {
        c->setFlagged(c->currentMessageAccount(), c->currentUid(), !c->messageFlagged());
    })->setCheckable(true);
    signalAction(u"search"_s, i18nc("@action", "Search"), u"edit-find"_s, &MailApplication::searchRequested);
    controllerAction(u"refresh"_s, i18nc("@action", "Check for Mail"), u"view-refresh"_s, [](MailController *c) {
        c->refresh();
    });
    signalAction(u"toggle_sidebar"_s, i18nc("@action", "Show Folders"), u"sidebar-show"_s, &MailApplication::toggleSidebarRequested);
    signalAction(u"settings"_s, i18nc("@action", "Settings"), u"configure"_s, &MailApplication::settingsRequested);
    signalAction(u"shortcuts_help"_s, i18nc("@action", "Keyboard Shortcuts"), u"help-keybord-shortcuts"_s, &MailApplication::shortcutsHelpRequested);
    signalAction(u"next_message"_s, i18nc("@action", "Next Message"), u"go-down"_s, &MailApplication::nextMessageRequested);
    signalAction(u"previous_message"_s, i18nc("@action", "Previous Message"), u"go-up"_s, &MailApplication::previousMessageRequested);
    signalAction(u"go_to_folder"_s, i18nc("@action", "Go to Folder…"), u"folder"_s, &MailApplication::goToFolderRequested);

    // Single keys do not fire while a text field has focus: the field accepts the ShortcutOverride.
    // Mark as Read and Mark as Unread share U; only one of them is enabled at a time (a toggle).
    const QList<std::pair<const char *, QKeySequence>> shortcuts = {
        {"compose", QKeySequence(Qt::CTRL | Qt::Key_N)},
        {"reply", QKeySequence(Qt::Key_R)},
        {"reply_all", QKeySequence(Qt::SHIFT | Qt::Key_R)},
        {"forward", QKeySequence(Qt::SHIFT | Qt::Key_F)},
        {"archive", QKeySequence(Qt::Key_E)},
        {"delete", QKeySequence(Qt::Key_Delete)},
        {"mark_read", QKeySequence(Qt::Key_U)},
        {"mark_unread", QKeySequence(Qt::Key_U)},
        {"flag", QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F)},
        {"refresh", QKeySequence(Qt::Key_F5)},
        {"toggle_sidebar", QKeySequence(Qt::CTRL | Qt::Key_B)},
        {"settings", QKeySequence(Qt::CTRL | Qt::Key_Comma)},
    };
    for (const auto &[id, sequence] : shortcuts) {
        if (QAction *action = mainCollection()->action(QString::fromLatin1(id))) {
            KirigamiActionCollection::setDefaultShortcut(action, sequence);
        }
    }

    // The base's file_quit (KStandardActions::quit) calls QCoreApplication::quit directly, which skips
    // the wait for pending sends. Quitting goes through MailController::requestQuit() (T227).
    if (QAction *quit = mainCollection()->action(u"file_quit"_s)) {
        disconnect(quit, &QAction::triggered, nullptr, nullptr);
        connect(quit, &QAction::triggered, this, [this] {
            if (m_controller) {
                m_controller->requestQuit();
            } else {
                QCoreApplication::quit();
            }
        });
    }

    readSettings();
    updateEnabled();
}

void MailApplication::updateEnabled()
{
    const MailController *c = m_controller;
    const bool account = c && c->hasAccount();
    const bool message = account && c->currentUid() > 0;
    auto set = [this](const char *id, bool enabled) {
        if (QAction *action = mainCollection()->action(QString::fromLatin1(id))) {
            action->setEnabled(enabled);
        }
    };
    // New Message needs no open message (T62 opens a blank composer with no selection).
    set("compose", account);
    set("reply", message && c->canCompose());
    set("reply_all", message && c->canCompose());
    set("forward", message && c->canCompose());
    set("archive", message && c->canArchive());
    set("delete", message && c->canDelete());
    set("move", message && c->canMove());
    set("mark_read", message && c->messageUnread());
    set("mark_unread", message && !c->messageUnread());
    set("flag", message);
    set("search", account);
    set("refresh", account && !c->busy());
    set("toggle_sidebar", account);
    set("settings", account);
    set("next_message", account);
    set("previous_message", account);
    set("go_to_folder", account);
    if (QAction *flag = mainCollection()->action(QStringLiteral("flag"))) {
        flag->setChecked(message && c->messageFlagged());
    }
}
