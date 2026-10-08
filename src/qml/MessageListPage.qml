// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.components as Components
import PlasmaMail

Kirigami.ScrollablePage {
    id: page

    // Shows `uid` in the message column after this page (on narrow windows, moves there).
    function openMessage(uid) {
        MailController.openMessage(uid);
        applicationWindow().pageStack.currentIndex = Kirigami.ColumnView.index + 1;
    }

    title: MailController.currentFolderName
    // A fixed list column, like Geary's; the message column takes the rest.
    Kirigami.ColumnView.minimumWidth: Kirigami.Units.gridUnit * 20
    Kirigami.ColumnView.preferredWidth: Kirigami.Units.gridUnit * 28
    Kirigami.ColumnView.maximumWidth: Kirigami.Units.gridUnit * 40

    actions: [
        Kirigami.Action {
            text: KI18n.i18nc("@action:intoolbar", "Refresh")
            icon.name: "view-refresh"
            displayHint: Kirigami.DisplayHint.IconOnly
            tooltip: KI18n.i18nc("@info:tooltip", "Check for new mail (F5)")
            shortcut: "F5"
            enabled: !MailController.busy
            onTriggered: MailController.refresh()
        }
    ]

    header: QQC2.ProgressBar {
        visible: MailController.busy
        indeterminate: true
    }

    ListView {
        id: list

        model: MailController.messageModel

        // Geary-style row: unread dot, sender avatar, sender + date, subject below.
        delegate: QQC2.ItemDelegate {
            id: row

            required property var uid
            required property string subject
            required property string from
            required property string dateText
            required property bool unread

            width: list.width
            topPadding: Kirigami.Units.mediumSpacing
            bottomPadding: Kirigami.Units.mediumSpacing
            leftPadding: Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.largeSpacing
            highlighted: uid === MailController.currentUid
            onClicked: page.openMessage(uid)

            contentItem: RowLayout {
                spacing: Kirigami.Units.mediumSpacing

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.smallSpacing * 2
                    implicitHeight: implicitWidth
                    radius: width / 2
                    color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.highlightColor
                    opacity: row.unread ? 1 : 0
                }

                Components.Avatar {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing * 2
                    Layout.preferredHeight: Layout.preferredWidth
                    name: row.from
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing / 2

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: row.from
                            elide: Text.ElideRight
                            font.weight: row.unread ? Font.Bold : Font.Normal
                            color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }
                        QQC2.Label {
                            text: row.dateText
                            font: Kirigami.Theme.smallFont
                            color: row.highlighted ? Kirigami.Theme.highlightedTextColor
                                 : row.unread ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.subject.length > 0 ? row.subject : KI18n.i18nc("@info", "(no subject)")
                        elide: Text.ElideRight
                        font.weight: row.unread ? Font.DemiBold : Font.Normal
                        color: row.highlighted ? Kirigami.Theme.highlightedTextColor
                             : row.unread ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                    }
                }
            }

            Kirigami.Separator {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Kirigami.Units.gridUnit * 3
                opacity: 0.5
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
