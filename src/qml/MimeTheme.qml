// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma Singleton

import QtQuick

// What the colour scheme does not supply: alphas for tinting the scheme's own colours (selection,
// hover, dividers) and the label chip colours. Every other colour comes from Kirigami.Theme and
// follows the Plasma colour scheme.
QtObject {
    // Selected row: the set's highlight colour at this alpha.
    readonly property real selectedAlpha: 0.3
    // Hovered row: the set's text colour at this alpha.
    readonly property real hoverAlpha: 0.06
    // Row and header dividers; 0.08 gave 1.19:1 on white, under the 1.2:1 floor.
    readonly property real dividerAlpha: 0.10
    // Hairline between the three columns.
    readonly property real columnDividerAlpha: 0.2

    // Label tag colours (tag icons), after Mimestream, deep enough to stay visible on white and on
    // dark. Labels do not exist in the app yet (PLAN open question 1), so nothing uses these.
    readonly property var labelColours: {
        "Awaiting": "#e8624a",
        "Delegated": "#f0962a",
        "Follow Up": "#e0b030",
        "Home": "#1ba26b",
        "Inquiries": "#6f9fd0",
        "Travel": "#8f72e8",
        "Website": "#9a97a0"
    }
    // Chip backgrounds where they differ from the tag colour.
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
