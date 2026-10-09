// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

// The message window: a new message, or a reply / forward when the initial* values are given
// (see MailController.composeFor()).
Kirigami.ApplicationWindow {
    id: composer

    // What the fields start with; closing asks before discarding only when they were changed.
    property string initialAccount
    property string initialTo
    property string initialCc
    property string initialSubject
    property string initialBody
    property string initialInReplyTo
    property var initialReferences: []

    readonly property bool hasContent: toField.text !== initialTo || ccField.text !== initialCc || bccField.text.length > 0
        || subjectField.text !== initialSubject || bodyArea.text !== initialBody
    property bool discardConfirmed: false
    // True from pressing Send until the server has answered.
    property bool waiting: false
    property string errorText

    title: subjectField.text.length > 0 ? subjectField.text : KI18n.i18nc("@title:window", "New Message")
    width: Kirigami.Units.gridUnit * 38
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 16
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    function send() {
        errorText = "";
        waiting = true;
        MailController.sendMessage({
            account: fromBox.currentValue,
            to: toField.text,
            cc: ccField.text,
            bcc: bccField.text,
            subject: subjectField.text,
            body: bodyArea.text,
            inReplyTo: initialInReplyTo,
            references: initialReferences
        });
    }

    Component.onCompleted: {
        toField.text = initialTo;
        ccField.text = initialCc;
        subjectField.text = initialSubject;
        bodyArea.text = initialBody;
        const index = fromBox.indexOfValue(initialAccount);
        fromBox.currentIndex = index >= 0 ? index : 0;
        // A reply starts with the cursor above the quoted text.
        if (initialTo.length > 0) {
            bodyArea.forceActiveFocus();
            bodyArea.cursorPosition = 0;
        }
    }

    // The answer to Send; only this window's own send counts.
    Connections {
        target: MailController
        enabled: composer.waiting

        function onMessageSent() {
            composer.waiting = false;
            composer.discardConfirmed = true;
            composer.close();
        }
        function onSendFailed(message: string) {
            composer.waiting = false;
            composer.errorText = message;
        }
    }

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
                QQC2.ComboBox {
                    id: fromBox

                    Layout.fillWidth: true
                    model: MailController.accounts
                    valueRole: "email"
                    // "Name <address>", or just the address.
                    textRole: "email"
                    enabled: !composer.waiting
                    delegate: QQC2.ItemDelegate {
                        required property var model

                        width: fromBox.width
                        text: model.displayName.length > 0 && model.displayName !== model.email ? model.displayName + " <" + model.email + ">" : model.email
                        highlighted: fromBox.highlightedIndex === model.index
                    }
                    displayText: {
                        const row = currentIndex;
                        return currentValue ? currentValue : "";
                    }
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
                    enabled: !composer.waiting
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
                    enabled: !composer.waiting
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Bcc:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.TextField {
                    id: bccField

                    Layout.fillWidth: true
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    enabled: !composer.waiting
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Subject:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.TextField {
                    id: subjectField

                    Layout.fillWidth: true
                    enabled: !composer.waiting
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
                    readOnly: composer.waiting
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

                // Progress or the reason the last try failed; the text stays in the window.
                QQC2.Label {
                    Layout.fillWidth: true
                    text: composer.waiting ? KI18n.i18nc("@info:status", "Sending…") : composer.errorText
                    color: composer.waiting ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.negativeTextColor
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                QQC2.BusyIndicator {
                    visible: composer.waiting
                    running: visible
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                }
                QQC2.Button {
                    text: KI18n.i18nc("@action:button", "Discard")
                    icon.name: "edit-delete"
                    enabled: !composer.waiting
                    onClicked: composer.close()
                }
                QQC2.Button {
                    text: KI18n.i18nc("@action:button", "Send")
                    icon.name: "document-send"
                    enabled: !composer.waiting && !MailController.sending && fromBox.currentValue !== undefined
                        && (toField.text.trim().length > 0 || ccField.text.trim().length > 0 || bccField.text.trim().length > 0)
                    onClicked: composer.send()
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
