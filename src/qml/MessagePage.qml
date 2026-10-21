// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtCore
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import QtWebEngine
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import Korrespond

Kirigami.Page {
    id: page

    readonly property string html: MailController.messageHtml

    Kirigami.Theme.colorSet: Kirigami.Theme.View
    Kirigami.Theme.inherit: false

    // The body's own colours follow the pane.
    function syncBodyColours() {
        MailController.setMessageColours(Kirigami.Theme.textColor, Kirigami.Theme.backgroundColor, Kirigami.Theme.linkColor);
    }

    // Any of the three changing (scheme switch) re-renders the open message once, from the stored mail.
    readonly property color bodyText: Kirigami.Theme.textColor
    readonly property color bodyBackground: Kirigami.Theme.backgroundColor
    readonly property color bodyLink: Kirigami.Theme.linkColor
    onBodyTextChanged: Qt.callLater(syncBodyColours)
    onBodyBackgroundChanged: Qt.callLater(syncBodyColours)
    onBodyLinkChanged: Qt.callLater(syncBodyColours)

    // What the view currently shows. currentMessageChanged also fires for flag and read-state updates,
    // and reloading identical HTML makes the pane flash.
    property string shownHtml
    property bool shownImages: false
    property bool hasShown: false

    function showHtml() {
        const images = MailController.remoteImagesLoaded;
        if (hasShown && html === shownHtml && images === shownImages) {
            return;
        }
        hasShown = true;
        shownHtml = html;
        shownImages = images;
        // Set right before loading so the setting and the HTML always belong to the same message.
        view.settings.autoLoadImages = images;
        view.loadHtml(html, "about:blank");
    }

    readonly property bool hasMessage: MailController.currentUid > 0

    Connections {
        target: page.application

        function onComposeRequested(kind: string) {
            page.openCompose(kind);
        }
    }
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

    // The window's action collection (set by Main.qml); one definition serves the toolbar and the keys.
    property MailApplication application

    // Opens a compose window: blank for "new", else prefilled for "reply", "replyAll" or "forward"
    // (see composeFor()). A separate window per message; it destroys itself when closed.
    // The initial values are passed to createObject() so ComposeWindow sees them when it completes.
    function openCompose(kind) {
        let initial = {};
        if (kind !== "new") {
            const data = MailController.composeFor(kind);
            // composeFor() answers an empty map when no message is open.
            if (!data.kind) {
                return;
            }
            initial = {
                initialAccount: data.account,
                initialTo: data.to,
                initialCc: data.cc,
                initialSubject: data.subject,
                initialBody: data.body,
                initialInReplyTo: data.inReplyTo,
                initialReferences: data.references
            };
        }
        const component = Qt.createComponent("Korrespond", "ComposeWindow");
        const composer = component.createObject((page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow), initial);
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

        // The search field is in this title bar.
        Connections {
            target: page.application

            function onSearchRequested() {
                searchField.forceActiveFocus();
            }
        }

        Repeater {
            model: [
                { move: "archive" },
                { move: "delete" },
                { unread: true },
                { flag: true },
                { move: "folder" },
                { icon: "tag", text: KI18n.i18nc("@action:button", "Label"), tooltip: KI18n.i18nc("@info:tooltip", "Label") },
                { separator: true },
                { icon: "mail-reply-sender", action: "reply", text: KI18n.i18nc("@action:button", "Reply"), tooltip: KI18n.i18nc("@info:tooltip", "Reply") + " (R)" },
                { icon: "mail-reply-all", action: "reply_all", text: KI18n.i18nc("@action:button", "Reply All"), tooltip: KI18n.i18nc("@info:tooltip", "Reply All") + " (Shift+R)" },
                { icon: "mail-forward", action: "forward", text: KI18n.i18nc("@action:button", "Forward"), tooltip: KI18n.i18nc("@info:tooltip", "Forward") + " (Shift+F)" }
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
                        icon.color: MailController.messageFlagged ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
                        text: MailController.messageFlagged ? KI18n.i18nc("@action:button", "Unflag")
                                                            : KI18n.i18nc("@action:button", "Flag")
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: page.application.action("flag").trigger()

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
                        onClicked: page.application.action(MailController.messageUnread ? "mark_read" : "mark_unread").trigger()

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
                        text: KI18n.i18nc("@action:button", "Archive")
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canArchive
                        onClicked: page.application.action("archive").trigger()

                        QQC2.ToolTip.text: enabled ? text + " (E)" : KI18n.i18nc("@info:tooltip", "Archive is not available: this account has no archive folder, or the message is already there.")
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
                        onClicked: page.application.action("delete").trigger()

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
                        text: toolLoader.modelData.text
                        display: QQC2.AbstractButton.IconOnly
                        enabled: MailController.canCompose // enable only when we can compose a message
                        onClicked: page.application.action(toolLoader.modelData.action).trigger()

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
            // Dev only: the KORRESPOND_DEMO_KEY_UNDO_SEARCH hook in main.cpp finds the field by name.
            objectName: "searchField"

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

    // The title bar band: the page row's header gets the Header set explicitly.
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
            id: band

            // A scheme without Header colours gets Kirigami's dark fallback, so the Window set is
            // used when its text would not read on the Header background.
            readonly property bool useWindow: MimeTheme.contrast(Kirigami.Theme.backgroundColor, windowSet.Kirigami.Theme.textColor) < 4.5
            Kirigami.Theme.colorSet: Kirigami.Theme.Header
            Kirigami.Theme.inherit: false
            color: useWindow ? windowSet.Kirigami.Theme.backgroundColor : Kirigami.Theme.backgroundColor

            Item {
                id: windowSet
                Kirigami.Theme.colorSet: Kirigami.Theme.Window
                Kirigami.Theme.inherit: false
            }

            // The header's own contents (title, actions) take their colours from the toolbar's set.
            Binding {
                target: (page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow).pageStack.globalToolBar
                property: "colorSet"
                value: Kirigami.Theme.Window
                when: useWindow
            }
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
            previewAttachment(index);
        }
    }

    // A preview window of its own, with a copy of what it needs: it stays valid when another message
    // is opened. An empty answer means the controller refused or failed (it showed the error).
    function previewAttachment(index) {
        const info = MailController.previewAttachment(index);
        if (!info.kind) {
            return;
        }
        const component = Qt.createComponent("Korrespond", "AttachmentPreviewWindow");
        const preview = component.createObject((page.QQC2.ApplicationWindow.window as Kirigami.ApplicationWindow), {
            info: info
        });
        if (preview) {
            (preview as Window).show();
        } else {
            console.warn(component.errorString());
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
            visible: page.loaded && !MailController.conversationOpen
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
                            font.weight: Font.Bold
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: page.sender.address
                            textFormat: Text.PlainText
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                        }
                        QQC2.Label {
                            Layout.alignment: Qt.AlignRight
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
            visible: page.loaded && !MailController.conversationOpen
            color: Qt.alpha(Kirigami.Theme.textColor, MimeTheme.dividerAlpha)
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Information
            visible: page.loaded && !MailController.conversationOpen && MailController.messageHasRemoteImages
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
                visible: page.loaded && !MailController.conversationOpen
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

            // A conversation of more than one message (conversation view on): the stacked messages.
            Loader {
                anchors.fill: parent
                active: MailController.conversationOpen

                sourceComponent: ConversationView {
                    onComposeRequested: (row, kind) => {
                        MailController.selectConversationMessage(row);
                        page.openCompose(kind);
                    }
                    onAttachmentRequested: (row, index) => {
                        MailController.selectConversationMessage(row);
                        page.openAttachment(index);
                    }
                }
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
