// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.settings as KirigamiSettings
import PlasmaMail

Kirigami.ApplicationWindow {
    id: mainWindow

    // Shows account setup until an account exists, the mail view after.
    function showStartPage() {
        hadAccount = MailController.hasAccount;
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

    // Whether the start page was last built for "has an account"; see onAccountChanged.
    property bool hadAccount: false

    // Dev only: the PLASMAMAIL_SCREENSHOT_SETTINGS hook in main.cpp opens a settings module.
    function openSettingsModule(moduleId: string) {
        settings.open(moduleId);
    }

    // The connection and activity bar; only with an account (not on the first-run setup page).
    footer: MailController.hasAccount ? statusBar : null

    title: KI18n.i18nc("@title:window", "Plasma Mail")
    width: Kirigami.Units.gridUnit * 75
    height: Kirigami.Units.gridUnit * 45

    Component.onCompleted: showStartPage()

    // Without "keep running in the tray", closing the window quits: through the controller, which
    // sends pending messages first. With it, the window just hides (unchanged).
    onClosing: close => {
        if (!MailController.keepInTray) {
            close.accepted = false;
            MailController.requestQuit();
        }
    }

    Shortcut {
        sequence: StandardKey.Quit
        onActivated: MailController.requestQuit()
    }

    globalDrawer: FolderSidebar {
        id: sidebar

        // Docked beside the pages on wide windows, slides over them on narrow ones.
        function updateOpen() {
            drawerOpen = !modal && MailController.hasAccount && !MailController.sidebarCollapsed;
        }
        // Wide: hides or shows the docked column (remembered). Narrow: opens or closes the drawer.
        function toggle() {
            if (modal) {
                drawerOpen = !drawerOpen;
            } else {
                MailController.sidebarCollapsed = !MailController.sidebarCollapsed;
            }
        }

        enabled: MailController.hasAccount
        modal: !mainWindow.wideScreen
        onModalChanged: updateOpen()

        Connections {
            target: MailController

            function onSidebarCollapsedChanged() {
                sidebar.updateOpen();
            }
        }
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
    // The columns draw their own dividers (MimeTheme.columnDividerAlpha).
    pageStack.separatorVisible: false

    // In a wide window the page row's default (fixed) mode gives every column the same width;
    // the dynamic mode uses each page's own width hints, which the resizable list column needs.
    Binding {
        target: mainWindow.pageStack.columnView
        property: "columnResizeMode"
        value: mainWindow.pageStack.wideMode ? Kirigami.ColumnView.DynamicColumns : Kirigami.ColumnView.SingleColumn
    }

    KirigamiSettings.ConfigurationView {
        id: settings

        window: mainWindow
        modules: [
            KirigamiSettings.ConfigurationModule {
                moduleId: "accounts"
                text: KI18n.i18nc("@action:button", "Accounts")
                icon.name: "mail-folder-inbox"
                page: () => accountsPage
            },
            KirigamiSettings.ConfigurationModule {
                moduleId: "notifications"
                text: KI18n.i18nc("@action:button", "Notifications")
                icon.name: "preferences-desktop-notification"
                page: () => notificationsPage
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

    // Kirigami's ScrollablePage (also under FormCardPage and the About page) throws "Cannot
    // read property 'flickable' of null" when the settings window is destroyed with its pages
    // still in it. Removing the pages first, while the window still exists, avoids that.
    Connections {
        target: settings.configViewItem
        ignoreUnknownSignals: true // the mobile settings page has no closing signal

        function onClosing() {
            const pageStack = (settings.configViewItem as Kirigami.ApplicationWindow)?.pageStack;
            if (pageStack) {
                pageStack.layers.clear();
                pageStack.clear();
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+,"
        enabled: MailController.hasAccount
        onActivated: settings.open()
    }

    Shortcut {
        sequence: "Ctrl+B"
        enabled: MailController.hasAccount
        onActivated: sidebar.toggle()
    }

    Component {
        id: setupPage

        AccountSetupPage {}
    }

    StatusBar {
        id: statusBar

        visible: MailController.hasAccount
    }

    Component {
        id: accountsPage

        AccountSettingsPage {}
    }

    Component {
        id: notificationsPage

        NotificationSettingsPage {}
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
            // The message column always keeps room for its toolbar; the panel widths are capped to leave it.
            Kirigami.ColumnView.minimumWidth: Kirigami.Units.gridUnit * 30
        }
    }

    Connections {
        target: MailController
        function onErrorOccurred(message: string) {
            mainWindow.showPassiveNotification(message);
        }
        // A notification was clicked: show the message column (the window itself is raised in C++).
        function onShowMessageRequested() {
            mainWindow.pageStack.currentIndex = mainWindow.pageStack.depth - 1;
        }
        function onAccountChanged() {
            // Adding a second account keeps the open mail view; only the first or the last one
            // switches between setup and mail.
            if (mainWindow.hadAccount !== MailController.hasAccount) {
                mainWindow.showStartPage();
            }
            // After removing the account there is nothing left to configure.
            if (!MailController.hasAccount && settings.configViewItem) {
                const configWindow = settings.configViewItem as Window;
                if (configWindow) {
                    configWindow.close(); // desktop: the settings window
                } else {
                    mainWindow.pageStack.layers.clear(); // mobile: the settings layer
                }
            }
        }
    }
}
