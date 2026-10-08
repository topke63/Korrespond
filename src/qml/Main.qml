// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.ApplicationWindow {
    id: window

    title: KI18n.i18nc("@title:window", "Plasma Mail")
    width: Kirigami.Units.gridUnit * 50
    height: Kirigami.Units.gridUnit * 35

    // Replaced by the account setup and message list pages in T20–T22.
    pageStack.initialPage: Kirigami.Page {
        title: KI18n.i18nc("@title", "Plasma Mail")

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            icon.name: "mail-message"
            text: KI18n.i18nc("@info:placeholder", "No account set up yet")
        }
    }

    Connections {
        target: MailController
        function onErrorOccurred(message: string) {
            window.showPassiveNotification(message);
        }
    }
}
