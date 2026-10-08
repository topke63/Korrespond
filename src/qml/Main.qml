// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.ApplicationWindow {
    id: window

    // Shows account setup until an account exists, the mail view after.
    function showStartPage() {
        pageStack.clear();
        pageStack.push(MailController.hasAccount ? mailPage : setupPage);
    }

    title: KI18n.i18nc("@title:window", "Plasma Mail")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 35

    Component.onCompleted: showStartPage()

    globalDrawer: Kirigami.GlobalDrawer {
        id: drawer

        // Docked beside the pages on wide windows, slides over them on narrow ones.
        function updateOpen() {
            drawerOpen = !modal && MailController.hasAccount;
        }

        enabled: MailController.hasAccount
        modal: !window.wideScreen
        title: MailController.accountEmail
        titleIcon: "mail-folder-inbox"
        onModalChanged: updateOpen()
        onEnabledChanged: updateOpen()
        Component.onCompleted: updateOpen()

        Repeater {
            model: MailController.folderModel

            delegate: QQC2.ItemDelegate {
                required property string path
                required property string name
                required property string iconName
                required property int depth

                Layout.fillWidth: true
                leftPadding: Kirigami.Units.largeSpacing + depth * Kirigami.Units.gridUnit
                text: name
                icon.name: iconName
                highlighted: path === MailController.currentFolder
                onClicked: {
                    MailController.selectFolder(path);
                    if (drawer.modal) {
                        drawer.close();
                    }
                }
            }
        }

        footer: QQC2.ToolBar {
            position: QQC2.ToolBar.Footer
            contentItem: QQC2.ToolButton {
                text: KI18n.i18nc("@action:button", "Remove Account")
                icon.name: "list-remove-user"
                display: QQC2.AbstractButton.TextBesideIcon
                onClicked: removeDialog.open()
            }
        }
    }

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
        id: mailPage

        MessageListPage {}
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
