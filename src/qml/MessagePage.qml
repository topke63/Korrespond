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

    title: MailController.messageSubject.length > 0 ? MailController.messageSubject
                                                    : KI18n.i18nc("@title", "(no subject)")
    padding: 0
    onHtmlChanged: showHtml()
    Component.onCompleted: showHtml()

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Kirigami.FormLayout {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            visible: MailController.currentUid > 0

            QQC2.Label {
                Kirigami.FormData.label: KI18n.i18nc("@label", "From:")
                Layout.fillWidth: true
                text: MailController.messageFrom
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
            QQC2.Label {
                Kirigami.FormData.label: KI18n.i18nc("@label", "To:")
                Layout.fillWidth: true
                text: MailController.messageTo
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
            QQC2.Label {
                Kirigami.FormData.label: KI18n.i18nc("@label", "Date:")
                text: isNaN(MailController.messageDate) ? ""
                    : MailController.messageDate.toLocaleString(Qt.locale(), Locale.ShortFormat)
            }
            Flow {
                Kirigami.FormData.label: KI18n.i18nc("@label", "Attachments:")
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
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Mail is untrusted: no scripts, no remote images, and the CSP from the renderer
            // blocks all other remote loads. Links open in the user's browser instead.
            WebEngineView {
                id: view

                anchors.fill: parent
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
            }

            QQC2.BusyIndicator {
                anchors.centerIn: parent
                visible: page.html.length === 0 && MailController.busy
                running: visible
            }
        }
    }
}
