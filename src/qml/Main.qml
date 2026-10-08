// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.settings as KirigamiSettings
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
        onSettingsRequested: {
            if (modal) {
                close();
            }
            settings.open();
        }
        onAboutRequested: settings.open("about")
    }
    // Inside the page row, so the sidebar's header lines up with the pages' title bars.
    pageStack.leftSidebar: sidebar

    KirigamiSettings.ConfigurationView {
        id: settings

        window: window
        modules: [
            KirigamiSettings.ConfigurationModule {
                moduleId: "accounts"
                text: KI18n.i18nc("@action:button", "Accounts")
                icon.name: "mail-folder-inbox"
                page: () => accountsPage
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "storage"
                text: KI18n.i18nc("@action:button", "Local Storage")
                icon.name: "drive-harddisk"
                page: () => storagePage
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "about"
                text: KI18n.i18nc("@action:button", "About Plasma Mail")
                icon.name: "help-about"
                page: () => Qt.createComponent("org.kde.kirigamiaddons.formcard", "AboutPage")
                category: KI18n.i18nc("@title:group", "About")
            }
        ]
    }

    Shortcut {
        sequence: "Ctrl+,"
        enabled: MailController.hasAccount
        onActivated: settings.open()
    }

    Component {
        id: setupPage

        AccountSetupPage {}
    }

    Component {
        id: accountsPage

        AccountSettingsPage {}
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
            // After removing the account there is nothing left to configure.
            if (!MailController.hasAccount && settings.configViewItem) {
                if (typeof settings.configViewItem.close === "function") {
                    settings.configViewItem.close(); // desktop: the settings window
                } else {
                    window.pageStack.layers.clear(); // mobile: the settings layer
                }
            }
        }
    }
}
