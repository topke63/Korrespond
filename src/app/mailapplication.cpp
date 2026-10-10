// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailapplication.h"

#include "mailcontroller.h"
#include "shortcutsproxymodel.h"

#include <KLocalizedString>

#include <QAbstractListModel>
#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <QKeySequence>

using namespace Qt::StringLiterals;

MailApplication::MailApplication(QObject *parent)
    : AbstractKirigamiApplication(parent)
{
    setupActions();
    m_editorModel = new ShortcutsProxyModel(shortcutsModel(), this);
    m_folderTimer.setSingleShot(true);
    m_folderTimer.setInterval(0);
    connect(&m_folderTimer, &QTimer::timeout, this, &MailApplication::rebuildFolderActions);
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
        // Refresh is off while a refresh runs.
        connect(m_controller, &MailController::busyChanged, this, &MailApplication::updateEnabled);
        // The folder entries follow the accounts, their folders and the open message's account.
        connect(m_controller, &MailController::accountChanged, this, &MailApplication::scheduleFolderActions);
        connect(m_controller, &MailController::currentMessageChanged, this, &MailApplication::scheduleFolderActions);
        QAbstractListModel *accounts = m_controller->accounts();
        connect(accounts, &QAbstractItemModel::modelReset, this, &MailApplication::scheduleFolderActions);
        connect(accounts, &QAbstractItemModel::rowsInserted, this, &MailApplication::scheduleFolderActions);
        connect(accounts, &QAbstractItemModel::rowsRemoved, this, &MailApplication::scheduleFolderActions);
        connect(accounts, &QAbstractItemModel::rowsMoved, this, &MailApplication::scheduleFolderActions);
    }
    updateEnabled();
    scheduleFolderActions();
    Q_EMIT controllerChanged();
}

QObject *MailApplication::shortcutsEditorModel()
{
    return m_editorModel;
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
    controllerAction(u"mark_read"_s, i18nc("@action", "Mark as Read"), u"mail-mark-read"_s, [](MailController *c) {
        c->setOpenUnread(false);
    });
    controllerAction(u"mark_unread"_s, i18nc("@action", "Mark as Unread"), u"mail-mark-unread"_s, [](MailController *c) {
        c->setOpenUnread(true);
    });
    controllerAction(u"flag"_s, i18nc("@action", "Flag"), u"mail-mark-important"_s, [](MailController *c) {
        c->setFlagged(c->currentMessageAccount(), c->currentUid(), !c->messageFlagged());
    })->setCheckable(true);
    signalAction(u"search"_s, i18nc("@action", "Search"), u"edit-find"_s, &MailApplication::searchRequested);
    controllerAction(u"refresh"_s, i18nc("@action", "Refresh"), u"view-refresh"_s, [](MailController *c) {
        c->refresh();
    });
    signalAction(u"toggle_sidebar"_s, i18nc("@action", "Show Folders"), u"sidebar-show"_s, &MailApplication::toggleSidebarRequested);
    signalAction(u"settings"_s, i18nc("@action", "Settings"), u"configure"_s, &MailApplication::settingsRequested);
    signalAction(u"shortcuts_help"_s, i18nc("@action", "Keyboard Shortcuts"), u"help-keybord-shortcuts"_s, &MailApplication::shortcutsHelpRequested);
    signalAction(u"next_message"_s, i18nc("@action", "Next Message"), u"go-down"_s, &MailApplication::nextMessageRequested);
    signalAction(u"previous_message"_s, i18nc("@action", "Previous Message"), u"go-up"_s, &MailApplication::previousMessageRequested);

    // Single keys do not fire while a text field has focus: the field accepts the ShortcutOverride.
    // Mark as Read and Mark as Unread share U; only one of them is enabled at a time (a toggle).
    // The first sequence is the primary one (menus, tooltips). No two-key sequences. Up and Down are
    // not here: a window-wide Down would also grab the arrows of the sidebar and the text fields, so
    // the message list handles them itself (MessageListPage.qml).
    const QList<std::pair<const char *, QList<QKeySequence>>> shortcuts = {
        {"compose", {QKeySequence(Qt::CTRL | Qt::Key_N), QKeySequence(Qt::Key_C)}},
        {"reply", {QKeySequence(Qt::Key_R)}},
        {"reply_all", {QKeySequence(Qt::SHIFT | Qt::Key_R), QKeySequence(Qt::Key_A)}},
        {"forward", {QKeySequence(Qt::SHIFT | Qt::Key_F), QKeySequence(Qt::Key_F)}},
        {"archive", {QKeySequence(Qt::Key_E)}},
        {"delete", {QKeySequence(Qt::Key_Delete), QKeySequence(Qt::Key_NumberSign)}},
        {"mark_read", {QKeySequence(Qt::Key_U)}},
        {"mark_unread", {QKeySequence(Qt::Key_U)}},
        {"flag", {QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F), QKeySequence(Qt::Key_S)}},
        {"search", {QKeySequence(Qt::Key_Slash)}},
        {"refresh", {QKeySequence(Qt::Key_F5)}},
        {"toggle_sidebar", {QKeySequence(Qt::CTRL | Qt::Key_B)}},
        {"settings", {QKeySequence(Qt::CTRL | Qt::Key_Comma)}},
        {"shortcuts_help", {QKeySequence(Qt::Key_Question)}},
        {"next_message", {QKeySequence(Qt::Key_J)}},
        {"previous_message", {QKeySequence(Qt::Key_K)}},
        {"open_kcommand_bar", {QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_I), QKeySequence(Qt::CTRL | Qt::Key_K)}},
    };
    for (const auto &[id, sequences] : shortcuts) {
        if (QAction *action = mainCollection()->action(QString::fromLatin1(id))) {
            KirigamiActionCollection::setDefaultShortcuts(action, sequences);
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

QVariantList MailApplication::shortcutGroups() const
{
    struct Group {
        QString title;
        QList<std::pair<const char *, QString>> items; // action id, text ("" = the action's own text)
    };
    const QList<Group> groups = {
        {i18nc("@title:group shortcuts", "Messages"),
         {{"compose", {}},
          {"reply", {}},
          {"reply_all", {}},
          {"forward", {}},
          {"archive", {}},
          {"delete", {}},
          {"mark_unread", i18nc("@action", "Mark as Read / Unread")},
          {"flag", {}}}},
        {i18nc("@title:group shortcuts", "Navigation"),
         {{"next_message", {}}, {"previous_message", {}}, {"search", {}}, {"refresh", {}}, {"toggle_sidebar", i18nc("@action", "Show or Hide Folders")}}},
        {i18nc("@title:group shortcuts", "App"),
         {{"open_kcommand_bar", i18nc("@action", "Command Bar")},
          {"shortcuts_help", {}},
          {"settings", {}},
          {"options_configure_keybinding", i18nc("@action", "Change Keyboard Shortcuts…")},
          {"file_quit", {}}}},
    };
    auto keys = [](const QAction *action) {
        QStringList names;
        const auto sequences = action->shortcuts();
        for (const QKeySequence &sequence : sequences) {
            names.append(sequence.toString(QKeySequence::NativeText));
        }
        return names.join(i18nc("@item separator between shortcuts", ", "));
    };

    QVariantList result;
    for (const Group &group : groups) {
        QVariantList items;
        for (const auto &[id, text] : group.items) {
            const QAction *action = mainCollection()->action(QString::fromLatin1(id));
            if (!action) {
                continue;
            }
            items.append(QVariantMap{{u"text"_s, text.isEmpty() ? QString(action->text()).remove(u'&') : text}, {u"shortcut"_s, keys(action)}});
        }
        if (group.title == i18nc("@title:group shortcuts", "Messages")) {
            // Not an action: a window-wide Ctrl+Z that only exists while a send is waiting.
            items.append(QVariantMap{{u"text"_s, i18nc("@action", "Undo Send")},
                                     {u"shortcut"_s, QKeySequence(Qt::CTRL | Qt::Key_Z).toString(QKeySequence::NativeText)}});
        }
        result.append(QVariantMap{{u"title"_s, group.title}, {u"items"_s, items}});
    }
    return result;
}

void MailApplication::scheduleFolderActions()
{
    m_folderTimer.start();
}

void MailApplication::rebuildFolderActions()
{
    for (QAction *action : std::as_const(m_folderActions)) {
        mainCollection()->takeAction(action);
        action->deleteLater();
    }
    m_folderActions.clear();
    m_moveActions.clear();
    MailController *c = m_controller;
    if (!c || !c->hasAccount()) {
        if (m_editorModel) {
            m_editorModel->setHiddenNames({});
        }
        return;
    }

    int serial = 0;
    auto add = [this, &serial](const QString &prefix, const QString &text, const QString &icon) {
        auto *action = new QAction(this);
        action->setText(text);
        action->setIcon(QIcon::fromTheme(icon));
        // Named by position: a path can hold any characters, and the name is only an identifier.
        mainCollection()->addAction(prefix + QString::number(serial++), action);
        m_folderActions.append(action);
        return action;
    };

    // Go to: the shared views, then every account's folders. Several accounts: say which one.
    const QVariantList shared = c->sharedFolders();
    for (const QVariant &entry : shared) {
        const QVariantMap map = entry.toMap();
        const QString path = map.value(u"path"_s).toString();
        QAction *action = add(u"go_to_folder_"_s, i18nc("@action", "Go to %1", map.value(u"name"_s).toString()), map.value(u"iconName"_s).toString());
        connect(action, &QAction::triggered, this, [this, path] {
            if (m_controller) {
                m_controller->selectFolder(QString(), path);
            }
        });
    }
    QAbstractListModel *accounts = c->accounts();
    const int accountCount = accounts->rowCount();
    const QHash<int, QByteArray> accountRoles = accounts->roleNames();
    const int emailRole = accountRoles.key("email");
    const int folderModelRole = accountRoles.key("folderModel");
    // Watch each account's folder list: the folders arrive after the first sync.
    for (int row = 0; row < accountCount; ++row) {
        const QModelIndex index = accounts->index(row);
        const QString email = index.data(emailRole).toString();
        auto *folders = qvariant_cast<QAbstractListModel *>(index.data(folderModelRole));
        if (!folders) {
            continue;
        }
        const QHash<int, QByteArray> roles = folders->roleNames();
        const int pathRole = roles.key("path");
        const int nameRole = roles.key("name");
        const int iconRole = roles.key("iconName");
        connect(folders, &QAbstractItemModel::modelReset, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
        connect(folders, &QAbstractItemModel::rowsInserted, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
        connect(folders, &QAbstractItemModel::rowsRemoved, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
        connect(folders, &QAbstractItemModel::dataChanged, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
        for (int i = 0; i < folders->rowCount(); ++i) {
            const QModelIndex folder = folders->index(i);
            const QString path = folder.data(pathRole).toString();
            const QString name = folder.data(nameRole).toString();
            const QString label =
                accountCount > 1 ? i18nc("@action %1 folder, %2 account email", "Go to %1 (%2)", name, email) : i18nc("@action", "Go to %1", name);
            QAction *action = add(u"go_to_folder_"_s, label, folder.data(iconRole).toString());
            connect(action, &QAction::triggered, this, [this, email, path] {
                if (m_controller) {
                    m_controller->selectFolder(email, path);
                }
            });
        }
    }

    // Move to: the folders of the open message's account.
    QAbstractListModel *targets = c->messageFolderModel();
    const QHash<int, QByteArray> roles = targets->roleNames();
    const int pathRole = roles.key("path");
    const int nameRole = roles.key("name");
    const int iconRole = roles.key("iconName");
    connect(targets, &QAbstractItemModel::modelReset, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
    connect(targets, &QAbstractItemModel::rowsInserted, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
    connect(targets, &QAbstractItemModel::rowsRemoved, this, &MailApplication::scheduleFolderActions, Qt::UniqueConnection);
    for (int i = 0; i < targets->rowCount(); ++i) {
        const QModelIndex folder = targets->index(i);
        const QString path = folder.data(pathRole).toString();
        QAction *action = add(u"move_to_folder_"_s, i18nc("@action", "Move to %1", folder.data(nameRole).toString()), folder.data(iconRole).toString());
        action->setProperty("path", path);
        connect(action, &QAction::triggered, this, [this, path] {
            if (m_controller) {
                m_controller->moveMessage(path);
            }
        });
        m_moveActions.append(action);
    }
    updateEnabled();
    if (m_editorModel) {
        QSet<QString> names;
        for (const QAction *action : std::as_const(m_folderActions)) {
            names.insert(action->text());
        }
        m_editorModel->setHiddenNames(names);
    }
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
    set("mark_read", message && c->messageUnread());
    set("mark_unread", message && !c->messageUnread());
    set("flag", message);
    set("search", account);
    set("refresh", account && !c->busy());
    set("toggle_sidebar", account);
    set("settings", account);
    set("next_message", account);
    set("previous_message", account);
    // Move to <folder>: the open message's own folder is not a target.
    for (QAction *action : std::as_const(m_moveActions)) {
        action->setEnabled(message && c->canMove() && action->property("path").toString() != c->currentMessageFolder());
    }
    if (QAction *flag = mainCollection()->action(QStringLiteral("flag"))) {
        flag->setChecked(message && c->messageFlagged());
    }
}
