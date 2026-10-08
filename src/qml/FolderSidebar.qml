// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.components as Components
import PlasmaMail

// Folder list beside the pages (docked on wide windows, sliding in on narrow ones), with the
// account and its menu in a header that lines up with the pages' title bars.
Kirigami.OverlayDrawer {
    id: sidebar

    signal storageSettingsRequested()
    signal removeAccountRequested()

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

                Components.Avatar {
                    Layout.preferredWidth: Kirigami.Units.iconSizes.medium
                    Layout.preferredHeight: Kirigami.Units.iconSizes.medium
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

        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: folderList

                clip: true
                topMargin: Kirigami.Units.smallSpacing
                bottomMargin: Kirigami.Units.smallSpacing
                model: MailController.folderModel

                delegate: QQC2.ItemDelegate {
                    required property string path
                    required property string name
                    required property string iconName
                    required property int depth

                    width: ListView.view.width - Kirigami.Units.smallSpacing * 2
                    x: Kirigami.Units.smallSpacing
                    leftPadding: Kirigami.Units.largeSpacing + depth * Kirigami.Units.gridUnit
                    topPadding: Kirigami.Units.smallSpacing * 1.5
                    bottomPadding: Kirigami.Units.smallSpacing * 1.5
                    text: name
                    icon.name: iconName
                    highlighted: path === MailController.currentFolder
                    onClicked: {
                        MailController.selectFolder(path);
                        if (sidebar.modal) {
                            sidebar.close();
                        }
                    }
                }
            }
        }
    }
}
