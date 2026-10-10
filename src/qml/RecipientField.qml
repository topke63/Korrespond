// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import Korrespond

// An address field with suggestions from the contacts cache. The text after the last comma is the
// part being typed: Down/Up move through the suggestions, Enter or Tab take the marked one, a
// click takes any, Escape closes the list. Nothing is replaced unless one is taken; with no
// suggestion marked, Enter and Tab behave as always.
QQC2.TextField {
    id: field

    // The part being typed (after the last comma, trimmed).
    readonly property string token: {
        const at = text.lastIndexOf(",");
        return text.substring(at + 1).trim();
    }
    property var suggestions: []
    // The marked suggestion; -1 until Down is pressed.
    property int marked: -1
    readonly property bool listOpen: popup.visible

    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase

    function refresh() {
        marked = -1;
        suggestions = activeFocus && token.length > 0 ? MailController.contactSuggestions(token) : [];
        if (suggestions.length > 0) {
            popup.open();
        } else {
            popup.close();
        }
    }

    // Puts suggestion `index` in place of the part being typed and goes on with a comma.
    function accept(index) {
        const item = suggestions[index];
        if (!item) {
            return;
        }
        const at = text.lastIndexOf(",");
        const head = at >= 0 ? text.substring(0, at + 1) + " " : "";
        text = head + item.display + ", ";
        cursorPosition = text.length;
        suggestions = [];
        marked = -1;
        popup.close();
    }

    onTextEdited: refresh()
    onActiveFocusChanged: {
        if (!activeFocus) {
            popup.close();
        }
    }

    Keys.onPressed: event => {
        if (!popup.visible) {
            return;
        }
        if (event.key === Qt.Key_Down) {
            marked = Math.min(suggestions.length - 1, marked + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Up) {
            marked = Math.max(-1, marked - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            popup.close();
            event.accepted = true;
        } else if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Tab) && marked >= 0) {
            accept(marked);
            event.accepted = true;
        }
    }

    QQC2.Popup {
        id: popup

        y: field.height
        width: field.width
        padding: 0
        focus: false // the field keeps the keyboard
        closePolicy: QQC2.Popup.NoAutoClose

        contentItem: ListView {
            implicitHeight: Math.min(contentHeight, Kirigami.Units.gridUnit * 12)
            clip: true
            model: field.suggestions
            interactive: false

            delegate: QQC2.ItemDelegate {
                id: row

                required property int index
                required property var modelData

                width: ListView.view.width
                highlighted: row.index === field.marked
                onClicked: field.accept(row.index)

                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: row.modelData.name.length > 0 ? row.modelData.name : row.modelData.email
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                    }
                    QQC2.Label {
                        visible: row.modelData.name.length > 0
                        text: row.modelData.email
                        textFormat: Text.PlainText
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
