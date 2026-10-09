// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

// Folder list beside the pages (docked on wide windows, sliding in on narrow ones), with the
// account and its menu in a header that lines up with the pages' title bars.
Kirigami.OverlayDrawer {
    id: sidebar

    signal settingsRequested()
    signal aboutRequested()

    // Mimestream-style colored icons by folder role; other folders stay neutral.
    function roleColor(role: string): color {
        switch (role) {
        case "inbox": return "#3daee9";
        case "drafts": return "#1abc9c";
        case "sent": return "#27ae60";
        case "archive": return "#9b59b6";
        case "junk": return "#f39c12";
        case "trash": return "#da4453";
        default: return Kirigami.Theme.textColor;
        }
    }

    function sharedColor(path: string): color {
        switch (path) {
        case "plasmamail:inboxes": return "#3daee9";
        case "plasmamail:unread": return Kirigami.Theme.highlightColor;
        case "plasmamail:flagged": return "#f67400";
        default: return Kirigami.Theme.textColor;
        }
    }

    edge: Qt.application.layoutDirection === Qt.RightToLeft ? Qt.RightEdge : Qt.LeftEdge
    // Docked: the width the user chose (live while dragging). Sliding over a narrow window: fixed.
    width: modal ? Kirigami.Units.gridUnit * 14 : Math.min(maxDockedWidth, resizer.dragging ? resizer.liveWidth : MailController.sidebarWidth)
    // The most it may take: the list keeps its minimum and the message column 30 grid units.
    readonly property int maxDockedWidth: Math.max(MailController.sidebarMinWidth,
                                                   Math.min(MailController.sidebarMaxWidth,
                                                            applicationWindow().width - MailController.listMinWidth - Kirigami.Units.gridUnit * 30))
    handleVisible: modal && enabled
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    // The folder list, with the drag handle for the width on its right edge.
    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            QQC2.ToolBar {
                Layout.fillWidth: true
                Layout.preferredHeight: applicationWindow().pageStack.globalToolBar.preferredHeight

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        level: 2
                        font.weight: Font.Bold
                        text: KI18n.i18nc("@title", "Mail")
                        elide: Text.ElideRight
                    }
                    QQC2.ToolButton {
                        icon.name: "application-menu"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Main Menu")
                        down: accountMenu.visible
                        onClicked: accountMenu.popup(this, 0, height)

                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        QQC2.Menu {
                            id: accountMenu

                            QQC2.MenuItem {
                                text: KI18n.i18nc("@action:inmenu", "Settings…")
                                icon.name: "settings-configure"
                                onTriggered: sidebar.settingsRequested()
                            }
                            QQC2.MenuItem {
                                text: KI18n.i18nc("@action:inmenu", "About Plasma Mail")
                                icon.name: "help-about"
                                onTriggered: sidebar.aboutRequested()
                            }

                        }
                    }
                }
            }

            QQC2.ScrollView {
                id: scroll

                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: scroll.availableWidth
                    spacing: 1

                    SectionLabel {
                        text: KI18n.i18nc("@title:group", "All Accounts")
                    }

                    Repeater {
                        model: MailController.sharedFolders

                        delegate: FolderRow {
                            required property var modelData

                            path: modelData.path
                            text: modelData.name
                            icon.name: modelData.iconName
                            icon.color: sidebar.sharedColor(modelData.path)
                        }
                    }

                    // One section per account, in the order they were added.
                    Repeater {
                        model: MailController.accounts

                        delegate: ColumnLayout {
                            id: accountSection

                            required property string email
                            required property var folderModel
                            property bool expanded: true

                            Layout.fillWidth: true
                            spacing: 1

                            QQC2.ItemDelegate {
                                id: accountHeader

                                Layout.fillWidth: true
                                Layout.topMargin: Kirigami.Units.largeSpacing
                                Layout.leftMargin: Kirigami.Units.smallSpacing
                                Layout.rightMargin: Kirigami.Units.smallSpacing
                                topPadding: Kirigami.Units.smallSpacing
                                bottomPadding: Kirigami.Units.smallSpacing
                                onClicked: accountSection.expanded = !accountSection.expanded
                                Accessible.name: KI18n.i18nc("@action:button", "Show or hide the folders of %1", accountSection.email)

                                background: Rectangle {
                                    radius: Kirigami.Units.cornerRadius
                                    color: accountHeader.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.05) : "transparent"
                                }
                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.smallSpacing

                                    Kirigami.Icon {
                                        implicitWidth: Kirigami.Units.iconSizes.small
                                        implicitHeight: Kirigami.Units.iconSizes.small
                                        source: accountSection.expanded ? "arrow-down" : "arrow-right"
                                        color: Kirigami.Theme.disabledTextColor
                                        isMask: true
                                    }
                                    SenderAvatar {
                                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                        name: accountSection.email
                                    }
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: accountSection.email
                                        elide: Text.ElideRight
                                        font.weight: Font.DemiBold
                                    }
                                }
                            }

                            Repeater {
                                model: accountSection.expanded ? accountSection.folderModel : null

                                // Model data via `model`: redeclaring `path` here would hide FolderRow's own
                                // `path`, which its click handler reads (that left folder clicks doing nothing).
                                delegate: FolderRow {
                                    required property var model

                                    account: accountSection.email
                                    path: model.path
                                    // Indented under the account header.
                                    leftPadding: Kirigami.Units.gridUnit * 1.5 + model.depth * Kirigami.Units.gridUnit
                                    text: model.name
                                    icon.name: model.iconName
                                    icon.color: sidebar.roleColor(model.folderRole)
                                }
                            }
                        }
                    }

                    Item {
                        Layout.preferredHeight: Kirigami.Units.smallSpacing
                    }
                }
            }
        }

        PanelResizer {
            id: resizer

            // Only beside the pages; the sliding sidebar of a narrow window keeps its width.
            visible: !sidebar.modal
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            panelWidth: MailController.sidebarWidth
            minimum: MailController.sidebarMinWidth
            maximum: sidebar.maxDockedWidth
            onResized: width => MailController.savePanelWidths(width, MailController.listWidth)
            onResetRequested: MailController.savePanelWidths(MailController.sidebarDefaultWidth, MailController.listWidth)
        }
    }

    // Small dim heading above a group of rows.
    component SectionLabel: QQC2.Label {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing * 1.5
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
    }

    // A clickable folder (or shared view) with a tinted icon and the soft selection.
    component FolderRow: QQC2.ItemDelegate {
        id: folderRow

        // The account a folder belongs to; empty for the shared views.
        property string account
        property string path

        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.largeSpacing
        topPadding: Kirigami.Units.smallSpacing + 1
        bottomPadding: Kirigami.Units.smallSpacing + 1
        highlighted: path === MailController.currentFolder && account === MailController.currentAccount
        font.weight: highlighted ? Font.DemiBold : Font.Normal
        onClicked: {
            MailController.selectFolder(account, path);
            if (sidebar.modal) {
                sidebar.close();
            }
        }

        background: Rectangle {
            radius: Kirigami.Units.cornerRadius
            color: folderRow.highlighted ? Qt.alpha(Kirigami.Theme.textColor, 0.12)
                 : folderRow.hovered ? Qt.alpha(Kirigami.Theme.textColor, 0.05) : "transparent"
        }
        contentItem: RowLayout {
            spacing: Kirigami.Units.mediumSpacing

            Kirigami.Icon {
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
                source: folderRow.icon.name
                color: folderRow.icon.color
                isMask: true
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: folderRow.text
                font: folderRow.font
                elide: Text.ElideRight
            }
        }
    }
}
