// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagerenderer.h"

#include <KMime/Message>

#include <QRegularExpression>

using namespace Qt::StringLiterals;

static QString cspMeta()
{
    return u"<meta http-equiv=\"Content-Security-Policy\" content=\""_s + QLatin1StringView(messageContentSecurityPolicy) + u"\">"_s;
}

// Expects already-escaped text, so a URL ends at whitespace or at an escaped quote or bracket.
static QString linkify(const QString &escaped)
{
    static const QRegularExpression url(u"https?://(?:(?!&(?:quot|lt|gt);)\\S)+"_s, QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression trailingPunctuation(u"[.,;:!?)\\]]+$"_s);

    QString result;
    qsizetype last = 0;
    QRegularExpressionMatchIterator it = url.globalMatch(escaped);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        QString link = match.captured();
        // "see https://example.com." – the dot ends the sentence, not the URL.
        const QRegularExpressionMatch tail = trailingPunctuation.match(link);
        if (tail.hasMatch()) {
            link.chop(tail.capturedLength());
        }
        result += QStringView(escaped).mid(last, match.capturedStart() - last);
        result += u"<a href=\""_s + link + u"\">"_s + link + u"</a>"_s;
        last = match.capturedStart() + link.size();
    }
    result += QStringView(escaped).mid(last);
    return result;
}

static QString plainTextHtml(const QString &text, const QColor &textColor, const QColor &background)
{
    const QString fg = textColor.name(QColor::HexRgb);
    const QString bg = background.name(QColor::HexRgb);
    return u"<!DOCTYPE html><html><head>"_s + cspMeta() + u"<style>body{margin:0;padding:8px;color:"_s + fg + u";background:"_s + bg
        + u";}a{color:inherit;}</style></head><body><div style=\"white-space:pre-wrap;overflow-wrap:anywhere;font-family:sans-serif;\">"_s
        + linkify(text.toHtmlEscaped()) + u"</div></body></html>"_s;
}

QString messageBodyHtml(const KMime::Message &msg, const QColor &text, const QColor &background)
{
    const KMime::Content *html = msg.mainBodyPart("text/html");
    if (html && html->contentType() && html->contentType()->isHTMLText()) {
        // A <meta> before the mail's own markup is placed into <head> by the HTML parser,
        // so the policy applies before any of the mail's resources are requested.
        return cspMeta() + html->decodedText();
    }

    const KMime::Content *plain = msg.mainBodyPart("text/plain");
    // No Content-Type header means text/plain (RFC 2045).
    if (!plain || (plain->contentType() && !plain->contentType()->isPlainText())) {
        plain = msg.textContent();
    }
    return plainTextHtml(plain ? plain->decodedText() : QString(), text, background);
}
