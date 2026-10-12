// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.ki18n
import org.kde.kirigami as Kirigami
import Korrespond

// The message window: a new message, or a reply / forward when the initial* values are given
// (see MailController.composeFor()).
Kirigami.ApplicationWindow {
    id: composer

    // What the fields start with; closing asks before discarding only when they were changed.
    property string initialAccount
    property string initialTo
    property string initialCc
    property string initialBcc
    property string initialSubject
    property string initialBody
    // The formatted version of initialBody (a message that came back); empty for plain text.
    property string initialHtml
    property string initialInReplyTo
    property var initialReferences: []
    // A message that came back (undo send, failed hand-off): the paths of its attachments, the error
    // to show, and the fact that its text is not saved anywhere else (closing asks first).
    property var initialAttachments: []
    property string initialError
    property bool restored: false

    readonly property bool hasContent: restored || toField.text !== initialTo || ccField.text !== initialCc || bccField.text.length > 0
        || subjectField.text !== initialSubject || plainBody() !== initialBody || attachments.length > 0
    property bool discardConfirmed: false
    // True from pressing Send until the server has answered (or the message is queued).
    property bool waiting: false
    // The id MailController gave the message; 0 until sendMessage() returned.
    property int sendId: 0
    property string errorText
    // The names of restored attachments whose files are gone (shown as a dismissable warning).
    property var missingAttachments: []
    // The files to attach: {path, name, size}.
    property var attachments: []
    readonly property double attachmentsTotal: attachments.reduce((sum, file) => sum + file.size, 0)
    readonly property bool attachmentsTooLarge: attachmentsTotal > MailController.maxAttachmentBytes

    title: subjectField.text.length > 0 ? subjectField.text : KI18n.i18nc("@title:window", "New Message")
    width: Kirigami.Units.gridUnit * 38
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 22
    minimumHeight: Kirigami.Units.gridUnit * 16
    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None

    // The text of the editor without formatting (bodyArea.text is HTML in rich-text mode). Reading
    // `length` makes bindings that call this follow edits.
    function plainBody(): string {
        return bodyArea.length >= 0 ? bodyArea.getText(0, bodyArea.length) : "";
    }

    // A restored attachment is a plain path, not a URL: encode each segment so that % and # in a file
    // name survive the QUrl conversion done by fileInfo().
    function asFileUrl(entry) {
        const text = String(entry);
        return text.startsWith("/") ? "file://" + text.split("/").map(encodeURIComponent).join("/") : entry;
    }

    function displayName(entry) {
        const text = String(entry);
        const last = text.split("/").pop();
        if (text.startsWith("/")) {
            return last;
        }
        try {
            return decodeURIComponent(last);
        } catch (error) {
            return last;
        }
    }

    // Returns the file names that could not be read (deleted, unreadable).
    function addAttachments(urls) {
        const list = attachments.slice();
        const missing = [];
        for (const url of urls) {
            const info = MailController.fileInfo(asFileUrl(url));
            if (info.path === undefined) {
                missing.push(displayName(url));
            } else if (!list.some(file => file.path === info.path)) {
                list.push(info);
            }
        }
        attachments = list;
        return missing;
    }

    function send() {
        if (attachmentsTooLarge) {
            return;
        }
        errorText = "";
        sendId = 0;
        waiting = true;
        sendId = MailController.sendMessage({
            account: fromBox.currentValue,
            to: toField.text,
            cc: ccField.text,
            bcc: bccField.text,
            subject: subjectField.text,
            body: plainBody(),
            html: formatter.formattedHtml(),
            inReplyTo: initialInReplyTo,
            references: initialReferences,
            attachments: attachments.map(file => file.path)
        });
        // Queued for the undo-send delay: the content lives in the controller now (Undo or a failure
        // reopens it), so the window goes away at once.
        if (MailController.isPendingSend(sendId)) {
            waiting = false;
            discardConfirmed = true;
            close();
        }
    }

    Component.onCompleted: {
        toField.text = initialTo;
        ccField.text = initialCc;
        bccField.text = initialBcc;
        subjectField.text = initialSubject;
        // The binding of `document` may not have run yet; the body has to go into the document now.
        formatter.editorDocument = bodyArea.textDocument;
        if (initialHtml.length > 0) {
            formatter.setHtml(initialHtml);
        } else {
            formatter.setPlainText(initialBody);
        }
        errorText = initialError;
        if (initialAttachments.length > 0) {
            missingAttachments = addAttachments(initialAttachments);
        }
        const index = fromBox.indexOfValue(initialAccount);
        fromBox.currentIndex = index >= 0 ? index : 0;
        // A reply starts with the cursor above the quoted text.
        if (initialTo.length > 0) {
            bodyArea.forceActiveFocus();
            bodyArea.cursorPosition = 0;
        }
    }

    // The answer to Send; only this window's own send counts (by id; 0 = still inside sendMessage()).
    Connections {
        target: MailController
        enabled: composer.waiting

        function onMessageSent(id: int) {
            if (composer.sendId !== 0 && id !== composer.sendId) {
                return;
            }
            composer.waiting = false;
            composer.discardConfirmed = true;
            composer.close();
        }
        function onSendFailed(id: int, message: string, original: var, queued: bool) {
            if (queued || (composer.sendId !== 0 && id !== composer.sendId)) {
                return;
            }
            composer.waiting = false;
            composer.errorText = message;
        }
    }

    // Closing with typed text asks first, like Discard.
    onClosing: close => {
        if (hasContent && !discardConfirmed) {
            close.accepted = false;
            discardDialog.open();
        } else {
            destroy();
        }
    }

    pageStack.initialPage: Kirigami.Page {
        padding: 0

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Restored message whose attached files were removed in the meantime.
            Kirigami.InlineMessage {
                Layout.fillWidth: true
                type: Kirigami.MessageType.Warning
                showCloseButton: true
                visible: composer.missingAttachments.length > 0
                text: KI18n.i18np("This attachment is no longer available and was left out: %2",
                    "These %1 attachments are no longer available and were left out: %2",
                    composer.missingAttachments.length, composer.missingAttachments.join(", "))
            }

            // From / To / Cc / Subject rows with hairline dividers, like the mail clients it is modeled on.
            GridLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.gridUnit
                Layout.rightMargin: Kirigami.Units.gridUnit
                Layout.topMargin: Kirigami.Units.largeSpacing
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "From:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.ComboBox {
                    id: fromBox

                    Layout.fillWidth: true
                    model: MailController.accounts
                    valueRole: "email"
                    // "Name <address>", or just the address.
                    textRole: "email"
                    enabled: !composer.waiting
                    delegate: QQC2.ItemDelegate {
                        required property var model

                        width: fromBox.width
                        text: model.displayName.length > 0 && model.displayName !== model.email ? model.displayName + " <" + model.email + ">" : model.email
                        highlighted: fromBox.highlightedIndex === model.index
                    }
                    displayText: {
                        const row = currentIndex;
                        return currentValue ? currentValue : "";
                    }
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "To:")
                    color: Kirigami.Theme.disabledTextColor
                }
                RecipientField {
                    id: toField

                    Layout.fillWidth: true
                    placeholderText: KI18n.i18nc("@info:placeholder", "name@example.com, …")
                    focus: true
                    enabled: !composer.waiting
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Cc:")
                    color: Kirigami.Theme.disabledTextColor
                }
                RecipientField {
                    id: ccField

                    Layout.fillWidth: true
                    enabled: !composer.waiting
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Bcc:")
                    color: Kirigami.Theme.disabledTextColor
                }
                RecipientField {
                    id: bccField

                    Layout.fillWidth: true
                    enabled: !composer.waiting
                }

                QQC2.Label {
                    Layout.alignment: Qt.AlignRight
                    text: KI18n.i18nc("@label", "Subject:")
                    color: Kirigami.Theme.disabledTextColor
                }
                QQC2.TextField {
                    id: subjectField

                    Layout.fillWidth: true
                    enabled: !composer.waiting
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
            }

            QQC2.ToolBar {
                Layout.fillWidth: true
                leftPadding: Kirigami.Units.gridUnit - Kirigami.Units.smallSpacing
                background: null

                Row {
                    spacing: Kirigami.Units.smallSpacing

                    QQC2.ToolButton {
                        icon.name: "format-text-bold"
                        text: KI18n.i18nc("@action:button", "Bold")
                        display: QQC2.AbstractButton.IconOnly
                        focusPolicy: Qt.NoFocus
                        enabled: !composer.waiting
                        down: pressed || formatter.bold
                        onClicked: formatter.bold = !formatter.bold
                        QQC2.ToolTip.text: KI18n.i18nc("@info:tooltip", "Bold (Ctrl+B)")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "format-text-italic"
                        text: KI18n.i18nc("@action:button", "Italic")
                        display: QQC2.AbstractButton.IconOnly
                        focusPolicy: Qt.NoFocus
                        enabled: !composer.waiting
                        down: pressed || formatter.italic
                        onClicked: formatter.italic = !formatter.italic
                        QQC2.ToolTip.text: KI18n.i18nc("@info:tooltip", "Italic (Ctrl+I)")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "format-text-underline"
                        text: KI18n.i18nc("@action:button", "Underline")
                        display: QQC2.AbstractButton.IconOnly
                        focusPolicy: Qt.NoFocus
                        enabled: !composer.waiting
                        down: pressed || formatter.underline
                        onClicked: formatter.underline = !formatter.underline
                        QQC2.ToolTip.text: KI18n.i18nc("@info:tooltip", "Underline (Ctrl+U)")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                    QQC2.ToolButton {
                        icon.name: "format-text-strikethrough"
                        text: KI18n.i18nc("@action:button", "Strikethrough")
                        display: QQC2.AbstractButton.IconOnly
                        focusPolicy: Qt.NoFocus
                        enabled: !composer.waiting
                        down: pressed || formatter.strikeOut
                        onClicked: formatter.strikeOut = !formatter.strikeOut
                        QQC2.ToolTip.text: KI18n.i18nc("@info:tooltip", "Strikethrough")
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    }
                }
            }

            FormatHandler {
                id: formatter

                editorDocument: bodyArea.textDocument
                selectionStart: bodyArea.selectionStart
                selectionEnd: bodyArea.selectionEnd
                cursorPosition: bodyArea.cursorPosition
            }

            Shortcut {
                sequence: StandardKey.Bold
                enabled: bodyArea.activeFocus
                onActivated: formatter.bold = !formatter.bold
            }

            Shortcut {
                sequence: StandardKey.Italic
                enabled: bodyArea.activeFocus
                onActivated: formatter.italic = !formatter.italic
            }

            Shortcut {
                sequence: StandardKey.Underline
                enabled: bodyArea.activeFocus
                onActivated: formatter.underline = !formatter.underline
            }

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                QQC2.TextArea {
                    id: bodyArea

                    wrapMode: TextEdit.Wrap
                    textFormat: TextEdit.RichText
                    placeholderText: KI18n.i18nc("@info:placeholder", "Write your message…")
                    leftPadding: Kirigami.Units.gridUnit
                    rightPadding: Kirigami.Units.gridUnit
                    topPadding: Kirigami.Units.largeSpacing
                    background: null
                    readOnly: composer.waiting
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }

            Flow {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                Layout.leftMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing
                visible: composer.attachments.length > 0

                Repeater {
                    model: composer.attachments

                    delegate: Kirigami.Chip {
                        required property var modelData

                        text: modelData.name + " (" + Qt.locale().formattedDataSize(modelData.size) + ")"
                        icon.name: "mail-attachment"
                        closable: !composer.waiting
                        checkable: false
                        onRemoved: composer.attachments = composer.attachments.filter(file => file.path !== modelData.path)
                    }
                }
                QQC2.Label {
                    text: composer.attachmentsTooLarge
                        ? KI18n.i18nc("@info", "Attachments are larger than 25 MB.")
                        : KI18n.i18nc("@info", "Total: %1", Qt.locale().formattedDataSize(composer.attachmentsTotal))
                    color: composer.attachmentsTooLarge ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                Layout.leftMargin: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.smallSpacing

                // Progress or the reason the last try failed; the text stays in the window.
                QQC2.Label {
                    Layout.fillWidth: true
                    text: composer.waiting ? KI18n.i18nc("@info:status", "Sending…") : composer.errorText
                    color: composer.waiting ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.negativeTextColor
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                QQC2.BusyIndicator {
                    visible: composer.waiting
                    running: visible
                    Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                    Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                }
                QQC2.Button {
                    icon.name: "mail-attachment"
                    display: QQC2.AbstractButton.IconOnly
                    enabled: !composer.waiting
                    onClicked: attachDialog.open()
                    QQC2.ToolTip.text: KI18n.i18nc("@info:tooltip", "Attach files")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    Accessible.name: KI18n.i18nc("@action:button", "Attach files")
                }
                QQC2.Button {
                    text: KI18n.i18nc("@action:button", "Discard")
                    icon.name: "edit-delete"
                    enabled: !composer.waiting
                    onClicked: composer.close()
                }
                QQC2.Button {
                    text: KI18n.i18nc("@action:button", "Send")
                    icon.name: "document-send"
                    enabled: !composer.waiting && !MailController.sending && !composer.attachmentsTooLarge && fromBox.currentValue !== undefined
                        && (toField.text.trim().length > 0 || ccField.text.trim().length > 0 || bccField.text.trim().length > 0)
                    onClicked: composer.send()
                }
            }
        }

        // Files dropped anywhere on the window are attached; other drags (text) fall through to the fields.
        DropArea {
            id: dropArea

            function acceptsFiles(urls: var, hasUrls: bool): bool {
                if (!hasUrls || composer.waiting || MailController.sending || !urls || urls.length === 0) {
                    return false;
                }
                for (const url of urls) {
                    if (!String(url).startsWith("file:")) {
                        return false;
                    }
                }
                return true;
            }

            anchors.fill: parent
            z: 100
            onEntered: drag => {
                if (acceptsFiles(drag.urls, drag.hasUrls)) {
                    drag.accept(Qt.CopyAction);
                } else {
                    drag.accepted = false;
                }
            }
            onDropped: drop => {
                if (acceptsFiles(drop.urls, drop.hasUrls)) {
                    composer.addAttachments(drop.urls);
                    drop.accept(Qt.CopyAction);
                }
            }

            Rectangle {
                anchors.fill: parent
                visible: dropArea.containsDrag
                color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.85)
                Accessible.role: Accessible.StaticText
                Accessible.name: dropLabel.text

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing
                    radius: Kirigami.Units.cornerRadius
                    color: "transparent"
                    border.width: 2
                    border.color: Kirigami.Theme.highlightColor
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Kirigami.Units.largeSpacing

                    Kirigami.Icon {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: Kirigami.Units.iconSizes.huge
                        implicitHeight: Kirigami.Units.iconSizes.huge
                        source: "mail-attachment"
                    }

                    Kirigami.Heading {
                        id: dropLabel

                        Layout.alignment: Qt.AlignHCenter
                        level: 2
                        text: KI18n.i18nc("@info", "Drop files to attach")
                    }
                }
            }
        }
    }

    FileDialog {
        id: attachDialog

        title: KI18n.i18nc("@title:window", "Attach Files")
        fileMode: FileDialog.OpenFiles
        onAccepted: composer.addAttachments(selectedFiles)
    }

    Kirigami.PromptDialog {
        id: discardDialog

        title: KI18n.i18nc("@title:dialog", "Discard Message?")
        subtitle: KI18n.i18nc("@info", "The text you wrote will be lost.")
        standardButtons: Kirigami.Dialog.NoButton
        customFooterActions: [
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Discard")
                icon.name: "edit-delete"
                onTriggered: {
                    composer.discardConfirmed = true;
                    discardDialog.close();
                    composer.close();
                }
            },
            Kirigami.Action {
                text: KI18n.i18nc("@action:button", "Keep Writing")
                icon.name: "document-edit"
                onTriggered: discardDialog.close()
            }
        ]
    }
}
