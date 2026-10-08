// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtWebEngine
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.Page {
    id: page

    readonly property string html: MailController.messageHtml

    function showHtml() {
        view.loadHtml(html, "about:blank");
    }

    readonly property bool hasMessage: MailController.currentUid > 0
    // "Name <address>" split for the sender block; a bare address is used as the name.
    readonly property var sender: {
        const from = MailController.messageFrom.trim();
        const match = from.match(/^"?(.*?)"?\s*<([^>]+)>$/);
        if (!match) {
            return { name: from, address: "" };
        }
        return { name: match[1].length > 0 ? match[1] : match[2], address: match[2] };
    }

    // The subject is shown large in the page itself, like Geary; the title bar stays clean.
    title: ""
    padding: 0
    onHtmlChanged: showHtml()
    Component.onCompleted: showHtml()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Subject, sender and attachments above the body.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.gridUnit * 1.5
            Layout.rightMargin: Kirigami.Units.gridUnit * 1.5
            Layout.topMargin: Kirigami.Units.gridUnit
            Layout.bottomMargin: Kirigami.Units.gridUnit
            visible: page.hasMessage
            spacing: Kirigami.Units.gridUnit * 0.75

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 1
                font.weight: Font.Bold
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: MailController.messageSubject.length > 0 ? MailController.messageSubject
                                                               : KI18n.i18nc("@title", "(no subject)")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.largeSpacing

                SenderAvatar {
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Kirigami.Units.gridUnit * 2.5
                    name: page.sender.name
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true

                        QQC2.Label {
                            text: page.sender.name
                            textFormat: Text.PlainText
                            font.weight: Font.DemiBold
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: page.sender.address
                            textFormat: Text.PlainText
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            text: isNaN(MailController.messageDate) ? ""
                                : MailController.messageDate.toLocaleString(Qt.locale(), Locale.ShortFormat)
                            color: Kirigami.Theme.disabledTextColor
                            font: Kirigami.Theme.smallFont
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: MailController.messageTo.length > 0
                        text: KI18n.i18nc("@label recipients of a message", "To: %1", MailController.messageTo)
                        textFormat: Text.PlainText
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                        elide: Text.ElideRight
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                visible: MailController.messageAttachments.length > 0
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: MailController.messageAttachments

                    delegate: Kirigami.Chip {
                        required property string modelData

                        text: modelData
                        icon.name: "mail-attachment"
                        closable: false
                        checkable: false
                        hoverEnabled: false
                        focusPolicy: Qt.NoFocus
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: page.hasMessage
            opacity: 0.5
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Mail is untrusted: no scripts, no remote images, and the CSP from the renderer
            // blocks all other remote loads. Links open in the user's browser instead.
            WebEngineView {
                id: view

                anchors.fill: parent
                visible: page.hasMessage
                backgroundColor: Kirigami.Theme.backgroundColor
                settings.javascriptEnabled: false
                settings.autoLoadImages: false
                settings.localContentCanAccessRemoteUrls: false
                settings.localContentCanAccessFileUrls: false

                onNavigationRequested: request => {
                    const scheme = request.url.toString().split(":")[0];
                    if (scheme === "about" || scheme === "data") {
                        return; // our own loadHtml()
                    }
                    request.reject();
                    if (request.navigationType === WebEngineNavigationRequest.LinkClickedNavigation) {
                        Qt.openUrlExternally(request.url);
                    }
                }
                onNewWindowRequested: request => Qt.openUrlExternally(request.requestedUrl)
                // Mail pages log every blocked remote image/font; blocking is intended, so stay quiet.
                onJavaScriptConsoleMessage: (level, message, lineNumber, sourceID) => {}
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: !page.hasMessage
                icon.name: "mail-message"
                text: KI18n.i18nc("@info:placeholder", "No message selected")
            }

            QQC2.BusyIndicator {
                anchors.centerIn: parent
                visible: page.html.length === 0 && MailController.busy
                running: visible
            }
        }
    }
}
