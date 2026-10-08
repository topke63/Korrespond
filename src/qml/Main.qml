// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.ApplicationWindow {
    id: window

    // Shows account setup until an account exists, the mail view after.
    function showStartPage() {
        pageStack.clear();
        if (MailController.hasAccount) {
            // Message list and message side by side; on narrow windows one at a time.
            pageStack.push(mailPage);
            pageStack.push(messagePage);
            pageStack.currentIndex = 0;
        } else {
            pageStack.push(setupPage);
        }
    }

    title: KI18n.i18nc("@title:window", "Plasma Mail")
    width: Kirigami.Units.gridUnit * 75
    height: Kirigami.Units.gridUnit * 45

    Component.onCompleted: showStartPage()

    globalDrawer: FolderSidebar {
        id: sidebar

        // Docked beside the pages on wide windows, slides over them on narrow ones.
        function updateOpen() {
            drawerOpen = !modal && MailController.hasAccount;
        }

        enabled: MailController.hasAccount
        modal: !window.wideScreen
        onModalChanged: updateOpen()
        onEnabledChanged: updateOpen()
        Component.onCompleted: updateOpen()
        onStorageSettingsRequested: {
            if (modal) {
                close();
            }
            window.pageStack.layers.push(storagePage);
        }
        onRemoveAccountRequested: removeDialog.open()
    }
    // Inside the page row, so the sidebar's header lines up with the pages' title bars.
    pageStack.leftSidebar: sidebar

    Kirigami.PromptDialog {
        id: removeDialog

        title: KI18n.i18nc("@title:dialog", "Remove Account?")
        subtitle: KI18n.i18nc("@info", "This removes %1 and its saved password from this computer. No mail is deleted on the server.",
                              MailController.accountEmail)
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Remove Account")
                icon.name: "list-remove-user"
                onTriggered: {
                    removeDialog.close();
                    MailController.removeAccount();
                }
            },
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: removeDialog.close()
            }
        ]
    }

    Component {
        id: setupPage

        AccountSetupPage {}
    }

    Component {
        id: storagePage

        StorageSettingsPage {}
    }

    Component {
        id: mailPage

        MessageListPage {}
    }

    Component {
        id: messagePage

        MessagePage {
            Kirigami.ColumnView.fillWidth: true
        }
    }

    Connections {
        target: MailController
        function onErrorOccurred(message: string) {
            window.showPassiveNotification(message);
        }
        function onAccountChanged() {
            window.showStartPage();
        }
    }
}
