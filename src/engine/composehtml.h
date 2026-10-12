// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

class QTextDocument;

// The HTML for a message the user formatted in the editor. Only bold, italic, underline and
// strike-through survive; colours, fonts, links and everything else in the document (for instance
// from pasted web text) are dropped.
namespace ComposeHtml
{
struct Result {
    QString html;
    // True when at least one piece of text is bold, italic, underlined or struck through; when
    // false the message is better sent as plain text only.
    bool hasFormatting = false;
};

// One <div> per paragraph (an empty one is <div><br></div>), <br> for line breaks inside it,
// <b> <i> <u> <s> around the styled text, text escaped, runs of spaces kept with &nbsp;.
Result fromDocument(const QTextDocument &document);
}
