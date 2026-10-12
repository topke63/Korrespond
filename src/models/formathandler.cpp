// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "formathandler.h"

#include <QQuickTextDocument>
#include <QTextDocument>

#include <algorithm>

FormatHandler::FormatHandler(QObject *parent)
    : QObject(parent)
{
}

QTextDocument *FormatHandler::document() const
{
    return m_document;
}

void FormatHandler::setDocument(QTextDocument *document)
{
    if (m_document == document) {
        return;
    }
    m_document = document;
    Q_EMIT documentChanged();
    Q_EMIT formatChanged();
}

QQuickTextDocument *FormatHandler::editorDocument() const
{
    return m_editorDocument;
}

void FormatHandler::setEditorDocument(QQuickTextDocument *editorDocument)
{
    m_editorDocument = editorDocument;
    setDocument(editorDocument ? editorDocument->textDocument() : nullptr);
}

int FormatHandler::selectionStart() const
{
    return m_selectionStart;
}

void FormatHandler::setSelectionStart(int position)
{
    if (m_selectionStart != position) {
        m_selectionStart = position;
        Q_EMIT selectionChanged();
        Q_EMIT formatChanged();
    }
}

int FormatHandler::selectionEnd() const
{
    return m_selectionEnd;
}

void FormatHandler::setSelectionEnd(int position)
{
    if (m_selectionEnd != position) {
        m_selectionEnd = position;
        Q_EMIT selectionChanged();
        Q_EMIT formatChanged();
    }
}

int FormatHandler::cursorPosition() const
{
    return m_cursorPosition;
}

void FormatHandler::setCursorPosition(int position)
{
    if (m_cursorPosition != position) {
        m_cursorPosition = position;
        Q_EMIT selectionChanged();
        Q_EMIT formatChanged();
    }
}

QTextCursor FormatHandler::targetCursor() const
{
    if (!m_document) {
        return {};
    }
    const int last = std::max(0, m_document->characterCount() - 1);
    QTextCursor cursor(m_document);
    const int start = std::clamp(std::min(m_selectionStart, m_selectionEnd), 0, last);
    const int end = std::clamp(std::max(m_selectionStart, m_selectionEnd), 0, last);
    if (start != end) {
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
    } else {
        cursor.setPosition(std::clamp(m_cursorPosition, 0, last));
        cursor.select(QTextCursor::WordUnderCursor);
    }
    return cursor;
}

void FormatHandler::merge(const QTextCharFormat &format)
{
    QTextCursor cursor = targetCursor();
    if (cursor.isNull() || !cursor.hasSelection()) {
        return;
    }
    cursor.mergeCharFormat(format);
    Q_EMIT formatChanged();
}

// The style is "on" when the first selected character has it; the cursor at the end of a word counts
// the character before it (that is what the user is looking at).
static QTextCharFormat formatAt(const QTextCursor &cursor)
{
    return cursor.charFormat();
}

bool FormatHandler::bold() const
{
    const QTextCursor cursor = targetCursor();
    return !cursor.isNull() && formatAt(cursor).fontWeight() >= QFont::Bold;
}

void FormatHandler::setBold(bool on)
{
    QTextCharFormat format;
    format.setFontWeight(on ? QFont::Bold : QFont::Normal);
    merge(format);
}

bool FormatHandler::italic() const
{
    const QTextCursor cursor = targetCursor();
    return !cursor.isNull() && formatAt(cursor).fontItalic();
}

void FormatHandler::setItalic(bool on)
{
    QTextCharFormat format;
    format.setFontItalic(on);
    merge(format);
}

bool FormatHandler::underline() const
{
    const QTextCursor cursor = targetCursor();
    return !cursor.isNull() && formatAt(cursor).fontUnderline();
}

void FormatHandler::setUnderline(bool on)
{
    QTextCharFormat format;
    format.setFontUnderline(on);
    merge(format);
}

bool FormatHandler::strikeOut() const
{
    const QTextCursor cursor = targetCursor();
    return !cursor.isNull() && formatAt(cursor).fontStrikeOut();
}

void FormatHandler::setStrikeOut(bool on)
{
    QTextCharFormat format;
    format.setFontStrikeOut(on);
    merge(format);
}

void FormatHandler::setPlainText(const QString &text)
{
    if (m_document) {
        m_document->setPlainText(text);
        Q_EMIT formatChanged();
    }
}
