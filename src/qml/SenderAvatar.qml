// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigamiaddons.components as Components

// Mailspring-style avatar: a solid circle in a color derived from the name, white initials.
Rectangle {
    id: avatar

    property string name

    implicitWidth: Kirigami.Units.iconSizes.medium
    implicitHeight: implicitWidth
    radius: width / 2
    color: Components.NameUtils.colorsFromString(name)

    Text {
        anchors.centerIn: parent
        // One letter reads cleaner than two at small sizes.
        text: Components.NameUtils.initialsFromString(avatar.name).charAt(0).toUpperCase()
        color: "white"
        font.pixelSize: Math.round(avatar.height * 0.45)
        font.weight: Font.DemiBold
    }
}
