// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.ScrollablePage {
    id: page

    // Shown from Settings → Accounts → Add Account: closes itself once the account is added.
    property bool asLayer: false

    readonly property bool isCustom: providerCombo.currentValue === "custom"
    // Gmail signs in with Google in the browser: no password.
    readonly property bool isGoogle: providerCombo.currentValue === "gmail"
    readonly property bool canConnect: emailField.text.trim().length > 0
        && (isGoogle ? MailController.googleConfigured : passwordField.text.length > 0)
        && (!isCustom || hostField.text.trim().length > 0)
        && !MailController.busy

    function connectAccount() {
        if (!canConnect) {
            return;
        }
        if (isGoogle) {
            MailController.setupGoogleAccount(emailField.text.trim(), nameField.text.trim());
            return;
        }
        // Yahoo app passwords never contain spaces, but are easy to paste with some.
        const password = providerCombo.currentValue === "yahoo" ? passwordField.text.replace(/\s/g, "")
                                                                 : passwordField.text;
        MailController.setupAccount(providerCombo.currentValue, emailField.text.trim(),
                                    nameField.text.trim(), password,
                                    isCustom ? hostField.text.trim() : "", portField.value);
    }

    title: page.asLayer ? KI18n.i18nc("@title", "Add Account") : KI18n.i18nc("@title", "Set Up Account")

    // Leaving while a login is still being checked gives it up.
    Component.onDestruction: MailController.cancelSetup()

    Connections {
        target: MailController
        enabled: page.asLayer
        function onAccountChanged() {
            const stack = (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).pageStack;
            if (stack.layers.depth > 1) {
                stack.layers.pop();
            }
        }
    }

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: providerCombo

            Kirigami.FormData.label: KI18n.i18nc("@label:listbox", "Provider:")
            textRole: "text"
            valueRole: "value"
            model: [
                { value: "yahoo", text: KI18n.i18nc("@item:inlistbox", "Yahoo"), enabled: true },
                { value: "custom", text: KI18n.i18nc("@item:inlistbox", "Custom IMAP"), enabled: true },
                { value: "gmail", text: KI18n.i18nc("@item:inlistbox", "Gmail"), enabled: true },
                { value: "outlook", text: KI18n.i18nc("@item:inlistbox", "Outlook (coming in a later version)"), enabled: false }
            ]
            delegate: QQC2.ItemDelegate {
                required property var modelData
                required property int index

                width: providerCombo.width
                text: modelData.text
                enabled: modelData.enabled
                highlighted: providerCombo.highlightedIndex === index
            }
        }

        QQC2.TextField {
            id: emailField

            Kirigami.FormData.label: KI18n.i18nc("@label:textbox", "Email:")
            placeholderText: KI18n.i18nc("@info:placeholder", "you@example.com")
            inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: page.connectAccount()
        }

        QQC2.TextField {
            id: nameField

            Kirigami.FormData.label: KI18n.i18nc("@label:textbox", "Your name:")
            onAccepted: page.connectAccount()
        }

        Kirigami.PasswordField {
            id: passwordField

            visible: !page.isGoogle
            Kirigami.FormData.label: KI18n.i18nc("@label:textbox", "Password:")
            onAccepted: page.connectAccount()
        }

        QQC2.Label {
            visible: page.isGoogle
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            color: MailController.googleConfigured ? Kirigami.Theme.textColor : Kirigami.Theme.negativeTextColor
            text: MailController.googleConfigured
                ? KI18n.i18nc("@info", "Your browser opens so you can sign in with Google. Plasma Mail never sees your Google password.")
                : KI18n.i18nc("@info", "Google sign-in needs an OAuth client. Add ClientId and ClientSecret to the [GoogleOAuth] group of ~/.config/plasmamailrc, then restart Plasma Mail.")
        }

        QQC2.Label {
            visible: providerCombo.currentValue === "yahoo"
            Layout.maximumWidth: Kirigami.Units.gridUnit * 20
            wrapMode: Text.Wrap
            font: Kirigami.Theme.smallFont
            text: KI18n.xi18nc("@info", "Yahoo needs an app password, not your normal password. <link url='%1'>How to create one</link>",
                               "https://help.yahoo.com/kb/SLN15241.html")
            onLinkActivated: link => Qt.openUrlExternally(link)
        }

        QQC2.TextField {
            id: hostField

            Kirigami.FormData.label: KI18n.i18nc("@label:textbox", "IMAP server:")
            visible: page.isCustom
            placeholderText: KI18n.i18nc("@info:placeholder", "imap.example.com")
            inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
            onAccepted: page.connectAccount()
        }

        QQC2.SpinBox {
            id: portField

            Kirigami.FormData.label: KI18n.i18nc("@label:spinbox", "Port (SSL/TLS):")
            visible: page.isCustom
            from: 1
            to: 65535
            value: 993
            textFromValue: (value, locale) => value.toString() // no thousands separator
        }

        RowLayout {
            spacing: Kirigami.Units.largeSpacing

            QQC2.Button {
                text: page.isGoogle ? KI18n.i18nc("@action:button", "Sign in with Google…") : KI18n.i18nc("@action:button", "Connect")
                icon.name: page.isGoogle ? "internet-services" : "network-connect"
                enabled: page.canConnect
                onClicked: page.connectAccount()
            }
            // Gives up a login that is still being checked (e.g. the browser sign-in).
            QQC2.Button {
                visible: MailController.busy
                text: KI18n.i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onClicked: MailController.cancelSetup()
            }
            QQC2.BusyIndicator {
                visible: MailController.busy
                running: visible
            }
        }
    }
}
