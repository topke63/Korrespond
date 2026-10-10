// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton

import QtQuick

// Colours of the three columns, after Mimestream: a lighter purple-grey sidebar, a deep purple
// message list and a neutral reading pane, with one blue accent. Follows the system light/dark.
QtObject {
    readonly property bool dark: Qt.styleHints.colorScheme !== Qt.Light

    readonly property color sidebarBackground: dark ? "#4c4951" : "#e8e6ee"
    readonly property color listBackground: dark ? "#1e1e1e" : "#ffffff"
    readonly property color readingBackground: dark ? "#1e1e1e" : "#ffffff"
    // Title bar band over the list and reading pane.
    readonly property color headerBackground: dark ? "#251e2c" : "#f3f1f6"

    readonly property color text: dark ? "#ececec" : "#1c1b22"
    readonly property color secondaryText: dark ? "#a4a4a4" : "#6e6e73"
    // Sidebar section headings and the message preview line.
    readonly property color sectionText: dark ? "#8c8991" : "#8a8790"
    readonly property color previewText: dark ? "#aaaaaa" : "#6e6e73"
    readonly property color accent: dark ? "#1866c5" : "#0a60d6"
    // Link text in the message body; WebEngine gets it through MailController.
    readonly property color link: dark ? "#5ea2ff" : "#0a60d6"

    // Tints of the sidebar folder icons and the toolbar icons.
    readonly property color iconTint: dark ? "#cfc8ba" : "#5c5a66"
    readonly property color toolbarIcon: dark ? "#9f99a2" : "#6e6e73"

    // Selected and hovered rows.
    readonly property color sidebarSelected: dark ? "#68656b" : "#d4d1df"
    readonly property color listSelected: dark ? "#464646" : "#dcdcdc"
    readonly property color hover: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
    readonly property color divider: dark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.08)
    // Hairline between the three columns.
    readonly property color columnDivider: dark ? "#050505" : "#d0d0d4"
}
