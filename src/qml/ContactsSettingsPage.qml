// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import Korrespond

// Settings → Contacts: the switch for suggestions, sender names and the Google sync, the state
// of each Google account, "Sync now" and "Clear contacts cache". Changes apply at once.
FormCard.FormCardPage {
    id: page

    property var accounts: MailController.contactAccounts()
    property var counts: MailController.contactCounts()

    function refresh() {
        accounts = MailController.contactAccounts();
        counts = MailController.contactCounts();
    }

    title: KI18n.i18nc("@title", "Contacts")

    Connections {
        target: MailController

        function onContactsChanged() {
            page.refresh();
        }
        function onAccountChanged() {
            page.refresh();
        }
    }

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Suggestions and Names")
    }

    FormCard.FormCard {
        FormCard.FormSwitchDelegate {
            text: KI18n.i18nc("@option:check", "Use contacts")
            description: KI18n.i18nc("@info", "Suggest addresses while writing and show names for senders that have none. Google contacts are only read, never changed. Switching off stops syncing and hides suggestions and names; the saved contacts stay until cleared.")
            checked: MailController.contactsEnabled
            onToggled: MailController.setContactsEnabled(checked)
        }
    }

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Google Accounts")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            visible: page.accounts.length === 0
            text: KI18n.i18nc("@info", "No Google account")
            description: KI18n.i18nc("@info", "Contacts come from Google accounts. For other accounts, suggestions use addresses seen in your mail.")
        }

        Repeater {
            model: page.accounts

            delegate: ColumnLayout {
                required property var modelData
                required property int index

                Layout.fillWidth: true
                spacing: 0

                FormCard.FormDelegateSeparator {
                    visible: index > 0
                }

                FormCard.FormTextDelegate {
                    text: modelData.email
                    description: modelData.text
                }

                FormCard.FormDelegateSeparator {
                    visible: modelData.state === "needed"
                }

                FormCard.FormButtonDelegate {
                    visible: modelData.state === "needed"
                    text: KI18n.i18nc("@action:button", "Allow Access to Contacts…")
                    description: KI18n.i18nc("@info", "Opens Google's sign-in in your browser. Mail stays connected. Korrespond only reads contacts.")
                    icon.name: "contact-new"
                    onClicked: MailController.allowContacts(modelData.email)
                }
            }
        }
    }

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Saved Contacts")
    }

    FormCard.FormCard {
        FormCard.FormTextDelegate {
            text: KI18n.i18nc("@label", "On this computer")
            description: KI18n.i18nc("@info %1 %2 %3 are numbers",
                                     "Contacts: %1 · Other contacts: %2 · Addresses seen in mail: %3",
                                     page.counts.google, page.counts.other, page.counts.history)
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: KI18n.i18nc("@action:button", "Sync Now")
            description: KI18n.i18nc("@info", "Contacts are also synced every 6 hours.")
            icon.name: "view-refresh"
            enabled: MailController.contactsEnabled
            onClicked: MailController.syncContactsNow()
        }

        FormCard.FormDelegateSeparator {}

        FormCard.FormButtonDelegate {
            text: KI18n.i18nc("@action:button", "Clear Contacts Cache…")
            description: KI18n.i18nc("@info", "Removes the saved contacts from this computer. Nothing changes at Google.")
            icon.name: "edit-clear-history"
            icon.color: Kirigami.Theme.negativeTextColor
            onClicked: clearDialog.open()
        }
    }

    Kirigami.PromptDialog {
        id: clearDialog

        title: KI18n.i18nc("@title:dialog", "Clear Contacts Cache?")
        subtitle: KI18n.i18nc("@info", "This removes all saved contacts and addresses from this computer. Google contacts are fetched again, and addresses are read again from the stored mail. Nothing is deleted at Google.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Clear Cache")
                icon.name: "edit-clear-history"
                onTriggered: {
                    clearDialog.close();
                    MailController.clearContacts();
                }
            },
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: clearDialog.close()
            }
        ]
    }
}
