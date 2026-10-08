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

        // Clean rows (Mimestream-like): unread dot, solid avatar, sender + date, subject below.
        delegate: QQC2.ItemDelegate {
            id: row

            required property var uid
            required property string subject
            required property string from
            required property string dateText
            required property bool unread

            readonly property color secondaryColor: highlighted ? Kirigami.Theme.highlightedTextColor
                                                                : Kirigami.Theme.disabledTextColor

            width: list.width
            topInset: 2
            bottomInset: 2
            leftInset: Kirigami.Units.smallSpacing
            rightInset: Kirigami.Units.smallSpacing
            topPadding: Kirigami.Units.smallSpacing * 2
            bottomPadding: Kirigami.Units.smallSpacing * 2
            leftPadding: Kirigami.Units.smallSpacing * 2
            rightPadding: Kirigami.Units.largeSpacing * 1.5
            highlighted: uid === MailController.currentUid
            onClicked: page.openMessage(uid)

            background: Rectangle {
                radius: Kirigami.Units.cornerRadius
                color: row.highlighted ? Kirigami.Theme.highlightColor
                     : row.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.06) : "transparent"
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.mediumSpacing

                Rectangle {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: 7
                    implicitHeight: 7
                    radius: width / 2
                    color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.highlightColor
                    opacity: row.unread ? 1 : 0
                }

                SenderAvatar {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.gridUnit * 2
                    name: row.from
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: row.from
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            font.weight: row.unread ? Font.Bold : Font.DemiBold
                            color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }
                        QQC2.Label {
                            text: row.dateText
                            font: Kirigami.Theme.smallFont
                            color: row.unread && !row.highlighted ? Kirigami.Theme.highlightColor : row.secondaryColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.subject.length > 0 ? row.subject : KI18n.i18nc("@info", "(no subject)")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: row.highlighted || row.unread ? (row.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor)
                                                             : row.secondaryColor
                    }
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
