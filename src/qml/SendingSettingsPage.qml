// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import PlasmaMail

// Settings → Sending: how long a sent message waits so it can still be undone. Changes apply
// at once, to messages sent afterwards (pending ones keep their deadline).
FormCard.FormCardPage {
    id: page

    // Seconds for each entry of the combo, in the order shown.
    readonly property var choices: [0, 5, 10, 20, 30]

    title: KI18n.i18nc("@title", "Sending")

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Undo Send")
    }

    FormCard.FormCard {
        FormCard.FormComboBoxDelegate {
            text: KI18n.i18nc("@label:listbox", "Undo send")
            description: KI18n.i18nc("@info", "Messages wait this long before they are handed to the server, so you can undo.")
            model: [
                KI18n.i18nc("@item:inlistbox undo send delay", "Off"),
                KI18n.i18ncp("@item:inlistbox undo send delay", "%1 second", "%1 seconds", 5),
                KI18n.i18ncp("@item:inlistbox undo send delay", "%1 second", "%1 seconds", 10),
                KI18n.i18ncp("@item:inlistbox undo send delay", "%1 second", "%1 seconds", 20),
                KI18n.i18ncp("@item:inlistbox undo send delay", "%1 second", "%1 seconds", 30)
            ]
            currentIndex: Math.max(0, page.choices.indexOf(MailController.undoSendSeconds))
            onActivated: index => MailController.undoSendSeconds = page.choices[index]
        }
    }
}
