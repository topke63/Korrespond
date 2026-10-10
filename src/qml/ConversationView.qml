// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtWebEngine
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

// The reading pane for a conversation: its messages stacked, oldest first. Unread messages and the
// newest are open (header, attachments, body); the others are one line that opens on a click. Each
// open message has its own web view, sized to its content, so the list scrolls as one page.
ListView {
    id: list

    // Row of the message, and "reply", "replyAll" or "forward".
    signal composeRequested(int row, string kind)
    // Row of the message and the index of its attachment.
    signal attachmentRequested(int row, int index)

    model: MailController.conversationModel
    clip: true
    spacing: 0
    boundsBehavior: Flickable.StopAtBounds

    QQC2.ScrollBar.vertical: QQC2.ScrollBar {}

    header: Kirigami.Heading {
        width: list.width
        topPadding: Kirigami.Units.gridUnit
        bottomPadding: Kirigami.Units.gridUnit * 0.75
        leftPadding: Kirigami.Units.gridUnit * 1.5
        rightPadding: Kirigami.Units.gridUnit * 1.5
        level: 1
        font.weight: Font.Bold
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        text: MailController.messageSubject.length > 0 ? MailController.messageSubject : KI18n.i18nc("@title", "(no subject)")
    }

    delegate: ColumnLayout {
        id: card

        required property int index
        required property string account
        required property var uid
        required property string from
        required property string fromName
        required property string dateText
        required property string preview
        required property bool unread
        required property bool flagged
        required property bool expanded
        required property bool loaded
        required property string html
        required property bool hasRemoteImages
        required property bool imagesLoaded
        required property var attachments

        readonly property string address: {
            const match = from.match(/<([^>]+)>\s*$/);
            return match ? match[1] : "";
        }

        width: list.width
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Qt.alpha(Kirigami.Theme.textColor, MimeTheme.dividerAlpha)
        }

        // The message's line: sender, date, and the start of the text while it is closed.
        QQC2.ItemDelegate {
            id: head

            Layout.fillWidth: true
            leftPadding: Kirigami.Units.gridUnit * 1.5
            rightPadding: Kirigami.Units.gridUnit * 1.5
            topPadding: Kirigami.Units.smallSpacing * 2
            bottomPadding: Kirigami.Units.smallSpacing * 2
            onClicked: MailController.setConversationItemExpanded(card.index, !card.expanded)

            background: Rectangle {
                color: head.hovered ? Qt.alpha(Kirigami.Theme.textColor, MimeTheme.hoverAlpha) : "transparent"
            }

            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Rectangle {
                    implicitWidth: 10
                    implicitHeight: 10
                    radius: width / 2
                    color: MimeTheme.readableOn(activePalette.highlight, Kirigami.Theme.backgroundColor, 3)
                    opacity: card.unread ? 1 : 0
                }

                SenderAvatar {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.gridUnit * 2
                    name: card.fromName
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.Label {
                            text: card.fromName
                            textFormat: Text.PlainText
                            font.weight: Font.Bold
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            visible: card.expanded
                            text: card.address
                            textFormat: Text.PlainText
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                        }
                        Item {
                            Layout.fillWidth: true
                            visible: !card.expanded
                        }
                        Kirigami.Icon {
                            visible: card.flagged
                            source: "flag"
                            color: Kirigami.Theme.neutralTextColor
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: implicitWidth
                        }
                        QQC2.Label {
                            text: card.dateText
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: !card.expanded && card.preview.length > 0
                        text: card.preview
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }

        // The open message.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
            Layout.rightMargin: Kirigami.Units.gridUnit * 1.5
            Layout.bottomMargin: Kirigami.Units.gridUnit
            visible: card.expanded
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                spacing: 0

                Repeater {
                    model: [
                        { icon: "mail-reply-sender", kind: "reply", text: KI18n.i18nc("@action:button", "Reply") },
                        { icon: "mail-reply-all", kind: "replyAll", text: KI18n.i18nc("@action:button", "Reply All") },
                        { icon: "mail-forward", kind: "forward", text: KI18n.i18nc("@action:button", "Forward") }
                    ]

                    delegate: QQC2.ToolButton {
                        required property var modelData

                        icon.name: modelData.icon
                        text: modelData.text
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canCompose
                        onClicked: list.composeRequested(card.index, modelData.kind)

                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                visible: card.attachments.length > 0
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: card.attachments

                    delegate: Kirigami.Chip {
                        id: chip

                        required property int index
                        required property string modelData

                        text: modelData
                        icon.name: "mail-attachment"
                        closable: false
                        checkable: false
                        onClicked: list.attachmentRequested(card.index, chip.index)
                    }
                }
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Information
                visible: card.loaded && card.hasRemoteImages && !card.imagesLoaded
                text: KI18n.i18nc("@info", "This message has images that were not loaded.")
                actions: Kirigami.Action {
                    text: KI18n.i18nc("@action:button", "Load images")
                    icon.name: "view-preview"
                    onTriggered: MailController.loadConversationImages(card.index)
                }
            }

            QQC2.BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: !card.loaded
                running: visible
            }

            Loader {
                Layout.fillWidth: true
                Layout.preferredHeight: item ? item.contentHeight : 0
                active: card.expanded && card.loaded && card.html.length > 0

                sourceComponent: WebEngineView {
                    id: web

                    // The page grows to its content: the list, not this view, scrolls.
                    readonly property real contentHeight: Math.max(1, contentsSize.height)

                    backgroundColor: Kirigami.Theme.backgroundColor
                    settings.javascriptEnabled: false
                    settings.autoLoadImages: card.imagesLoaded
                    settings.localContentCanAccessRemoteUrls: false
                    settings.localContentCanAccessFileUrls: false
                    settings.showScrollBars: false

                    Component.onCompleted: loadHtml(card.html, "about:blank")
                    Connections {
                        target: card

                        function onHtmlChanged() {
                            web.settings.autoLoadImages = card.imagesLoaded;
                            web.loadHtml(card.html, "about:blank");
                        }
                    }

                    // The wheel scrolls the conversation.
                    WheelHandler {
                        onWheel: event => {
                            const maxY = Math.max(list.originY, list.originY + list.contentHeight - list.height);
                            list.contentY = Math.max(list.originY, Math.min(maxY, list.contentY - event.angleDelta.y / 2));
                        }
                    }

                    onNavigationRequested: request => {
                        const scheme = request.url.toString().split(":")[0];
                        if (scheme === "about" || scheme === "data") {
                            return;
                        }
                        request.reject();
                        if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation) {
                            Qt.openUrlExternally(request.url);
                        }
                    }
                    onNewWindowRequested: request => Qt.openUrlExternally(request.requestedUrl)
                    onJavaScriptConsoleMessage: (level, message, lineNumber, sourceID) => {}
                }
            }
        }
    }

    SystemPalette {
        id: activePalette

        colorGroup: SystemPalette.Active
    }
}
