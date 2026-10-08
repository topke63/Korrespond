// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
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

    Component {
        id: setupPage

        AccountSetupPage {}
    }

    // Replaced by the folder and message list pages in T21–T22.
    Component {
        id: mailPage

        Kirigami.Page {
            title: MailController.accountEmail

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                icon.name: "mail-message"
                text: KI18n.i18nc("@info:placeholder", "No message selected")
            }
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
