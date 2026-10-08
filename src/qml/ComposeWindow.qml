// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

// New Message window. Sending arrives in a later milestone, so Send is disabled for now.
Kirigami.ApplicationWindow {
    id: composer

    readonly property bool hasContent: toField.text.length > 0 || ccField.text.length > 0
        || subjectField.text.length > 0 || bodyArea.text.length > 0
    property bool discardConfirmed: false

    title: subjectField.text.length > 0 ? subjectField.text : KI18n.i18nc("@title:window", "New Message")
    width: Kirigami.Units.gridUnit * 38
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 16
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    // Closing with typed text asks first, like Discard.
    onClosing: close => {
        if (hasContent && !discardConfirmed) {
            close.accepted = false;
            discardDialog.open();
        } else {
            destroy();
        }
    }

    pageStack.initialPage: Kirigami.Page {
        padding: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // From / To / Cc / Subject rows with hairline dividers, like the mail clients it is modeled on.
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.topMargin: Kirigami.Units.largeSpacing
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "From:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: MailController.accountDisplayName.length > 0 && MailController.accountDisplayName !== MailController.accountEmail
                        ? MailController.accountDisplayName + " <" + MailController.accountEmail + ">"
                        : MailController.accountEmail
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "To:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.TextField {
                    id: toField

                    Layout.fillWidth: true
                    placeholderText: KI18n.i18nc("@info:placeholder", "name@example.com, …")
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    focus: true
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Cc:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.TextField {
                    id: ccField

                    Layout.fillWidth: true
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Subject:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.TextField {
                    id: subjectField

                    Layout.fillWidth: true
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                QQC2.TextArea {
                    id: bodyArea

                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.PlainText
                    placeholderText: KI18n.i18nc("@info:placeholder", "Write your message…")
                    leftPadding: Kirigami.Units.gridUnit
                    rightPadding: Kirigami.Units.gridUnit
                    topPadding: Kirigami.Units.largeSpacing
                    background: null
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                Layout.leftMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    text: KI18n.i18nc("@info", "Sending mail comes in a later version.")
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                }
                QQC2.Button {
                    text: KI18n.i18nc("@action:button", "Discard")
                    icon.name: "edit-delete"
                    onClicked: composer.close()
                }
                QQC2.Button {
                    text: KI18n.i18nc("@action:button", "Send")
                    icon.name: "document-send"
                    enabled: false // sending is a later milestone
                }
            }
        }
    }

    Kirigami.PromptDialog {
        id: discardDialog

        title: KI18n.i18nc("@title:dialog", "Discard Message?")
        subtitle: KI18n.i18nc("@info", "The text you wrote will be lost.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Discard")
                icon.name: "edit-delete"
                onTriggered: {
                    composer.discardConfirmed = true;
                    discardDialog.close();
                    composer.close();
                }
            },
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Keep Writing")
                icon.name: "document-edit"
                onTriggered: discardDialog.close()
            }
        ]
    }
}
