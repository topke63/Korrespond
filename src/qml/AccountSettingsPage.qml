// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.formcard as FormCard
import PlasmaMail

// Settings → Accounts: the account manager. Lists the accounts; one opens its details.
FormCard.FormCardPage {
    id: page

    title: KI18n.i18nc("@title", "Accounts")

    FormCard.FormHeader {
        title: KI18n.i18nc("@title:group", "Accounts")
    }

    FormCard.FormCard {
        // One row per account, in the order they were added.
        Repeater {
            model: MailController.accounts

            delegate: FormCard.AbstractFormDelegate {
                id: accountRow

                required property string email
                required property string displayName
                required property string provider
                required property string server
                required property string authMethod

                onClicked: page.QQC2.ApplicationWindow.window.pageStack.layers.push(detailsPage, {
                    email: email,
                    displayName: displayName,
                    provider: provider,
                    server: server,
                    authMethod: authMethod
                })
                Accessible.name: KI18n.i18nc("@action:button", "Show details of %1", email)

                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing

                    SenderAvatar {
                        implicitWidth: Kirigami.Units.gridUnit * 2
                        name: accountRow.email
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: accountRow.displayName.length > 0 ? accountRow.displayName : accountRow.email
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: KI18n.i18nc("@info address · provider", "%1 · %2", accountRow.email, accountRow.provider)
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                            elide: Text.ElideRight
                        }
                    }
                    Kirigami.Icon {
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        source: "go-next-symbolic"
                    }
                }
            }
        }
    }

    FormCard.FormCard {
        Layout.topMargin: Kirigami.Units.largeSpacing

        FormCard.FormButtonDelegate {
            text: KI18n.i18nc("@action:button", "Add Account…")
            icon.name: "list-add-user"
            onClicked: page.QQC2.ApplicationWindow.window.pageStack.layers.push(setupPage)
        }
    }

    Component {
        id: detailsPage

        AccountDetailsPage {}
    }

    Component {
        id: setupPage

        AccountSetupPage {
            asLayer: true
        }
    }
}
