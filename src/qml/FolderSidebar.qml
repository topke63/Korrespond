// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

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

    edge: Application.layoutDirection === Qt.RightToLeft ? Qt.RightEdge : Qt.LeftEdge
    // Docked: the width the user chose (live while dragging). Sliding over a narrow window: fixed.
    width: modal ? Kirigami.Units.gridUnit * 14 : Math.min(maxDockedWidth, resizer.dragging ? resizer.liveWidth : MailController.sidebarWidth)
    // Docked beside the pages the sidebar fills the page row. The drawer's own height subtracts the
    // window's footer (the status bar), which the page row has already left out.
    Binding {
        target: sidebar
        property: "height"
        value: sidebar.parent ? sidebar.parent.height : sidebar.implicitHeight
        when: !sidebar.modal
    }
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

    // The "Mail" row takes the Header set, unless that set's background would not read against the
    // Window text (a scheme without Header colours gets Kirigami's dark fallback); then the Window set.
    readonly property bool headerUseWindow: MimeTheme.contrast(headerProbe.Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor) < 4.5

    // The drawer template's own background takes the theme's window colour.
    background: Rectangle {
        color: Kirigami.Theme.backgroundColor
    }

    // The folder list, with the drag handle for the width on its right edge.
    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Only probes the Header set's background for headerUseWindow. It must stay visible: an
            // invisible item resolves Kirigami.Theme to black, which gave the wrong answer on Light.
            // Zero height, so the column (spacing 0) leaves no gap.
            Item {
                id: headerProbe
                Layout.preferredHeight: 0
                Layout.maximumHeight: 0
                Kirigami.Theme.colorSet: Kirigami.Theme.Header
                Kirigami.Theme.inherit: false
            }

            QQC2.ToolBar {
                Layout.fillWidth: true
                Layout.preferredHeight: applicationWindow().pageStack.globalToolBar.preferredHeight
                // Replaces the desktop style's Header default, so the background and the row's contents
                // (logo, title, buttons) are the same set.
                Kirigami.Theme.colorSet: sidebar.headerUseWindow ? Kirigami.Theme.Window : Kirigami.Theme.Header
                Kirigami.Theme.inherit: false

                background: Rectangle {
                    color: Kirigami.Theme.backgroundColor
                }
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    // The app logo: the installed themed icon, else the copy inside the executable.
                    Kirigami.Icon {
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                        Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        source: "org.topke.plasmamail"
                        fallback: ":/icons/sc-apps-org.topke.plasmamail.svg"
                        isMask: false
                    }
                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 2
                        font.weight: Font.Bold
                        text: KI18n.i18nc("@title", "Mail")
                        elide: Text.ElideRight
                    }
                    QQC2.ToolButton {
                        icon.name: "sidebar-collapse-left"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Hide Sidebar")
                        onClicked: sidebar.modal ? sidebar.close() : MailController.sidebarCollapsed = true

                        QQC2.ToolTip.text: text + " (Ctrl+B)"
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
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
                                text: KI18n.i18nc("@action:inmenu", "Collapse All")
                                icon.name: "arrow-right"
                                enabled: MailController.hasAccount
                                onTriggered: MailController.setAllAccountsCollapsed(true)
                            }
                            QQC2.MenuItem {
                                text: KI18n.i18nc("@action:inmenu", "Expand All")
                                icon.name: "arrow-down"
                                enabled: MailController.collapsedAccounts.length > 0
                                onTriggered: MailController.setAllAccountsCollapsed(false)
                            }
                            QQC2.MenuSeparator {
                            }
                            // Display only: hidden folders still count and sync.
                            QQC2.MenuItem {
                                text: KI18n.i18nc("@action:inmenu", "Show Hidden Folders")
                                checkable: true
                                checked: MailController.showHiddenFolders
                                onToggled: MailController.showHiddenFolders = checked
                            }
                            QQC2.MenuSeparator {
                            }
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
                        }
                    }

                    // One section per account, in the order they were added.
                    Repeater {
                        model: MailController.accounts

                        delegate: ColumnLayout {
                            id: accountSection

                            required property string email
                            required property var folderModel
                            // Folded accounts are kept by the controller (saved, restored at start).
                            readonly property bool expanded: MailController.collapsedAccounts.indexOf(email) < 0

                            Layout.fillWidth: true
                            spacing: 1

                            QQC2.ItemDelegate {
                                id: accountHeader

                                Layout.fillWidth: true
                                Layout.topMargin: Kirigami.Units.largeSpacing
                                Layout.leftMargin: Kirigami.Units.smallSpacing
                                Layout.rightMargin: Kirigami.Units.smallSpacing
                                // The folder rows' padding, so the header has their height.
                                leftPadding: Kirigami.Units.largeSpacing
                                topPadding: Kirigami.Units.smallSpacing + 3
                                bottomPadding: Kirigami.Units.smallSpacing + 3
                                onClicked: MailController.setAccountCollapsed(accountSection.email, accountSection.expanded)
                                Accessible.name: KI18n.i18nc("@action:button", "Show or hide the folders of %1", accountSection.email)

                                background: Rectangle {
                                    radius: Kirigami.Units.cornerRadius
                                    color: accountHeader.hovered ? Qt.alpha(Kirigami.Theme.textColor, MimeTheme.hoverAlpha) : "transparent"
                                }
                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.mediumSpacing

                                    Kirigami.Icon {
                                        implicitWidth: Kirigami.Units.iconSizes.small
                                        implicitHeight: Kirigami.Units.iconSizes.small
                                        source: accountSection.expanded ? "arrow-down" : "arrow-right"
                                        color: Kirigami.Theme.disabledTextColor
                                        isMask: true
                                    }
                                    SenderAvatar {
                                        implicitWidth: Kirigami.Units.iconSizes.small
                                        name: accountSection.email
                                    }
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: accountSection.email
                                        elide: Text.ElideRight
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                        font.weight: Font.DemiBold
                                        color: Kirigami.Theme.disabledTextColor
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
                                    // Inbox can't be hidden. Hiding a folder hides only its own row; its
                                    // subfolders stay, at their usual indent. The selected folder stays
                                    // listed so the selection doesn't vanish.
                                    canHide: model.folderRole !== "inbox" && model.path.toUpperCase() !== "INBOX"
                                    hidden: (MailController.hiddenFolders[accountSection.email] || []).indexOf(model.path) >= 0
                                    visible: !hidden || MailController.showHiddenFolders || highlighted
                                    // Indented under the account header.
                                    leftPadding: Kirigami.Units.gridUnit * 1.5 + model.depth * Kirigami.Units.gridUnit
                                    text: model.name
                                    icon.name: model.iconName
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

        // The column divider on the right edge, over the header and the folders.
        Rectangle {
            visible: !sidebar.modal
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Qt.alpha(Kirigami.Theme.textColor, MimeTheme.columnDividerAlpha)
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

    // Small dim heading above a group of rows (DemiBold, not uppercase, as in Mimestream).
    component SectionLabel: QQC2.Label {
        Layout.fillWidth: true
        // The folder rows' text column: row margin, left padding, icon and the gap after it.
        Layout.leftMargin: Kirigami.Units.smallSpacing + Kirigami.Units.largeSpacing
                           + Kirigami.Units.iconSizes.small + Kirigami.Units.mediumSpacing
        Layout.topMargin: Kirigami.Units.largeSpacing
        Layout.bottomMargin: Kirigami.Units.smallSpacing
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        font.weight: Font.DemiBold
        color: Kirigami.Theme.disabledTextColor
    }

    // A clickable folder (or shared view) with a monochrome tinted icon and the soft selection.
    component FolderRow: QQC2.ItemDelegate {
        id: folderRow

        // The account a folder belongs to; empty for the shared views.
        property string account
        property string path
        // Only account folders can be hidden from the sidebar (not the shared views, not Inbox).
        property bool canHide: false
        property bool hidden: false

        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.smallSpacing
        Layout.rightMargin: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.largeSpacing
        topPadding: Kirigami.Units.smallSpacing + 3
        bottomPadding: Kirigami.Units.smallSpacing + 3
        highlighted: path === MailController.currentFolder && account === MailController.currentAccount
        onClicked: {
            MailController.selectFolder(account, path);
            if (sidebar.modal) {
                sidebar.close();
            }
        }

        TapHandler {
            acceptedButtons: Qt.RightButton
            enabled: folderRow.canHide || folderRow.hidden
            onTapped: rowMenu.popup()
        }
        QQC2.Menu {
            id: rowMenu

            QQC2.MenuItem {
                text: folderRow.hidden ? KI18n.i18nc("@action:inmenu", "Show in Sidebar")
                                       : KI18n.i18nc("@action:inmenu", "Hide from Sidebar")
                icon.name: folderRow.hidden ? "view-visible" : "view-hidden"
                onTriggered: MailController.setFolderHidden(folderRow.account, folderRow.path, !folderRow.hidden)
            }
        }

        background: Rectangle {
            radius: Kirigami.Units.cornerRadius
            color: folderRow.highlighted ? Qt.alpha(Kirigami.Theme.highlightColor, MimeTheme.selectedAlpha)
                 : folderRow.hovered ? Qt.alpha(Kirigami.Theme.textColor, MimeTheme.hoverAlpha) : "transparent"
        }
        contentItem: RowLayout {
            spacing: Kirigami.Units.mediumSpacing

            Kirigami.Icon {
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
                source: folderRow.icon.name
                color: folderRow.hidden ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                isMask: true
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: folderRow.text
                font: folderRow.font
                color: folderRow.hidden ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.textColor
                elide: Text.ElideRight
            }
        }
    }
}
