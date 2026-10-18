// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import Korrespond

// A window that shows one attachment. `info` is what MailController.previewAttachment() returned:
// {name, mimeType, size, kind, fileUrl, text, truncated}. The window keeps its own copy, so it does
// not depend on which message is open. Kinds without a view show the name, size and type with Open
// and Save As only.
Kirigami.ApplicationWindow {
    id: win

    property var info: ({})
    readonly property string kind: info.kind ?? "none"

    title: info.name ?? KI18n.i18nc("@title:window", "Attachment")
    width: Kirigami.Units.gridUnit * 44
    height: Kirigami.Units.gridUnit * 32
    minimumWidth: Kirigami.Units.gridUnit * 24
    minimumHeight: Kirigami.Units.gridUnit * 16
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    function sizeText(): string {
        return Qt.locale().formattedDataSize(info.size ?? 0, 1);
    }

    function saveAs(): void {
        const folder = String(StandardPaths.writableLocation(StandardPaths.DownloadLocation)).replace(/\/+$/, "");
        saveDialog.currentFolder = folder;
        // Encoded, so '#', '?' or '%' in the name stay part of the file name.
        saveDialog.selectedFile = folder + "/" + encodeURIComponent(info.name ?? "attachment");
        saveDialog.open();
    }

    FileDialog {
        id: saveDialog

        fileMode: FileDialog.SaveFile
        onAccepted: MailController.savePreviewFileAs(win.info.fileUrl, selectedFile)
    }

    Shortcut {
        sequences: [StandardKey.Cancel, StandardKey.Close]
        onActivated: win.close()
    }

    pageStack.initialPage: Kirigami.Page {
        padding: 0

        header: QQC2.ToolBar {
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    source: "mail-attachment"
                    implicitWidth: Kirigami.Units.iconSizes.medium
                    implicitHeight: implicitWidth
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: win.info.name ?? ""
                        font.bold: true
                        elide: Text.ElideMiddle
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        text: win.sizeText() + " · " + (win.info.mimeType ?? "")
                        color: Kirigami.Theme.disabledTextColor
                        font: Kirigami.Theme.smallFont
                        elide: Text.ElideRight
                    }
                }
                QQC2.Button {
                    icon.name: "document-open"
                    text: KI18n.i18nc("@action:button", "Open")
                    onClicked: MailController.openPreviewFile(win.info.fileUrl)
                }
                QQC2.Button {
                    icon.name: "document-save-as"
                    text: KI18n.i18nc("@action:button", "Save As…")
                    onClicked: win.saveAs()
                }
            }
        }

        // The view for the kind; the others are added by their own tasks, and every kind without a
        // view (or none at all) falls back to the placeholder.
        contentItem: Loader {
            sourceComponent: win.kind === "image" ? imageView : placeholder
        }
    }

    // Images: fit to the window by default; + / - / 0 (or the buttons, or Ctrl+wheel) zoom, 1 shows
    // actual size.
    Component {
        id: imageView

        Item {
            id: viewer

            property bool fit: true
            property real zoom: 1
            readonly property real fitZoom: image.implicitWidth > 0 && image.implicitHeight > 0
                ? Math.min(flick.width / image.implicitWidth, flick.height / image.implicitHeight, 1) : 1
            readonly property real shown: fit ? fitZoom : zoom

            function zoomBy(factor: real): void {
                zoom = Math.max(0.05, Math.min(16, shown * factor));
                fit = false;
            }

            Flickable {
                id: flick

                anchors.fill: parent
                clip: true
                contentWidth: Math.max(width, image.width)
                contentHeight: Math.max(height, image.height)
                boundsBehavior: Flickable.StopAtBounds
                QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
                QQC2.ScrollBar.horizontal: QQC2.ScrollBar {}

                Image {
                    id: image

                    x: Math.max(0, (flick.width - width) / 2)
                    y: Math.max(0, (flick.height - height) / 2)
                    width: implicitWidth * viewer.shown
                    height: implicitHeight * viewer.shown
                    source: win.info.fileUrl
                    asynchronous: true
                    cache: false
                    smooth: true
                    mipmap: true
                    visible: status === Image.Ready
                }

                WheelHandler {
                    acceptedModifiers: Qt.ControlModifier
                    onWheel: event => viewer.zoomBy(event.angleDelta.y > 0 ? 1.15 : 1 / 1.15)
                }
            }

            QQC2.BusyIndicator {
                anchors.centerIn: parent
                running: image.status === Image.Loading
                visible: running
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: image.status === Image.Error
                text: KI18n.i18nc("@info", "This image cannot be shown")
                explanation: KI18n.i18nc("@info", "The file may be damaged or in a format that is not supported. Use Open or Save As.")
                icon.name: "image-missing"
            }

            Shortcut {
                sequence: "+"
                onActivated: viewer.zoomBy(1.25)
            }
            Shortcut {
                sequence: "="
                onActivated: viewer.zoomBy(1.25)
            }
            Shortcut {
                sequence: "-"
                onActivated: viewer.zoomBy(0.8)
            }
            Shortcut {
                sequence: "0"
                onActivated: viewer.fit = true
            }
            Shortcut {
                sequence: "1"
                onActivated: {
                    viewer.zoom = 1;
                    viewer.fit = false;
                }
            }

            QQC2.Pane {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: Kirigami.Units.largeSpacing
                visible: image.status === Image.Ready
                padding: Kirigami.Units.smallSpacing
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                background: Rectangle {
                    color: Kirigami.Theme.backgroundColor
                    opacity: 0.9
                    radius: Kirigami.Units.cornerRadius
                    border.color: Kirigami.Theme.disabledTextColor
                    border.width: 1
                }

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.ToolButton {
                        icon.name: "zoom-out"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Zoom Out")
                        onClicked: viewer.zoomBy(0.8)
                    }
                    QQC2.Label {
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                        horizontalAlignment: Text.AlignHCenter
                        text: Math.round(viewer.shown * 100) + " %"
                    }
                    QQC2.ToolButton {
                        icon.name: "zoom-in"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Zoom In")
                        onClicked: viewer.zoomBy(1.25)
                    }
                    QQC2.ToolButton {
                        icon.name: "zoom-fit-best"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Fit to Window")
                        checked: viewer.fit
                        onClicked: viewer.fit = true
                    }
                    QQC2.ToolButton {
                        icon.name: "zoom-original"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Actual Size")
                        onClicked: {
                            viewer.zoom = 1;
                            viewer.fit = false;
                        }
                    }
                }
            }
        }
    }

    Component {
        id: placeholder

        Kirigami.PlaceholderMessage {
            text: KI18n.i18nc("@info", "No preview for this type of file")
            explanation: KI18n.i18nc("@info", "Use Open to view it in another program, or Save As to keep a copy.")
            icon.name: "document-preview"
        }
    }
}
