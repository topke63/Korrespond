// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <AbstractKirigamiApplication>
#include <QPointer>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

Q_MOC_INCLUDE("mailcontroller.h")

class MailController;
class QAction;
class QAbstractItemModel;
class ShortcutsProxyModel;

// The app's actions, each with a stable id, text and icon, in one action collection (Kirigami
// Addons StatefulApp). The command bar, the shortcuts editor and saved shortcuts come with it.
// Actions that MailController does directly are forwarded to it; the others emit a *Requested
// signal that the QML window handles. QML gets an action with application.action("archive").
class MailApplication : public AbstractKirigamiApplication
{
    Q_OBJECT
    QML_ELEMENT

    // Set from QML (the MailController singleton); the actions' enabled state follows it.
    Q_PROPERTY(MailController *controller READ controller WRITE setController NOTIFY controllerChanged)
    // The model for the shortcuts editor: shortcutsModel without the folder entries.
    Q_PROPERTY(QObject *shortcutsEditorModel READ shortcutsEditorModel CONSTANT)

public:
    explicit MailApplication(QObject *parent = nullptr);

    [[nodiscard]] MailController *controller() const;
    void setController(MailController *controller);

    [[nodiscard]] QObject *shortcutsEditorModel();

    // The ids of every action in the collection (dev check, tests).
    [[nodiscard]] QStringList actionNames() const;

    // The keyboard help: [{title, items: [{text, shortcut}]}], grouped Messages, Navigation, App.
    // The shortcut text is the action's current keys (a changed shortcut shows), or empty.
    Q_INVOKABLE [[nodiscard]] QVariantList shortcutGroups() const;

Q_SIGNALS:
    void controllerChanged();
    // kind: "new", "reply", "replyAll" or "forward" (the argument of MessagePage.openCompose).
    void composeRequested(const QString &kind);
    void searchRequested();
    void toggleSidebarRequested();
    void settingsRequested();
    void shortcutsHelpRequested();
    void nextMessageRequested();
    void previousMessageRequested();

protected:
    void setupActions() override;

private:
    void updateEnabled();
    // Go to / Move to entries for the folders (command bar, T214): rebuilt, after a short delay, when
    // the accounts, their folders or the open message change.
    void scheduleFolderActions();
    void rebuildFolderActions();
    QAction *addMailAction(const QString &id, const QString &text, const QString &icon);

    QPointer<MailController> m_controller;
    QTimer m_folderTimer;
    QPointer<ShortcutsProxyModel> m_editorModel;
    QList<QAction *> m_folderActions; // owned by the collection's parent (this); removed on rebuild
    QList<QAction *> m_moveActions; // the subset that moves the message; each has a "path" property
};
