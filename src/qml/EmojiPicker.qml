// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import Korrespond

// A popup to pick one emoji: a search field, the categories and a grid. Emits picked() with the
// emoji; the popup closes after a pick unless `keepOpen` is set (several emoji in a row).
QQC2.Popup {
    id: picker

    // Set by the caller for the time of one pick, e.g. while Shift is held.
    property bool keepOpen: false

    signal picked(string emoji)

    readonly property int cellSize: Kirigami.Units.gridUnit * 2

    width: cellSize * 8 + padding * 2 + Kirigami.Units.largeSpacing
    height: Kirigami.Units.gridUnit * 18
    padding: Kirigami.Units.smallSpacing
    modal: false
    focus: true
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

    onOpened: {
        searchField.text = "";
        searchField.forceActiveFocus();
    }

    EmojiModel {
        id: emojiModel

        filter: searchField.text
        category: picker.category
    }

    // Empty = all.
    property string category: ""

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        QQC2.TextField {
            id: searchField

            Layout.fillWidth: true
            placeholderText: KI18n.i18nc("@info:placeholder", "Search emoji…")
            onAccepted: {
                if (grid.count > 0) {
                    picker.pick(emojiModel.emojiAt(0));
                }
            }
        }

        Row {
            Layout.fillWidth: true
            spacing: 0

            QQC2.ToolButton {
                text: KI18n.i18nc("@action:button All emoji categories", "All")
                checkable: true
                checked: picker.category === ""
                autoExclusive: true
                focusPolicy: Qt.NoFocus
                onClicked: picker.category = ""
            }

            Repeater {
                model: emojiModel.categories

                QQC2.ToolButton {
                    id: categoryButton

                    required property string modelData

                    text: emojiModel.categoryEmoji(modelData)
                    font.family: "Noto Color Emoji"
                    checkable: true
                    checked: picker.category === modelData
                    autoExclusive: true
                    focusPolicy: Qt.NoFocus
                    onClicked: picker.category = modelData
                }
            }
        }

        GridView {
            id: grid

            Layout.fillWidth: true
            Layout.fillHeight: true
            cellWidth: picker.cellSize
            cellHeight: picker.cellSize
            clip: true
            model: emojiModel
            boundsBehavior: Flickable.StopAtBounds

            QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

            delegate: QQC2.AbstractButton {
                id: cell

                required property string emoji
                required property string name

                width: grid.cellWidth
                height: grid.cellHeight
                focusPolicy: Qt.NoFocus
                hoverEnabled: true
                onClicked: picker.pick(emoji)

                contentItem: Text {
                    text: cell.emoji
                    font.family: "Noto Color Emoji"
                    font.pixelSize: Kirigami.Units.gridUnit * 1.2
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Kirigami.Units.cornerRadius
                    color: Kirigami.Theme.highlightColor
                    opacity: cell.hovered || cell.down ? 0.3 : 0
                }
                QQC2.ToolTip.text: cell.name
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }

        QQC2.Label {
            Layout.alignment: Qt.AlignHCenter
            visible: grid.count === 0
            text: KI18n.i18nc("@info", "No emoji found")
            color: Kirigami.Theme.disabledTextColor
        }
    }

    function pick(emoji: string): void {
        picked(emoji);
        if (!keepOpen) {
            close();
        }
    }
}
