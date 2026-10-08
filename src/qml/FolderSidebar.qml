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

    signal storageSettingsRequested()
    signal removeAccountRequested()

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

    edge: Qt.application.layoutDirection === Qt.RightToLeft ? Qt.RightEdge : Qt.LeftEdge
    width: Kirigami.Units.gridUnit * 14
    handleVisible: modal && enabled
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    contentItem: ColumnLayout {
        spacing: 0

        QQC2.ToolBar {
            Layout.fillWidth: true
            Layout.preferredHeight: applicationWindow().pageStack.globalToolBar.preferredHeight

            contentItem: RowLayout {
                spacing: Kirigami.Units.smallSpacing

                SenderAvatar {
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    name: MailController.accountEmail
                }
                QQC2.Label {
                    Layout.fillWidth: true
                    text: MailController.accountEmail
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                }
                QQC2.ToolButton {
                    icon.name: "application-menu"
                    display: QQC2.AbstractButton.IconOnly
                    text: KI18n.i18nc("@action:button", "Account Menu")
                    down: accountMenu.visible
                    onClicked: accountMenu.popup(this, 0, height)

                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                    QQC2.Menu {
                        id: accountMenu

                        QQC2.MenuItem {
                            text: KI18n.i18nc("@action:inmenu", "Local Storage…")
                            icon.name: "drive-harddisk"
                            onTriggered: sidebar.storageSettingsRequested()
                        }
                        QQC2.MenuSeparator {}
                        QQC2.MenuItem {
                            text: KI18n.i18nc("@action:inmenu", "Remove Account…")
                            icon.name: "list-remove-user"
                            onTriggered: sidebar.removeAccountRequested()
                        }
                    }
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing * 1.5
            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
            text: KI18n.i18nc("@title:group", "Folders")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: folderList

                clip: true
                bottomMargin: Kirigami.Units.smallSpacing
                spacing: 1
                model: MailController.folderModel

                delegate: QQC2.ItemDelegate {
                    id: folderRow

                    required property string path
                    required property string name
                    required property string iconName
                    required property string folderRole
                    required property int depth

                    width: ListView.view.width - Kirigami.Units.smallSpacing * 2
                    x: Kirigami.Units.smallSpacing
                    leftPadding: Kirigami.Units.largeSpacing + depth * Kirigami.Units.gridUnit
                    topPadding: Kirigami.Units.smallSpacing + 1
                    bottomPadding: Kirigami.Units.smallSpacing + 1
                    text: name
                    icon.name: iconName
                    icon.color: sidebar.roleColor(folderRole)
                    highlighted: path === MailController.currentFolder
                    font.weight: highlighted ? Font.DemiBold : Font.Normal
                    onClicked: {
                        MailController.selectFolder(path);
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
        }
    }
}
