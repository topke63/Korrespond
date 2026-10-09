// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQml
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
    // The open message has arrived (until then only the busy indicator shows).
    readonly property bool loaded: hasMessage && (html.length > 0 || MailController.messageFrom.length > 0
                                                  || MailController.messageSubject.length > 0)
    // "Name <address>" split for the sender block; a bare address is used as the name.
    readonly property var sender: {
        const from = MailController.messageFrom.trim();
        const match = from.match(/^"?(.*?)"?\s*<([^>]+)>$/);
        if (!match) {
            return { name: from, address: "" };
        }
        return { name: match[1].length > 0 ? match[1] : match[2], address: match[2] };
    }

    // The subject is shown large in the page itself; the title bar holds the message actions.
    title: ""
    // Mimestream-style action bar. Reply, Reply All, Forward and Label are still shown for the
    // look only and stay disabled until their tasks are done.
    titleDelegate: RowLayout {
        Layout.fillWidth: true
        spacing: 0

        Repeater {
            model: [
                { move: "archive" },
                { move: "delete" },
                { unread: true },
                { flag: true },
                { move: "folder" },
                { icon: "tag", text: KI18n.i18nc("@action:button", "Label") },
                { separator: true },
                { icon: "mail-reply-sender", text: KI18n.i18nc("@action:button", "Reply") },
                { icon: "mail-reply-all", text: KI18n.i18nc("@action:button", "Reply All") },
                { icon: "mail-forward", text: KI18n.i18nc("@action:button", "Forward") }
            ]

            delegate: Loader {
                required property var modelData

                visible: page.hasMessage
                sourceComponent: modelData.separator ? separatorComponent
                               : modelData.flag ? flagComponent
                               : modelData.unread ? unreadComponent
                               : modelData.move === "archive" ? archiveComponent
                               : modelData.move === "delete" ? deleteComponent
                               : modelData.move === "folder" ? moveComponent : buttonComponent

                // Flag / unflag the open message.
                Component {
                    id: flagComponent

                    QQC2.ToolButton {
                        icon.name: "flag"
                        icon.color: MailController.messageFlagged ? "#f67400" : Kirigami.Theme.textColor
                        text: MailController.messageFlagged ? KI18n.i18nc("@action:button", "Unflag")
                                                            : KI18n.i18nc("@action:button", "Flag")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: MailController.setFlagged(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageFlagged)

                        QQC2.ToolTip.text: text + " (Ctrl+Shift+F)"
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                // Toggles the open message between read and unread.
                Component {
                    id: unreadComponent

                    QQC2.ToolButton {
                        icon.name: MailController.messageUnread ? "mail-mark-read" : "mail-mark-unread"
                        text: MailController.messageUnread ? KI18n.i18nc("@action:button", "Mark as Read")
                                                           : KI18n.i18nc("@action:button", "Mark as Unread")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: MailController.setUnread(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageUnread)

                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                // Archive and Delete move the open message on the server (never for good).
                Component {
                    id: archiveComponent

                    QQC2.ToolButton {
                        icon.name: "archive-insert"
                        text: KI18n.i18nc("@action:button", "Archive")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canArchive
                        onClicked: MailController.archiveMessage()

                        QQC2.ToolTip.text: enabled ? text : KI18n.i18nc("@info:tooltip", "Archive is not available: this account has no archive folder, or the message is already there.")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                Component {
                    id: deleteComponent

                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        text: KI18n.i18nc("@action:button", "Delete")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canDelete
                        onClicked: MailController.deleteMessage()

                        QQC2.ToolTip.text: enabled ? KI18n.i18nc("@info:tooltip", "Move to Trash") : KI18n.i18nc("@info:tooltip", "Delete is not available: this account has no Trash folder, or the message is already there.")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                // A menu of the account's folders; picking one moves the open message there.
                Component {
                    id: moveComponent

                    QQC2.ToolButton {
                        id: moveButton

                        icon.name: "folder"
                        text: KI18n.i18nc("@action:button", "Move to Folder")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canMove
                        down: moveMenu.visible
                        onClicked: moveMenu.popup(this, 0, height)

                        QQC2.ToolTip.text: text
                        QQC2.ToolTip.visible: hovered && !moveMenu.visible
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        QQC2.Menu {
                            id: moveMenu

                            Instantiator {
                                model: MailController.messageFolderModel

                                delegate: QQC2.MenuItem {
                                    required property var model

                                    text: " ".repeat(model.depth * 2) + model.name
                                    icon.name: model.iconName
                                    enabled: model.path !== MailController.currentMessageFolder
                                    onTriggered: MailController.moveMessage(model.path)
                                }
                                onObjectAdded: (index, object) => moveMenu.insertItem(index, object)
                                onObjectRemoved: (index, object) => moveMenu.removeItem(object)
                            }
                        }
                    }
                }

                Component {
                    id: buttonComponent

                    QQC2.ToolButton {
                        icon.name: modelData.icon
                        text: modelData.text
                        display: QQC2.AbstractButton.IconOnly
                        enabled: false // not implemented in milestone 1
                    }
                }
                Component {
                    id: separatorComponent

                    Kirigami.Separator {
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                        Layout.alignment: Qt.AlignVCenter
                        anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Kirigami.SearchField {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 14
            Layout.rightMargin: Kirigami.Units.smallSpacing
            enabled: false // search comes in a later milestone
        }
    }
    padding: 0
    onHtmlChanged: showHtml()

    Shortcut {
        sequence: "Ctrl+Shift+F"
        enabled: page.hasMessage
        onActivated: MailController.setFlagged(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageFlagged)
    }
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
            visible: page.loaded
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
            visible: page.loaded
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
                visible: page.loaded
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
