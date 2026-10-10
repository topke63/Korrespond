// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.ki18n
import org.kde.kirigamiaddons.formcard as FormCard
import Korrespond

// Settings → Message List: how the list groups messages. Applies at once.
FormCard.FormCardPage {
    title: KI18n.i18nc("@title", "Message List")

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Conversations")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: KI18n.i18nc("@option:check", "Group messages into conversations")
            description: KI18n.i18nc("@info", "One row per conversation in the current folder; the newest message stands for it.")
            checked: MailController.conversationView
            onToggled: MailController.conversationView = checked
        }
    }

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Message Content")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: KI18n.i18nc("@option:check", "Always load remote images")
            description: KI18n.i18nc("@info", "Images in HTML mail are fetched from the sender's server when the message is opened, which tells the sender that you read it.")
            checked: MailController.alwaysLoadImages
            onToggled: MailController.alwaysLoadImages = checked
        }
    }
}
