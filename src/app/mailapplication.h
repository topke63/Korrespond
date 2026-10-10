// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <AbstractKirigamiApplication>
#include <QPointer>
#include <QtQml/qqmlregistration.h>

Q_MOC_INCLUDE("mailcontroller.h")

class MailController;
class QAction;

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

public:
    explicit MailApplication(QObject *parent = nullptr);

    [[nodiscard]] MailController *controller() const;
    void setController(MailController *controller);

    // The ids of every action in the collection (dev check, tests).
    [[nodiscard]] QStringList actionNames() const;

Q_SIGNALS:
    void controllerChanged();
    // kind: "new", "reply", "replyAll" or "forward" (the argument of MessagePage.openCompose).
    void composeRequested(const QString &kind);
    void moveRequested();
    void searchRequested();
    void toggleSidebarRequested();
    void settingsRequested();
    void shortcutsHelpRequested();
    void nextMessageRequested();
    void previousMessageRequested();
    void goToFolderRequested();

protected:
    void setupActions() override;

private:
    void updateEnabled();
    QAction *addMailAction(const QString &id, const QString &text, const QString &icon);

    QPointer<MailController> m_controller;
};
