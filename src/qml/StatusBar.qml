// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.coreaddons as KCoreAddons
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

// The slim bar at the bottom of the window: how each account is connected, what the app is
// doing, and the counters (unread, stored).
QQC2.Pane {
    id: bar

    // Colours of the connection dot per account state.
    function stateColor(state: string): color {
        switch (state) {
        case "connected": return Kirigami.Theme.positiveTextColor;
        case "connecting": return Kirigami.Theme.neutralTextColor;
        case "failed": return Kirigami.Theme.negativeTextColor;
        default: return Kirigami.Theme.disabledTextColor;
        }
    }

    function stateText(state: string): string {
        switch (state) {
        case "connected": return KI18n.i18nc("@info:status", "Connected");
        case "connecting": return KI18n.i18nc("@info:status", "Connecting…");
        case "failed": return KI18n.i18nc("@info:status", "Not connected");
        default: return KI18n.i18nc("@info:status", "Not started");
        }
    }

    // The middle text: what is going on, else when the mail was last looked at.
    readonly property string activity: {
        if (MailController.activityText.length > 0) {
            return MailController.activityText;
        }
        const checked = MailController.lastCheckedAt;
        return isNaN(checked) ? KI18n.i18nc("@info:status", "Up to date")
                              : KI18n.i18nc("@info:status", "Up to date · checked %1", checked.toLocaleTimeString(Qt.locale(), Locale.ShortFormat));
    }

    topPadding: 0
    bottomPadding: 0
    leftPadding: Kirigami.Units.largeSpacing
    rightPadding: Kirigami.Units.largeSpacing
    implicitHeight: Kirigami.Units.gridUnit * 1.8
    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    background: Rectangle {
        color: Kirigami.Theme.backgroundColor

        Kirigami.Separator {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        // One chip per account.
        Repeater {
            model: MailController.accounts

            delegate: QQC2.AbstractButton {
                id: chip

                required property string email
                required property string server
                required property string connectionState
                required property string statusText
                required property string pushText

                // Only a failed account can be reconnected.
                enabled: connectionState === "failed"
                hoverEnabled: true
                padding: Kirigami.Units.smallSpacing
                onClicked: MailController.reconnect(email)

                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                QQC2.ToolTip.text: {
                    let text = chip.email + "\n" + chip.server + "\n" + bar.stateText(chip.connectionState) + "\n" + chip.pushText;
                    if (chip.statusText.length > 0) {
                        text += "\n" + chip.statusText;
                    }
                    return chip.enabled ? text + "\n" + KI18n.i18nc("@info:tooltip", "Click to reconnect") : text;
                }

                background: Rectangle {
                    radius: Kirigami.Units.cornerRadius
                    color: chip.hovered && chip.enabled ? Qt.alpha(Kirigami.Theme.textColor, 0.08) : "transparent"
                }
                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Rectangle {
                        implicitWidth: Kirigami.Units.smallSpacing * 2
                        implicitHeight: implicitWidth
                        radius: width / 2
                        color: bar.stateColor(chip.connectionState)
                    }
                    QQC2.Label {
                        text: chip.email
                        font: Kirigami.Theme.smallFont
                        elide: Text.ElideRight
                        // Not greyed out: only the click is off for a healthy account.
                        opacity: 1
                        color: Kirigami.Theme.textColor
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        QQC2.Label {
            Layout.fillWidth: true
            text: bar.activity
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
        }

        QQC2.Label {
            id: lastActionLabel

            visible: text.length > 0
            text: MailController.lastActionText
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            elide: Text.ElideRight
            Layout.maximumWidth: Kirigami.Units.gridUnit * 16

            HoverHandler {
                id: lastActionHover
            }
            QQC2.ToolTip.visible: lastActionHover.hovered && lastActionLabel.truncated
            QQC2.ToolTip.text: lastActionLabel.text
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        }

        QQC2.Label {
            text: KI18n.i18np("%1 unread", "%1 unread", MailController.unreadCount)
            font: Kirigami.Theme.smallFont
            color: MailController.unreadCount > 0 ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
        }

        QQC2.Label {
            visible: MailController.storageEnabled && MailController.storageReady
            text: KI18n.i18nc("@info:status amount of mail kept on this computer", "%1 stored", KCoreAddons.Format.formatByteSize(MailController.storageUsedBytes))
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
