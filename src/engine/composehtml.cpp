// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "composehtml.h"

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>

using namespace Qt::StringLiterals;

// Escapes `text` and keeps spaces that HTML would collapse (leading, or followed by another space).
static QString escaped(const QString &text, bool atBlockStart)
{
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == u' ') {
            const bool collapses = (i == 0 && atBlockStart) || (i + 1 < text.size() && text.at(i + 1) == u' ');
            result += collapses ? u"&nbsp;"_s : u" "_s;
        } else if (c == QChar::LineSeparator) {
            result += u"<br>"_s;
        } else if (c == u'\t') {
            result += u"&nbsp;&nbsp;&nbsp;&nbsp;"_s;
        } else if (c == u'<') {
            result += u"&lt;"_s;
        } else if (c == u'>') {
            result += u"&gt;"_s;
        } else if (c == u'&') {
            result += u"&amp;"_s;
        } else if (c == u'"') {
            result += u"&quot;"_s;
        } else if (c == QChar::Nbsp) {
            result += u"&nbsp;"_s;
        } else {
            result += c;
        }
    }
    return result;
}

ComposeHtml::Result ComposeHtml::fromDocument(const QTextDocument &document)
{
    Result result;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        QString line;
        bool atStart = true;
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.length() == 0) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            const bool bold = format.fontWeight() >= QFont::Bold;
            const bool italic = format.fontItalic();
            // Qt underlines link text itself; that is the link's look, not the user's choice.
            const bool underline = format.fontUnderline() && !format.isAnchor();
            const bool strike = format.fontStrikeOut();
            QString text = escaped(fragment.text(), atStart);
            atStart = false;
            if (bold) {
                text = u"<b>"_s + text + u"</b>"_s;
            }
            if (italic) {
                text = u"<i>"_s + text + u"</i>"_s;
            }
            if (underline) {
                text = u"<u>"_s + text + u"</u>"_s;
            }
            if (strike) {
                text = u"<s>"_s + text + u"</s>"_s;
            }
            result.hasFormatting = result.hasFormatting || bold || italic || underline || strike;
            line += text;
        }
        result.html += line.isEmpty() ? u"<div><br></div>"_s : u"<div>"_s + line + u"</div>"_s;
    }
    return result;
}
