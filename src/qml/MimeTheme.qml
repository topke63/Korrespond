// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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

    // WCAG 2 relative luminance of a colour.
    function luminance(colour) {
        const c = Qt.color(colour);
        const lin = v => v <= 0.03928 ? v / 12.92 : Math.pow((v + 0.055) / 1.055, 2.4);
        return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
    }

    // WCAG 2 contrast ratio of two colours, 1 to 21.
    function contrast(a, b) {
        const la = luminance(a);
        const lb = luminance(b);
        return (Math.max(la, lb) + 0.05) / (Math.min(la, lb) + 0.05);
    }

    // `fg` moved in lightness (darker on a light background, lighter on a dark one) until it
    // reaches `ratio` on `bg`; `fg` itself when it already does.
    function readableOn(fg, bg, ratio) {
        const c = Qt.color(fg);
        if (contrast(c, bg) >= ratio) {
            return c;
        }
        const lighter = luminance(bg) < 0.18;
        for (let step = 1; step <= 20; ++step) {
            const l = Math.min(1, Math.max(0, c.hslLightness + (lighter ? 1 : -1) * step * 0.05));
            const moved = Qt.hsla(c.hslHue, c.hslSaturation, l, c.a);
            if (contrast(moved, bg) >= ratio) {
                return moved;
            }
        }
        return lighter ? Qt.hsla(c.hslHue, c.hslSaturation, 1, c.a) : Qt.hsla(c.hslHue, c.hslSaturation, 0, c.a);
    }
}
