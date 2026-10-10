// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton

import QtQuick

// Colours of the three columns, after Mimestream: a lighter purple-grey sidebar, a deep purple
// message list and a neutral reading pane, with one blue accent. Follows the system light/dark.
QtObject {
    // From the application palette (not Qt.styleHints.colorScheme, which the KDE platform theme
    // ignores when PLASMAMAIL_COLOR_SCHEME forces a scheme); a dark window means a dark scheme.
    readonly property SystemPalette systemPalette: SystemPalette {}
    readonly property bool dark: systemPalette.window.hslLightness < 0.5

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

    // Label tag colours (tag icons), after Mimestream; the light variants are a little deeper to stay
    // visible on white. Labels do not exist in the app yet (PLAN open question 1), so nothing uses these.
    readonly property var labelColours: dark ? {
        "Awaiting": "#f78671",
        "Delegated": "#ffbd66",
        "Follow Up": "#f8d075",
        "Home": "#2ecb94",
        "Inquiries": "#a9c6e6",
        "Travel": "#bba3fd",
        "Website": "#bebbc1"
    } : {
        "Awaiting": "#e8624a",
        "Delegated": "#f0962a",
        "Follow Up": "#e0b030",
        "Home": "#1ba26b",
        "Inquiries": "#6f9fd0",
        "Travel": "#8f72e8",
        "Website": "#9a97a0"
    }
    // Chip backgrounds where they differ from the tag colour (the same in both schemes).
    readonly property var chipBackgrounds: {
        "Home": "#1ba26b",
        "Inquiries": "#b4d1f8",
        "Travel": "#b69dfe"
    }

    // A stable chip background for any label name: a known name keeps its colour, any other name
    // hashes into the palette, so it looks the same every time.
    function chipColour(name) {
        const key = String(name);
        if (chipBackgrounds[key] !== undefined) {
            return chipBackgrounds[key];
        }
        if (labelColours[key] !== undefined) {
            return labelColours[key];
        }
        const names = Object.keys(labelColours);
        let hash = 0;
        for (let i = 0; i < key.length; ++i) {
            hash = (hash * 31 + key.charCodeAt(i)) >>> 0;
        }
        return chipBackgrounds[names[hash % names.length]] ?? labelColours[names[hash % names.length]];
    }

    // White on a dark chip, dark on a light one (by perceived luminance).
    function chipTextColour(background) {
        const c = Qt.color(background);
        const luminance = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
        return luminance > 0.6 ? "#1c1b22" : "#ffffff";
    }
}
