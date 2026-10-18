// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import QtMultimedia
import QtQuick.Pdf
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
            sourceComponent: win.kind === "image" ? imageView : win.kind === "pdf" ? pdfView : win.kind === "text" ? textView : (win.kind === "audio" || win.kind === "video") ? mediaView : placeholder
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

    // PDF: all pages one below the other; the page count and zoom are in a floating bar. A file
    // that needs a password, or cannot be read, says so (Open and Save As stay in the header).
    Component {
        id: pdfView

        Item {
            id: viewer

            property bool passwordNeeded: false

            PdfDocument {
                id: pdf

                source: win.info.fileUrl
                onPasswordRequired: viewer.passwordNeeded = true
            }

            PdfMultiPageView {
                id: pages

                anchors.fill: parent
                document: pdf
                visible: pdf.status === PdfDocument.Ready
                // Fit the width of the window until the user zooms.
                onWidthChanged: if (viewer.fitWidth) viewer.fit()
            }

            property bool fitWidth: true

            // The widest page fits the window, with room for the scroll bar and the page margins.
            function fit(): void {
                if (pdf.status !== PdfDocument.Ready || pdf.pageCount === 0 || pages.width <= 0) {
                    return;
                }
                let widest = 1;
                for (let i = 0; i < pdf.pageCount; ++i) {
                    widest = Math.max(widest, pdf.pagePointSize(i).width);
                }
                pages.renderScale = Math.max(0.2, (pages.width - Kirigami.Units.gridUnit * 3) / widest);
            }

            Connections {
                target: pdf

                function onStatusChanged() {
                    if (viewer.fitWidth) {
                        viewer.fit();
                    }
                }
            }

            QQC2.BusyIndicator {
                anchors.centerIn: parent
                running: pdf.status === PdfDocument.Loading
                visible: running
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: pdf.status === PdfDocument.Error || viewer.passwordNeeded
                text: viewer.passwordNeeded ? KI18n.i18nc("@info", "This PDF needs a password") : KI18n.i18nc("@info", "This PDF cannot be shown")
                explanation: viewer.passwordNeeded ? KI18n.i18nc("@info", "Use Open to view it in another program, or Save As to keep a copy.")
                                                   : KI18n.i18nc("@info", "The file may be damaged. Use Open or Save As.")
                icon.name: viewer.passwordNeeded ? "dialog-password" : "image-missing"
            }

            QQC2.Pane {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottomMargin: Kirigami.Units.largeSpacing
                visible: pdf.status === PdfDocument.Ready
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

                    QQC2.Label {
                        Layout.leftMargin: Kirigami.Units.smallSpacing
                        text: KI18n.i18ncp("@info page counter, %1 is the current page", "Page %1 of %2", "Page %1 of %2", pages.currentPage + 1, pdf.pageCount)
                    }
                    QQC2.ToolButton {
                        icon.name: "zoom-out"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Zoom Out")
                        onClicked: {
                            viewer.fitWidth = false;
                            pages.renderScale = Math.max(0.2, pages.renderScale / 1.25);
                        }
                    }
                    QQC2.ToolButton {
                        icon.name: "zoom-in"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Zoom In")
                        onClicked: {
                            viewer.fitWidth = false;
                            pages.renderScale = Math.min(8, pages.renderScale * 1.25);
                        }
                    }
                    QQC2.ToolButton {
                        icon.name: "zoom-fit-width"
                        display: QQC2.AbstractButton.IconOnly
                        text: KI18n.i18nc("@action:button", "Fit Width")
                        checked: viewer.fitWidth
                        onClicked: {
                            viewer.fitWidth = true;
                            viewer.fit();
                        }
                    }
                }
            }
        }
    }

    // Plain text, code, CSV, logs: read-only, monospace, selectable. The window got at most the first
    // 1 MB (info.truncated says whether there was more).
    Component {
        id: textView

        ColumnLayout {
            spacing: 0

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: win.info.truncated ?? false
                type: Kirigami.MessageType.Information
                text: KI18n.i18nc("@info", "Only the first 1 MB of this file is shown. Use Open or Save As for the whole file.")
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                QQC2.TextArea {
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.NoWrap
                    textFormat: TextEdit.PlainText
                    font: Kirigami.Theme.fixedWidthFont
                    text: win.info.text ?? ""
                    leftPadding: Kirigami.Units.gridUnit
                    topPadding: Kirigami.Units.largeSpacing
                    background: null
                }
            }
        }
    }

    // Audio and video: starts paused; play / pause, a position slider, the time and the volume. A
    // file the system cannot decode shows the error text (Open and Save As stay in the header).
    Component {
        id: mediaView

        ColumnLayout {
            id: viewer

            property string errorText: ""

            function timeText(ms: real): string {
                const total = Math.max(0, Math.round(ms / 1000));
                const seconds = String(total % 60).padStart(2, "0");
                const minutes = Math.floor(total / 60) % 60;
                const hours = Math.floor(total / 3600);
                return (hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") : minutes) + ":" + seconds;
            }

            spacing: 0

            MediaPlayer {
                id: player

                source: win.info.fileUrl
                audioOutput: AudioOutput {
                    id: audioOutput
                }
                videoOutput: win.kind === "video" ? video : null
                onErrorOccurred: (error, errorString) => viewer.errorText = errorString
                // Paused, not stopped, shows the first frame of a video instead of an empty window.
                onMediaStatusChanged: {
                    if (mediaStatus === MediaPlayer.LoadedMedia && win.kind === "video") {
                        pause();
                    }
                }
            }

            Component.onDestruction: player.stop()

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: viewer.errorText.length > 0
                type: Kirigami.MessageType.Error
                text: KI18n.i18nc("@info", "This file cannot be played: %1", viewer.errorText)
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                VideoOutput {
                    id: video

                    anchors.fill: parent
                    visible: win.kind === "video"
                }
                Kirigami.Icon {
                    anchors.centerIn: parent
                    visible: win.kind === "audio"
                    source: "audio-x-generic"
                    implicitWidth: Kirigami.Units.iconSizes.huge
                    implicitHeight: implicitWidth
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play()
                }
            }

            QQC2.ToolBar {
                Layout.fillWidth: true

                contentItem: RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.ToolButton {
                        icon.name: player.playbackState === MediaPlayer.PlayingState ? "media-playback-pause" : "media-playback-start"
                        display: QQC2.AbstractButton.IconOnly
                        text: player.playbackState === MediaPlayer.PlayingState ? KI18n.i18nc("@action:button", "Pause") : KI18n.i18nc("@action:button", "Play")
                        onClicked: player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play()
                    }
                    QQC2.Label {
                        text: viewer.timeText(player.position)
                    }
                    QQC2.Slider {
                        Layout.fillWidth: true
                        from: 0
                        to: Math.max(1, player.duration)
                        value: player.position
                        enabled: player.seekable
                        onMoved: player.position = value
                    }
                    QQC2.Label {
                        text: viewer.timeText(player.duration)
                    }
                    Kirigami.Icon {
                        source: audioOutput.volume > 0 && !audioOutput.muted ? "audio-volume-high" : "audio-volume-muted"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: implicitWidth
                    }
                    QQC2.Slider {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                        from: 0
                        to: 1
                        value: audioOutput.volume
                        onMoved: audioOutput.volume = value
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
