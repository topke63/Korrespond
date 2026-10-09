// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.ScrollablePage {
    id: page

    // Shows message `uid` of `account` in the message column after this page (on narrow windows,
    // moves there).
    function openMessage(account, uid) {
        MailController.openMessage(account, uid);
        (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).pageStack.currentIndex = Kirigami.ColumnView.index + 1;
    }

    title: MailController.searchActive ? KI18n.i18nc("@title", "Search") : MailController.currentFolderName
    // Fixed accent for unread: the theme's highlight turns dull when the window is inactive.
    readonly property color unreadColor: "#3daee9"
    titleDelegate: Kirigami.Heading {
        level: 2
        font.weight: Font.Bold
        text: page.title
        elide: Text.ElideRight
        Layout.fillWidth: true
        leftPadding: Kirigami.Units.largeSpacing
    }
    // A fixed list column, like Geary's; the message column takes the rest.
    // The width the user chose (live while dragging the divider on the right edge); the message
    // column takes the rest. Main.qml puts the page row in the dynamic mode, where these count.
    Kirigami.ColumnView.minimumWidth: MailController.listMinWidth
    Kirigami.ColumnView.preferredWidth: Math.min(maxWidth, resizer.dragging ? resizer.liveWidth : MailController.listWidth)
    Kirigami.ColumnView.maximumWidth: maxWidth

    // The most the list may take: the message column keeps at least 30 grid units (room for its toolbar), so a width
    // saved on a bigger window (or a smaller window now) cannot push it off the screen.
    readonly property int maxWidth: Math.max(MailController.listMinWidth,
                                             Math.min(MailController.listMaxWidth,
                                                      (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).width - MailController.sidebarWidth - Kirigami.Units.gridUnit * 30))

    // On the page's overlay, so it does not scroll with the list. Not in a narrow window, where
    // one column is shown at a time.
    PanelResizer {
        id: resizer

        parent: page.overlay
        visible: (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).wideScreen
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        panelWidth: MailController.listWidth
        minimum: MailController.listMinWidth
        maximum: page.maxWidth
        onResized: width => MailController.savePanelWidths(MailController.sidebarWidth, width)
        onResetRequested: MailController.savePanelWidths(MailController.sidebarWidth, MailController.listDefaultWidth)
    }

    actions: [
        Kirigami.Action {
            text: KI18n.i18nc("@action:intoolbar", "New Message")
            icon.name: "mail-message-new"
            displayHint: Kirigami.DisplayHint.IconOnly
            tooltip: KI18n.i18nc("@info:tooltip", "Write a new message (Ctrl+N)")
            shortcut: "Ctrl+N"
            onTriggered: {
                // A separate window per message; it destroys itself when closed.
                const component = Qt.createComponent("PlasmaMail", "ComposeWindow");
                const composer = component.createObject((page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow));
                if (composer) {
                    (composer as Window).show();
                } else {
                    console.warn(component.errorString());
                }
            }
        },
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

        // Mimestream-like rows: unread dot, sender + date, subject, preview of stored mail; hairline dividers and a soft
        // neutral selection (avatars are only in the message view).
        delegate: QQC2.ItemDelegate {
            id: row

            required property int index
            required property string account
            required property var uid
            required property string subject
            required property string from
            required property string dateText
            required property bool unread
            required property bool flagged
            required property string preview

            width: list.width
            topInset: 1
            bottomInset: 1
            leftInset: Kirigami.Units.smallSpacing
            rightInset: Kirigami.Units.smallSpacing
            topPadding: Kirigami.Units.smallSpacing * 2
            bottomPadding: Kirigami.Units.smallSpacing * 2
            leftPadding: Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.largeSpacing * 1.5
            highlighted: uid === MailController.currentUid && account === MailController.currentMessageAccount
            onClicked: page.openMessage(account, uid)

            // Right click selects the message, then offers the same actions as the toolbar.
            TapHandler {
                acceptedButtons: Qt.RightButton
                onTapped: {
                    page.openMessage(row.account, row.uid);
                    rowMenu.popup();
                }
            }

            QQC2.Menu {
                id: rowMenu

                QQC2.MenuItem {
                    text: MailController.messageUnread ? KI18n.i18nc("@action:inmenu", "Mark as Read") : KI18n.i18nc("@action:inmenu", "Mark as Unread")
                    icon.name: MailController.messageUnread ? "mail-mark-read" : "mail-mark-unread"
                    onTriggered: MailController.setUnread(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageUnread)
                }
                QQC2.MenuItem {
                    text: MailController.messageFlagged ? KI18n.i18nc("@action:inmenu", "Unflag") : KI18n.i18nc("@action:inmenu", "Flag")
                    icon.name: "flag"
                    onTriggered: MailController.setFlagged(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageFlagged)
                }
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    text: KI18n.i18nc("@action:inmenu", "Archive")
                    icon.name: "archive-insert"
                    enabled: MailController.canArchive
                    onTriggered: MailController.archiveMessage()
                }
                QQC2.MenuItem {
                    text: KI18n.i18nc("@action:inmenu", "Delete")
                    icon.name: "edit-delete"
                    enabled: MailController.canDelete
                    onTriggered: MailController.deleteMessage()
                }
                MoveFolderMenu {
                    title: KI18n.i18nc("@action:inmenu", "Move to Folder")
                    enabled: MailController.canMove
                }
            }

            background: Rectangle {
                radius: Kirigami.Units.cornerRadius
                color: row.highlighted ? Qt.alpha(Kirigami.Theme.textColor, 0.12)
                     : row.unread ? Qt.alpha(page.unreadColor, 0.10)
                     : row.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.05) : "transparent"

                // Accent bar on the left edge of unread rows.
                Rectangle {
                    visible: row.unread
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.topMargin: Kirigami.Units.smallSpacing
                    anchors.bottomMargin: Kirigami.Units.smallSpacing
                    width: 3
                    radius: 1.5
                    color: page.unreadColor
                }
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Unread: a clear accent dot (the row also gets an accent bar, below).
                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: Math.round(senderLabel.height / 2 - height / 2)
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    implicitWidth: 10
                    implicitHeight: 10
                    radius: width / 2
                    color: page.unreadColor
                    opacity: row.unread ? 1 : 0
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            id: senderLabel

                            Layout.fillWidth: true
                            text: row.from
                            textFormat: Text.PlainText
                            elide: Text.ElideRight
                            font.weight: row.unread ? Font.Bold : Font.DemiBold
                        }
                        // Orange when flagged; faint on hover so any row can be flagged with a click.
                        QQC2.ToolButton {
                            implicitWidth: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing * 2
                            implicitHeight: implicitWidth
                            padding: Kirigami.Units.smallSpacing
                            flat: true
                            opacity: row.flagged ? 1 : row.hovered ? 0.5 : 0
                            enabled: row.flagged || row.hovered
                            icon.name: "flag"
                            icon.color: row.flagged ? "#f67400" : Kirigami.Theme.textColor
                            icon.width: Kirigami.Units.iconSizes.small
                            icon.height: Kirigami.Units.iconSizes.small
                            display: QQC2.AbstractButton.IconOnly
                            text: row.flagged ? KI18n.i18nc("@action:button", "Unflag") : KI18n.i18nc("@action:button", "Flag")
                            onClicked: MailController.setFlagged(row.account, row.uid, !row.flagged)
                        }
                        QQC2.Label {
                            text: row.dateText
                            font: Kirigami.Theme.smallFont
                            color: row.unread ? page.unreadColor : Kirigami.Theme.disabledTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.subject.length > 0 ? row.subject : KI18n.i18nc("@info", "(no subject)")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        opacity: row.unread ? 1 : 0.85
                    }
                    // Only stored messages have a preview; the row stays two lines otherwise.
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: row.preview.length > 0
                        text: row.preview
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }

            // Hairline divider under the text, hidden around the selected row.
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: Kirigami.Units.gridUnit * 1.5
                anchors.rightMargin: Kirigami.Units.largeSpacing
                height: 1
                color: Qt.alpha(Kirigami.Theme.textColor, 0.08)
                visible: !row.highlighted && !row.hovered
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
