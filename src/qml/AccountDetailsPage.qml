// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import PlasmaMail

// Settings → Accounts → one account: its details and removing it.
FormCard.FormCardPage {
    id: page

    title: MailController.accountDisplayName.length > 0 ? MailController.accountDisplayName : MailController.accountEmail

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Account Details")
    }

    FormCard.FormCard {
        FormCard.AbstractFormDelegate {
            background: null
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                SenderAvatar {
                    implicitWidth: Kirigami.Units.gridUnit * 2.5
                    name: MailController.accountEmail
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: MailController.accountDisplayName.length > 0 ? MailController.accountDisplayName
                                                                           : MailController.accountEmail
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: MailController.accountEmail
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: KI18n.i18nc("@label", "Provider")
            description: MailController.accountProvider
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: KI18n.i18nc("@label", "Incoming mail server (IMAP, SSL/TLS)")
            description: MailController.accountServer
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: KI18n.i18nc("@label", "Password")
            description: KI18n.i18nc("@info", "Stored in the system wallet (KWallet)")
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: KI18n.i18nc("@action:button", "Remove Account…")
            icon.name: "list-remove-user"
            icon.color: Kirigami.Theme.negativeTextColor
            onClicked: removeDialog.open()
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
                    MailController.removeAccount(); // Main.qml then closes the settings
                }
            },
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: removeDialog.close()
            }
        ]
    }
}
