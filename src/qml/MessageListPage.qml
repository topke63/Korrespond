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

    // The unread dot keeps the active highlight even while the window is inactive.
    SystemPalette {
        id: activePalette
        colorGroup: SystemPalette.Active
    }

    title: MailController.searchActive ? KI18n.i18nc("@title", "Search") : MailController.currentFolderName
    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false
    titleDelegate: RowLayout {
        spacing: 0

        // Brings the docked sidebar back while it is collapsed.
        QQC2.ToolButton {
            visible: MailController.sidebarCollapsed && (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).wideScreen
            icon.name: "sidebar-expand-left"
            display: QQC2.AbstractButton.IconOnly
            text: KI18n.i18nc("@action:button", "Show Sidebar")
            onClicked: MailController.sidebarCollapsed = false

            QQC2.ToolTip.text: text + " (Ctrl+B)"
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }
        Kirigami.Heading {
            level: 2
            font.weight: Font.Bold
            text: page.title
            elide: Text.ElideRight
            Layout.fillWidth: true
            leftPadding: Kirigami.Units.largeSpacing
        }
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

    // The column divider on the right edge, over the title bar band and the list (Main.qml turns the page row's own
    // separators off).
    Rectangle {
        parent: page.overlay
        visible: resizer.visible
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Qt.alpha(Kirigami.Theme.textColor, MimeTheme.columnDividerAlpha)
    }

    // The title bar band: the page row's header gets the divider on its right edge.
    function styleHeader() {
        const header = (page.Kirigami.ColumnView.globalHeader as Loader)?.item as Kirigami.AbstractApplicationHeader;
        if (header) {
            header.background = headerBand.createObject(header);
        }
    }
    Component.onCompleted: styleHeader()
    Connections {
        target: page.Kirigami.ColumnView.globalHeader as Loader
        ignoreUnknownSignals: true

        function onLoaded() {
            page.styleHeader();
        }
    }
    Component {
        id: headerBand

        Rectangle {
            id: band

            // The Header set; the divider takes its text colour. A scheme without Header colours
            // gets Kirigami's dark fallback, so the Window set is used when its text would not read
            // on the Header background.
            readonly property bool useWindow: MimeTheme.contrast(Kirigami.Theme.backgroundColor, windowSet.Kirigami.Theme.textColor) < 4.5
            Kirigami.Theme.colorSet: Kirigami.Theme.Header
            Kirigami.Theme.inherit: false
            color: useWindow ? windowSet.Kirigami.Theme.backgroundColor : Kirigami.Theme.backgroundColor

            Item {
                id: windowSet
                Kirigami.Theme.colorSet: Kirigami.Theme.Window
                Kirigami.Theme.inherit: false
            }

            // The header's own contents (title, actions) take their colours from the toolbar's set.
            Binding {
                target: (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).pageStack.globalToolBar
                property: "colorSet"
                value: Kirigami.Theme.Window
                when: useWindow
            }

            Rectangle {
                visible: resizer.visible
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Qt.alpha(band.useWindow ? windowSet.Kirigami.Theme.textColor : Kirigami.Theme.textColor, MimeTheme.columnDividerAlpha)
            }
        }
    }

    // The window's action collection (set by Main.qml); one definition serves the toolbar and the keys.
    property MailApplication application

    // Opens the message `delta` rows from the open one (Next and Previous Message); the list's
    // order is the model's. Roles: Qt.UserRole + 1 is the uid, + 2 the account (MessageListModel).
    function step(delta) {
        const model = MailController.messageModel;
        const count = model.rowCount();
        if (count === 0) {
            return;
        }
        let current = -1;
        for (let i = 0; i < count; ++i) {
            if (model.data(model.index(i, 0), Qt.UserRole + 1) == MailController.currentUid) {
                current = i;
                break;
            }
        }
        const target = current < 0 ? (delta > 0 ? 0 : count - 1) : Math.max(0, Math.min(count - 1, current + delta));
        if (target === current) {
            return;
        }
        const index = model.index(target, 0);
        MailController.openMessage(model.data(index, Qt.UserRole + 2), model.data(index, Qt.UserRole + 1));
        list.positionViewAtIndex(target, ListView.Contain);
    }

    Connections {
        target: page.application

        function onNextMessageRequested() {
            page.step(1);
        }
        function onPreviousMessageRequested() {
            page.step(-1);
        }
    }

    actions: [
        Kirigami.Action {
            fromQAction: page.application?.action("compose") ?? null
            displayHint: Kirigami.DisplayHint.IconOnly
            tooltip: KI18n.i18nc("@info:tooltip", "Write a new message (Ctrl+N)")
        },
        Kirigami.Action {
            fromQAction: page.application?.action("refresh") ?? null
            displayHint: Kirigami.DisplayHint.IconOnly
            tooltip: KI18n.i18nc("@info:tooltip", "Check for new mail (F5)")
        }
    ]

    header: QQC2.ProgressBar {
        visible: MailController.busy
        indeterminate: true
    }

    ListView {
        id: list

        model: MailController.messageModel

        // Up and Down open the previous or next message, like a click on that row (same as k and j).
        // Handled here, not as window shortcuts, so the sidebar and the text fields keep their arrows.
        Keys.onUpPressed: event => {
            // Only with the list itself in focus (a key from elsewhere in the page is not ours).
            if (list.activeFocus) {
                page.step(-1);
                event.accepted = true;
            }
        }
        Keys.onDownPressed: event => {
            // Only with the list itself in focus (a key from elsewhere in the page is not ours).
            if (list.activeFocus) {
                page.step(1);
                event.accepted = true;
            }
        }

        // Mimestream-like rows: unread dot, sender + date, subject, one-line preview of stored mail; no dividers, and
        // selection and hover as rounded rects inset from the list edges (avatars are only in the message view).
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
            leftInset: Kirigami.Units.largeSpacing * 1.5
            rightInset: Kirigami.Units.largeSpacing * 1.5
            topPadding: Kirigami.Units.smallSpacing * 2
            bottomPadding: Kirigami.Units.smallSpacing * 2
            leftPadding: Kirigami.Units.largeSpacing * 1.5 + Kirigami.Units.smallSpacing
            rightPadding: Kirigami.Units.largeSpacing * 1.5 + Kirigami.Units.smallSpacing
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
                    onTriggered: page.application.action(MailController.messageUnread ? "mark_read" : "mark_unread").trigger()
                }
                QQC2.MenuItem {
                    text: MailController.messageFlagged ? KI18n.i18nc("@action:inmenu", "Unflag") : KI18n.i18nc("@action:inmenu", "Flag")
                    icon.name: "flag"
                    onTriggered: page.application.action("flag").trigger()
                }
                QQC2.MenuSeparator {}
                QQC2.MenuItem {
                    text: KI18n.i18nc("@action:inmenu", "Archive")
                    icon.name: "archive-insert"
                    enabled: MailController.canArchive
                    onTriggered: page.application.action("archive").trigger()
                }
                QQC2.MenuItem {
                    text: KI18n.i18nc("@action:inmenu", "Delete")
                    icon.name: "edit-delete"
                    enabled: MailController.canDelete
                    onTriggered: page.application.action("delete").trigger()
                }
                MoveFolderMenu {
                    title: KI18n.i18nc("@action:inmenu", "Move to Folder")
                    enabled: MailController.canMove
                }
            }

            background: Rectangle {
                radius: 6
                color: row.highlighted ? Qt.alpha(Kirigami.Theme.highlightColor, MimeTheme.selectedAlpha)
                     : row.hovered ? Qt.alpha(Kirigami.Theme.textColor, MimeTheme.hoverAlpha) : "transparent"
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                // Unread: a dot in the active-group highlight, so it stays clear in an inactive window.
                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: Math.round(senderLabel.height / 2 - height / 2)
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    implicitWidth: 10
                    implicitHeight: 10
                    radius: width / 2
                    color: MimeTheme.readableOn(activePalette.highlight, Kirigami.Theme.backgroundColor, 3)
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
                            icon.color: row.flagged ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.disabledTextColor
                            icon.width: Kirigami.Units.iconSizes.small
                            icon.height: Kirigami.Units.iconSizes.small
                            display: QQC2.AbstractButton.IconOnly
                            text: row.flagged ? KI18n.i18nc("@action:button", "Unflag") : KI18n.i18nc("@action:button", "Flag")
                            onClicked: MailController.setFlagged(row.account, row.uid, !row.flagged)
                        }
                        QQC2.Label {
                            text: row.dateText
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.subject.length > 0 ? row.subject : KI18n.i18nc("@info", "(no subject)")
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }
                    // Only stored messages have a preview; the row stays two lines otherwise.
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: row.preview.length > 0
                        text: row.preview
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Kirigami.Theme.disabledTextColor
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
