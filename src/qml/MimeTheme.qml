// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton

import QtQuick

// Colours of the three columns, after Mimestream: a lighter purple-grey sidebar, a deep purple
// message list and a neutral reading pane, with one blue accent. Follows the system light/dark.
QtObject {
    readonly property bool dark: Qt.styleHints.colorScheme !== Qt.Light

    readonly property color sidebarBackground: dark ? "#2b2833" : "#e8e6ee"
    readonly property color listBackground: dark ? "#1e1c27" : "#ffffff"
    readonly property color readingBackground: dark ? "#1c1c1e" : "#ffffff"

    readonly property color text: dark ? "#ffffff" : "#1c1b22"
    readonly property color secondaryText: dark ? "#a3a1ad" : "#6b6975"
    readonly property color accent: dark ? "#3d8bff" : "#0a60d6"
    // Link text in the message body; WebEngine gets it through MailController.
    readonly property color link: dark ? "#6ea8ff" : "#0a60d6"

    // Selected and hovered rows.
    readonly property color sidebarSelected: dark ? "#4a4758" : "#d4d1df"
    readonly property color listSelected: dark ? "#3b3948" : "#dcdaea"
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
    readonly property color divider: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
}
