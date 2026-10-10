// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

// The "?" help: every shortcut that is set now, in groups. Read-only; changing a key is in
// Settings → Shortcuts.
Kirigami.Dialog {
    id: dialog

    // The MailApplication (its action collection).
    required property MailApplication application
    // The settings window opens the Shortcuts page from here.
    signal changeRequested

    // Refilled on every open, so a changed shortcut shows.
    property var groups: []

    title: KI18n.i18nc("@title:window", "Keyboard Shortcuts")
    preferredWidth: Kirigami.Units.gridUnit * 28
    preferredHeight: Kirigami.Units.gridUnit * 36
    standardButtons: Kirigami.Dialog.NoButton
    customFooterActions: [
        Kirigami.Action {
            text: KI18n.i18nc("@action:button", "Close")
            icon.name: "dialog-close"
            onTriggered: dialog.close()
        },
        Kirigami.Action {
            text: KI18n.i18nc("@action:button", "Change Shortcuts…")
            icon.name: "configure-shortcuts"
            onTriggered: {
                dialog.close();
                dialog.changeRequested();
            }
        }
    ]
    onAboutToShow: groups = application.shortcutGroups()

    // One flat list: a header row for each group, then its shortcuts.
    readonly property var rows: {
        const result = [];
        for (const group of groups) {
            result.push({header: true, text: group.title, shortcut: ""});
            for (const item of group.items) {
                result.push({header: false, text: item.text, shortcut: item.shortcut});
            }
        }
        return result;
    }

    ListView {
        id: list

        clip: true
        model: dialog.rows
        reuseItems: false

        delegate: ColumnLayout {
            id: row

            required property var modelData

            width: ListView.view.width
            spacing: 0

            Kirigami.ListSectionHeader {
                Layout.fillWidth: true
                visible: row.modelData.header
                text: row.modelData.text
            }

            RowLayout {
                Layout.fillWidth: true
                visible: !row.modelData.header
                spacing: Kirigami.Units.largeSpacing

                QQC2.Label {
                    Layout.fillWidth: true
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    Layout.bottomMargin: Kirigami.Units.smallSpacing
                    text: row.modelData.text
                    elide: Text.ElideRight
                }
                QQC2.Label {
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                    text: row.modelData.shortcut
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }
}
