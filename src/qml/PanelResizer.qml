// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami

// A thin vertical divider on the right edge of a panel; dragging it changes the panel's width.
// While dragging, `liveWidth` follows the pointer (use it for the panel's width when `dragging`);
// `resized(width)` is emitted when the drag ends, `resetRequested()` on a double click.
Item {
    id: resizer

    // The panel's width when no drag is going on, and the limits.
    required property int panelWidth
    required property int minimum
    required property int maximum

    readonly property bool dragging: drag.active
    // The width the panel should have while dragging.
    readonly property int liveWidth: Math.max(minimum, Math.min(maximum, Math.round(startWidth + dragDelta)))

    property real startWidth: panelWidth
    property real dragDelta: 0

    signal resized(int width)
    signal resetRequested

    implicitWidth: Kirigami.Units.smallSpacing * 2
    // Sits over the panel's edge, above its content.
    z: 100

    // The visible line: faint at rest, accent coloured on hover and while dragging.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: resizer.dragging || hover.hovered ? 3 : 1
        height: parent.height
        color: resizer.dragging || hover.hovered ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
        opacity: resizer.dragging || hover.hovered ? 0.8 : 0
    }

    HoverHandler {
        id: hover

        cursorShape: Qt.SplitHCursor
    }

    DragHandler {
        id: drag

        target: null
        xAxis.enabled: true
        yAxis.enabled: false
        cursorShape: Qt.SplitHCursor
        onActiveChanged: {
            if (active) {
                resizer.startWidth = resizer.panelWidth;
                resizer.dragDelta = 0;
            } else {
                resizer.resized(resizer.liveWidth);
            }
        }
        // The pointer's movement on screen: the divider itself moves with the panel's width.
        onCentroidChanged: {
            if (active) {
                resizer.dragDelta = centroid.scenePosition.x - centroid.scenePressPosition.x;
            }
        }
    }

    TapHandler {
        onDoubleTapped: resizer.resetRequested()
    }
}
