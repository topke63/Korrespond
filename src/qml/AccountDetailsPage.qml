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

    // The account shown (values of the accounts model's row).
    property string email
    property string displayName
    property string provider
    property string server
    property string authMethod

    title: displayName.length > 0 ? displayName : email

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
                    name: page.email
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: page.displayName.length > 0 ? page.displayName : page.email
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: page.email
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }
            }
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: KI18n.i18nc("@label", "Provider")
            description: page.provider
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: KI18n.i18nc("@label", "Incoming mail server (IMAP, SSL/TLS)")
            description: page.server
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormTextDelegate {
            text: page.authMethod === "oauth" ? KI18n.i18nc("@label", "Sign-in") : KI18n.i18nc("@label", "Password")
            description: page.authMethod === "oauth" ? KI18n.i18nc("@info", "Signed in with Google; only a sign-in token is stored in the system wallet (KWallet)")
                                                      : KI18n.i18nc("@info", "Stored in the system wallet (KWallet)")
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
                              page.email)
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Remove Account")
                icon.name: "list-remove-user"
                onTriggered: {
                    removeDialog.close();
                    MailController.removeAccount(page.email);
                    // Main.qml closes the settings when it was the last account.
                    const stack = page.QQC2.ApplicationWindow.window.pageStack;
                    if (stack.layers.depth > 1) {
                        stack.layers.pop();
                    }
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
