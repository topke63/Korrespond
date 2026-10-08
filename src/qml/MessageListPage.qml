// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.ScrollablePage {
    id: page

    // Opens `uid` in the column after this page, reusing the message page if it is shown.
    function openMessage(uid) {
        MailController.openMessage(uid);
        if (Kirigami.ColumnView.index === applicationWindow().pageStack.depth - 1) {
            applicationWindow().pageStack.push(messagePage);
        } else {
            applicationWindow().pageStack.currentIndex = Kirigami.ColumnView.index + 1;
        }
    }

    title: MailController.currentFolderName

    actions: [
        Kirigami.Action {
            text: KI18n.i18nc("@action:intoolbar", "Refresh")
            icon.name: "view-refresh"
            shortcut: "F5"
            enabled: !MailController.busy
            onTriggered: MailController.refresh()
        }
    ]

    header: QQC2.ProgressBar {
        visible: MailController.busy
        indeterminate: true
    }

    // The open message belongs to the folder being left.
    Connections {
        target: MailController
        function onCurrentFolderChanged() {
            applicationWindow().pageStack.pop(page);
        }
    }

    Component {
        id: messagePage

        MessagePage {}
    }

    ListView {
        id: list

        model: MailController.messageModel

        delegate: QQC2.ItemDelegate {
            id: row

            required property var uid
            required property string subject
            required property string from
            required property string dateText
            required property bool unread

            width: list.width
            highlighted: uid === MailController.currentUid
            onClicked: page.openMessage(uid)

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.from
                        font.bold: row.unread
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.subject.length > 0 ? row.subject : KI18n.i18nc("@info", "(no subject)")
                        font.bold: row.unread
                        elide: Text.ElideRight
                        opacity: row.unread ? 1 : 0.8
                    }
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignTop
                    text: row.dateText
                    font.bold: row.unread
                    opacity: 0.8
                }
            }
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0 && !MailController.busy
            icon.name: "mail-folder-inbox"
            text: KI18n.i18nc("@info:placeholder", "No messages")
        }
    }
}
