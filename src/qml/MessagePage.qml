// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import QtWebEngine
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import PlasmaMail

Kirigami.Page {
    id: page

    readonly property string html: MailController.messageHtml

    Kirigami.Theme.inherit: false
    Kirigami.Theme.backgroundColor: MimeTheme.readingBackground
    Kirigami.Theme.textColor: MimeTheme.text
    Kirigami.Theme.disabledTextColor: MimeTheme.secondaryText
    Kirigami.Theme.highlightColor: MimeTheme.accent

    // The body's own colours follow the pane.
    function syncBodyColours() {
        MailController.setMessageColours(MimeTheme.text, MimeTheme.readingBackground, MimeTheme.link);
    }
    Connections {
        target: MimeTheme
        function onDarkChanged() { page.syncBodyColours(); }
    }

    function showHtml() {
        // Set right before loading so the setting and the HTML always belong to the same message.
        view.settings.autoLoadImages = MailController.remoteImagesLoaded;
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

    // Opens a compose window prefilled for "reply", "replyAll" or "forward" (see composeFor()).
    // The initial values are passed to createObject() so ComposeWindow sees them when it completes.
    function openCompose(kind) {
        const data = MailController.composeFor(kind);
        // composeFor() answers an empty map when no message is open.
        if (!data.kind) {
            return;
        }
        const component = Qt.createComponent("PlasmaMail", "ComposeWindow");
        const composer = component.createObject((page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow), {
            initialAccount: data.account,
            initialTo: data.to,
            initialCc: data.cc,
            initialSubject: data.subject,
            initialBody: data.body,
            initialInReplyTo: data.inReplyTo,
            initialReferences: data.references
        });
        if (composer) {
            (composer as Window).show();
        } else {
            console.warn(component.errorString());
        }
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
                { icon: "tag", text: KI18n.i18nc("@action:button", "Label"), tooltip: KI18n.i18nc("@info:tooltip", "Label") },
                { separator: true },
                { icon: "mail-reply-sender", kind: "reply", text: KI18n.i18nc("@action:button", "Reply"), tooltip: KI18n.i18nc("@info:tooltip", "Reply") + " (R)" },
                { icon: "mail-reply-all", kind: "replyAll", text: KI18n.i18nc("@action:button", "Reply All"), tooltip: KI18n.i18nc("@info:tooltip", "Reply All") + " (Shift+R)" },
                { icon: "mail-forward", kind: "forward", text: KI18n.i18nc("@action:button", "Forward"), tooltip: KI18n.i18nc("@info:tooltip", "Forward") + " (Shift+F)" }
            ]

            delegate: Loader {
                id: toolLoader

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
                        icon.color: MailController.messageFlagged ? "#f67400" : MimeTheme.toolbarIcon
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
                        icon.color: enabled ? MimeTheme.toolbarIcon : Qt.alpha(MimeTheme.toolbarIcon, 0.4)
                        text: MailController.messageUnread ? KI18n.i18nc("@action:button", "Mark as Read")
                                                           : KI18n.i18nc("@action:button", "Mark as Unread")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: MailController.setUnread(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageUnread)

                        QQC2.ToolTip.text: text + " (U)"
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                // Archive and Delete move the open message on the server (never for good).
                Component {
                    id: archiveComponent

                    QQC2.ToolButton {
                        icon.name: "archive-insert"
                        icon.color: enabled ? MimeTheme.toolbarIcon : Qt.alpha(MimeTheme.toolbarIcon, 0.4)
                        text: KI18n.i18nc("@action:button", "Archive")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canArchive
                        onClicked: MailController.archiveMessage()

                        QQC2.ToolTip.text: enabled ? text + " (E)" : KI18n.i18nc("@info:tooltip", "Archive is not available: this account has no archive folder, or the message is already there.")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }

                Component {
                    id: deleteComponent

                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        icon.color: enabled ? MimeTheme.toolbarIcon : Qt.alpha(MimeTheme.toolbarIcon, 0.4)
                        text: KI18n.i18nc("@action:button", "Delete")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canDelete
                        onClicked: MailController.deleteMessage()

                        QQC2.ToolTip.text: enabled ? KI18n.i18nc("@info:tooltip", "Move to Trash") + " (Delete)" : KI18n.i18nc("@info:tooltip", "Delete is not available: this account has no Trash folder, or the message is already there.")
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
                        icon.color: enabled ? MimeTheme.toolbarIcon : Qt.alpha(MimeTheme.toolbarIcon, 0.4)
                        text: KI18n.i18nc("@action:button", "Move to Folder")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canMove
                        down: moveMenu.visible
                        onClicked: moveMenu.popup(this, 0, height)

                        QQC2.ToolTip.text: enabled ? text : KI18n.i18nc("@info:tooltip", "Move to Folder is not available for this message.")
                        QQC2.ToolTip.visible: hovered && !moveMenu.visible
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                        MoveFolderMenu {
                            id: moveMenu
                        }
                    }
                }

                Component {
                    id: buttonComponent

                    QQC2.ToolButton {
                        icon.name: toolLoader.modelData.icon
                        icon.color: enabled ? MimeTheme.toolbarIcon : Qt.alpha(MimeTheme.toolbarIcon, 0.4)
                        text: toolLoader.modelData.text
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canCompose // enable only when we can compose a message
                        onClicked: page.openCompose(toolLoader.modelData.kind)

                        QQC2.ToolTip.text: toolLoader.modelData.tooltip
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
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
            id: searchField

            Layout.preferredWidth: Kirigami.Units.gridUnit * 14
            Layout.rightMargin: Kirigami.Units.smallSpacing
            enabled: MailController.currentFolder !== ""
            // Enter searches the open folder on the server; Escape or an empty field shows the folder again.
            autoAccept: false // the server is asked on Enter only, not after every keystroke
            onAccepted: MailController.search(text)
            onTextChanged: {
                if (text === "") {
                    MailController.clearSearch();
                }
            }
            Keys.onEscapePressed: {
                text = "";
                MailController.clearSearch();
            }

            // The search ends by itself when another folder is opened.
            Connections {
                target: MailController

                function onSearchActiveChanged() {
                    if (!MailController.searchActive) {
                        searchField.text = "";
                    }
                }
            }
        }
    }
    padding: 0
    onHtmlChanged: showHtml()

    // The title bar band: the page row's header gets the deep purple instead of the theme's header colour.
    function styleHeader() {
        const header = (page.Kirigami.ColumnView.globalHeader as Loader)?.item as Kirigami.AbstractApplicationHeader;
        if (header) {
            header.background = headerBand.createObject(header);
        }
    }
    Connections {
        target: page.Kirigami.ColumnView.globalHeader as Loader
        ignoreUnknownSignals: true

        function onLoaded() {
            page.styleHeader();
        }
    }
    Component {
        id: headerBand

        Rectangle {
            color: MimeTheme.headerBackground
        }
    }

    // Attachment currently being asked about / saved (never logged).
    property int pendingIndex: -1
    property string pendingName: ""

    // Every call to MailController.openAttachment goes through here, so pendingIndex is always the
    // attachment the controller is working on (its blocked signal is answered with it).
    function openAttachment(index) {
        pendingIndex = index;
        if (MailController.attachmentIsRisky(index)) {
            askSave(index);
        } else {
            MailController.openAttachment(index);
        }
    }

    function askSave(index) {
        pendingIndex = index;
        pendingName = MailController.attachmentFileName(index);
        riskyDialog.open();
    }

    function saveAttachmentAs(index) {
        pendingIndex = index;
        pendingName = MailController.attachmentFileName(index);
        const folder = String(StandardPaths.writableLocation(StandardPaths.DownloadLocation)).replace(/\/+$/, "");
        saveDialog.currentFolder = folder;
        // Encoded, so '#', '?' or '%' in the name stay part of the file name.
        saveDialog.selectedFile = folder + "/" + encodeURIComponent(pendingName);
        saveDialog.open();
    }

    FileDialog {
        id: saveDialog

        fileMode: FileDialog.SaveFile
        onAccepted: {
            if (page.pendingIndex >= 0) {
                MailController.saveAttachment(page.pendingIndex, selectedFile);
            }
        }
    }

    Kirigami.PromptDialog {
        id: riskyDialog

        title: KI18n.i18nc("@title:dialog", "Potentially Unsafe File")
        subtitle: KI18n.i18nc("@info", "Opening this type of file can run code. Save it instead?")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Save As…")
                icon.name: "document-save-as"
                onTriggered: {
                    riskyDialog.close();
                    page.saveAttachmentAs(page.pendingIndex);
                }
            },
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Cancel")
                icon.name: "dialog-cancel"
                onTriggered: riskyDialog.close()
            }
        ]
    }

    Connections {
        target: MailController

        // Safety net: the controller refused to open a risky type. It is answered for the index that
        // openAttachment was last called with (set by page.openAttachment), not by the name.
        function onAttachmentBlocked(name) {
            if (page.pendingIndex >= 0) {
                page.askSave(page.pendingIndex);
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+Shift+F"
        enabled: page.hasMessage
        onActivated: MailController.setFlagged(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageFlagged)
    }
    // Single-key shortcuts; a focused text field takes the key instead.
    Shortcut {
        sequence: "R"
        enabled: page.hasMessage && MailController.canCompose
        onActivated: page.openCompose("reply")
    }
    Shortcut {
        sequence: "Shift+R"
        enabled: page.hasMessage && MailController.canCompose
        onActivated: page.openCompose("replyAll")
    }
    Shortcut {
        sequence: "Shift+F"
        enabled: page.hasMessage && MailController.canCompose
        onActivated: page.openCompose("forward")
    }
    Shortcut {
        sequence: "U"
        enabled: page.hasMessage
        onActivated: MailController.setUnread(MailController.currentMessageAccount, MailController.currentUid, !MailController.messageUnread)
    }
    Shortcut {
        sequence: "E"
        enabled: page.hasMessage && MailController.canArchive
        onActivated: MailController.archiveMessage()
    }
    Shortcut {
        sequence: "Delete"
        enabled: page.hasMessage && MailController.canDelete
        onActivated: MailController.deleteMessage()
    }
    Component.onCompleted: {
        styleHeader();
        syncBodyColours();
        showHtml();
    }

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
                color: MimeTheme.text
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
                            color: MimeTheme.text
                            font.weight: Font.Bold
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: page.sender.address
                            textFormat: Text.PlainText
                            color: MimeTheme.secondaryText
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            Layout.alignment: Qt.AlignRight
                            text: isNaN(MailController.messageDate) ? ""
                                : MailController.messageDate.toLocaleString(Qt.locale(), Locale.ShortFormat)
                            color: MimeTheme.secondaryText
                            font: Kirigami.Theme.smallFont
                        }
                    }
                    QQC2.Label {
                        Layout.fillWidth: true
                        visible: MailController.messageTo.length > 0
                        text: KI18n.i18nc("@label recipients of a message", "To: %1", MailController.messageTo)
                        textFormat: Text.PlainText
                        color: MimeTheme.secondaryText
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

                    delegate: Row {
                        id: attachment

                        required property int index
                        required property string modelData

                        spacing: 0

                        Kirigami.Chip {
                            text: attachment.modelData
                            icon.name: "mail-attachment"
                            closable: false
                            checkable: false
                            hoverEnabled: true
                            activeFocusOnTab: true
                            focusPolicy: Qt.StrongFocus

                            HoverHandler { cursorShape: Qt.PointingHandCursor }
                            TapHandler {
                                acceptedButtons: Qt.RightButton
                                onTapped: attachmentMenu.popup()
                            }
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: attachment.modelData
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay

                            onClicked: page.openAttachment(attachment.index)
                            Keys.onReturnPressed: clicked()
                            Keys.onEnterPressed: clicked()
                        }

                        QQC2.ToolButton {
                            icon.name: "arrow-down"
                            display: QQC2.AbstractButton.IconOnly
                            text: KI18n.i18nc("@action:button", "Attachment Actions")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: text
                            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                            onClicked: attachmentMenu.popup()
                        }

                        QQC2.Menu {
                            id: attachmentMenu

                            QQC2.MenuItem {
                                text: KI18n.i18nc("@action:inmenu", "Open")
                                icon.name: "document-open"
                                onTriggered: page.openAttachment(attachment.index)
                            }
                            QQC2.MenuItem {
                                text: KI18n.i18nc("@action:inmenu", "Save As…")
                                icon.name: "document-save-as"
                                onTriggered: page.saveAttachmentAs(attachment.index)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            visible: page.loaded
            color: MimeTheme.divider
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: page.loaded && MailController.messageHasRemoteImages
            text: KI18n.i18nc("@info", "This message has images that were not loaded.")
            actions: Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Load images")
                icon.name: "view-preview"
                onTriggered: MailController.loadRemoteImages()
            }
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
