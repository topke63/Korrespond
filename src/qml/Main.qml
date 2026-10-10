// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQml
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.settings as KirigamiSettings
import org.kde.kirigamiaddons.statefulapp as StatefulApp
import PlasmaMail

StatefulApp.StatefulWindow {
    id: mainWindow

    windowName: "Main"
    // The action collection (T211): ids and enabled state live in C++, see mailapplication.h.
    application: MailApplication {
        id: mailApplication

        controller: MailController
    }

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

    // Opens a message window filled with a message that came back (undo send, failed hand-off): the
    // map is what was passed to MailController.sendMessage(). The same creation path as a reply.
    function reopenMessage(data: var, error: string) {
        const component = Qt.createComponent("PlasmaMail", "ComposeWindow");
        const composer = component.createObject(mainWindow, {
            initialAccount: data.account ?? "",
            initialTo: data.to ?? "",
            initialCc: data.cc ?? "",
            initialBcc: data.bcc ?? "",
            initialSubject: data.subject ?? "",
            initialBody: data.body ?? "",
            initialInReplyTo: data.inReplyTo ?? "",
            initialReferences: data.references ?? [],
            initialAttachments: data.attachments ?? [],
            initialError: error,
            restored: true
        });
        if (composer) {
            (composer as Window).show();
        } else {
            console.warn(component.errorString());
        }
    }

    // Dev only (PLASMAMAIL_DEMO_COMPOSE_SEND in main.cpp): a fresh message window filled as the user
    // would have typed it, with its attachments; objectName lets the hook find it again.
    function openDemoComposer(data: var, name: string) {
        const component = Qt.createComponent("PlasmaMail", "ComposeWindow");
        const composer = component.createObject(mainWindow, {
            objectName: name,
            initialAccount: data.account ?? "",
            initialTo: data.to ?? "",
            initialSubject: data.subject ?? "",
            initialBody: data.body ?? ""
        });
        if (composer) {
            composer.addAttachments(data.attachments ?? []);
            (composer as Window).show();
        } else {
            console.warn(component.errorString());
        }
    }

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

    // Closing the window only hides it; the controller quits when the last window closes (lastWindowClosed).

    // A QAction's shortcut is only active in a window through a Kirigami.Action built from it, so the
    // actions with a default shortcut that no toolbar shows (compose and refresh are in the list
    // page's toolbar) get one here.
    Instantiator {
        model: ["reply", "reply_all", "forward", "archive", "delete", "mark_read", "mark_unread",
                "flag", "toggle_sidebar", "settings", "file_quit", "search", "next_message",
                "previous_message", "shortcuts_help", "open_kcommand_bar"]
        delegate: Kirigami.Action {
            required property string modelData

            fromQAction: mailApplication.action(modelData)
        }
    }

    // The actions that need the window (their keys are the action collection's default shortcuts,
    // Ctrl+Q included: file_quit routes to MailController.requestQuit()). Compose, search, next and
    // previous are handled by the pages. Move, Go to Folder and Keyboard Shortcuts have no UI yet (T214, T215).
    Connections {
        target: mailApplication

        function onToggleSidebarRequested() {
            sidebar.toggle();
        }
        function onSettingsRequested() {
            settings.open();
        }
        function onShortcutsHelpRequested() {
            // The help dialog comes with T215.
            console.info("shortcuts help requested");
        }
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
                moduleId: "sending"
                text: KI18n.i18nc("@action:button", "Sending")
                icon.name: "mail-send"
                page: () => sendingPage
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

    // Undoes the newest pending send; a text field's own Ctrl+Z wins while it has focus.
    Shortcut {
        sequence: "Ctrl+Z"
        enabled: MailController.pendingSends.length > 0
        onActivated: MailController.undoSend(-1)
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
        id: sendingPage

        SendingSettingsPage {}
    }

    Component {
        id: storagePage

        StorageSettingsPage {}
    }

    Component {
        id: mailPage

        MessageListPage {
            application: mailApplication
        }
    }

    Component {
        id: messagePage

        MessagePage {
            application: mailApplication
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
        // The compose window closed when the message was queued; the status bar counts down and
        // this notification offers Undo for as long as the delay lasts.
        function onSendQueued(id: int) {
            const seconds = MailController.pendingSend.secondsLeft ?? 10;
            mainWindow.showPassiveNotification(KI18n.i18nc("@info:status", "Sending message…"), seconds * 1000, KI18n.i18nc("@action:button", "Undo"), () => MailController.undoSend(id));
        }
        function onSendUndone(message: var) {
            mainWindow.reopenMessage(message, "");
        }
        // A queued message failed after its window was gone: bring it back with the error.
        function onSendFailed(id: int, message: string, original: var, queued: bool) {
            if (queued) {
                mainWindow.reopenMessage(original, message);
            }
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
