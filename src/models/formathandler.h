// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QString>
#include <QTextCharFormat>
#include <QTextCursor>

class QTextDocument;

// Bold, italic, underline and strike-through for the body editor. A TextEdit cannot style its
// selection by itself, so QML gives this object the editor's QTextDocument (`textDocument.textDocument`
// of the TextEdit) and the selection and cursor positions; the style properties read and write the
// character format there.
//
// With a selection the style applies to the selection; without one, to the word under the cursor
// (a TextEdit's typing format is not reachable from outside).
class FormatHandler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QTextDocument *document READ document WRITE setDocument NOTIFY documentChanged)
    // QML: the TextEdit's `textDocument` (a QQuickTextDocument); its QTextDocument is used.
    Q_PROPERTY(QQuickTextDocument *editorDocument READ editorDocument WRITE setEditorDocument NOTIFY documentChanged)
    Q_PROPERTY(int selectionStart READ selectionStart WRITE setSelectionStart NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd WRITE setSelectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY selectionChanged)
    Q_PROPERTY(bool bold READ bold WRITE setBold NOTIFY formatChanged)
    Q_PROPERTY(bool italic READ italic WRITE setItalic NOTIFY formatChanged)
    Q_PROPERTY(bool underline READ underline WRITE setUnderline NOTIFY formatChanged)
    Q_PROPERTY(bool strikeOut READ strikeOut WRITE setStrikeOut NOTIFY formatChanged)

public:
    explicit FormatHandler(QObject *parent = nullptr);

    QTextDocument *document() const;
    void setDocument(QTextDocument *document);

    QQuickTextDocument *editorDocument() const;
    void setEditorDocument(QQuickTextDocument *editorDocument);

    int selectionStart() const;
    void setSelectionStart(int position);
    int selectionEnd() const;
    void setSelectionEnd(int position);
    int cursorPosition() const;
    void setCursorPosition(int position);

    bool bold() const;
    void setBold(bool on);
    bool italic() const;
    void setItalic(bool on);
    bool underline() const;
    void setUnderline(bool on);
    bool strikeOut() const;
    void setStrikeOut(bool on);

    // The text as HTML (see ComposeHtml) when any of it is bold, italic, underlined or struck through;
    // an empty string when the text has no formatting, so it can go out as plain text.
    Q_INVOKABLE QString formattedHtml() const;

    // Replaces the whole text with HTML written by formattedHtml() (a restored message).
    Q_INVOKABLE void setHtml(const QString &html);

    // Replaces the whole text, without any formatting.
    Q_INVOKABLE void setPlainText(const QString &text);

Q_SIGNALS:
    void documentChanged();
    void selectionChanged();
    void formatChanged();

private:
    // The selection, or the word under the cursor when nothing is selected; null cursor when no
    // document.
    QTextCursor targetCursor() const;
    void merge(const QTextCharFormat &format);

    QPointer<QQuickTextDocument> m_editorDocument;
    QPointer<QTextDocument> m_document;
    int m_selectionStart = 0;
    int m_selectionEnd = 0;
    int m_cursorPosition = 0;
};
