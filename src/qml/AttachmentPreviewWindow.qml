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
            sourceComponent: placeholder
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
