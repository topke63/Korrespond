// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagerenderer.h"

#include <KLocalizedString>
#include <KMime/Message>

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRegularExpression>
#include <QTextDocumentFragment>

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

QString messagePreview(const KMime::Message &msg, int maxLength)
{
    QString text;
    const KMime::Content *plain = msg.mainBodyPart("text/plain");
    if (plain && (!plain->contentType() || plain->contentType()->isPlainText())) {
        text = plain->decodedText();
    } else if (const KMime::Content *html = msg.mainBodyPart("text/html"); html && html->contentType() && html->contentType()->isHTMLText()) {
        // Only parses; QTextDocumentFragment never loads the mail's resources.
        text = QTextDocumentFragment::fromHtml(html->decodedText()).toPlainText();
    } else if (const KMime::Content *content = msg.textContent()) {
        text = content->decodedText();
    }

    // Images in HTML become U+FFFC; mailing lists hide zero-width "preheader" padding.
    text.removeIf([](QChar c) {
        return c == QChar::ObjectReplacementCharacter || c.category() == QChar::Other_Format;
    });

    QString preview;
    const QStringList lines = text.split(u'\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(u'>')) {
            continue;
        }
        if (!preview.isEmpty()) {
            preview += u' ';
        }
        preview += trimmed.simplified();
        if (preview.size() > maxLength) {
            break;
        }
    }
    if (preview.size() > maxLength) {
        preview = preview.left(maxLength - 1).trimmed() + u'…';
    }
    return preview;
}

QStringList attachmentNames(const KMime::Message &msg)
{
    QStringList names;
    for (const KMime::Content *part : msg.attachments()) {
        QString name;
        if (const auto *disposition = part->contentDisposition()) {
            name = disposition->filename();
        }
        if (name.isEmpty()) {
            if (const auto *type = part->contentType()) {
                name = type->name();
            }
        }
        names.append(name.isEmpty() ? i18nc("@info placeholder for a mail attachment without a file name", "Unnamed attachment") : name);
    }
    return names;
}

QString safeAttachmentFileName(const QString &raw)
{
    QString name = raw.mid(raw.lastIndexOf(u'/') + 1);
    name = name.mid(name.lastIndexOf(u'\\') + 1);
    QString cleaned;
    for (const QChar c : std::as_const(name)) {
        if (c.category() != QChar::Other_Control) {
            cleaned.append(c);
        }
    }
    cleaned = cleaned.trimmed();
    if (cleaned.isEmpty() || cleaned == u"."_s || cleaned == u".."_s) {
        return u"attachment"_s;
    }
    return cleaned;
}

QList<AttachmentData> attachmentParts(const KMime::Message &msg)
{
    QList<AttachmentData> result;
    for (const KMime::Content *part : msg.attachments()) {
        QString name;
        if (const auto *disposition = part->contentDisposition()) {
            name = disposition->filename();
        }
        if (name.isEmpty()) {
            if (const auto *type = part->contentType()) {
                name = type->name();
            }
        }
        QString mime;
        if (const auto *type = part->contentType()) {
            mime = QString::fromLatin1(type->mimeType());
        }
        if (mime.isEmpty()) {
            mime = u"application/octet-stream"_s;
        }
        result.append(AttachmentData{safeAttachmentFileName(name), mime, part->decodedBody()});
    }
    return result;
}

namespace
{
// The lists of what counts as risky to open: file suffixes (lower case) and MIME types (the type
// itself or any of its parents).
const QStringList &riskySuffixes()
{
    static const QStringList list{u"sh"_s,  u"bash"_s, u"zsh"_s,  u"csh"_s,        u"ksh"_s,     u"fish"_s, u"desktop"_s,  u"exe"_s, u"bat"_s,
                                  u"cmd"_s, u"com"_s,  u"scr"_s,  u"pif"_s,        u"msi"_s,     u"jar"_s,  u"appimage"_s, u"run"_s, u"bin"_s,
                                  u"py"_s,  u"pyw"_s,  u"pl"_s,   u"rb"_s,         u"php"_s,     u"js"_s,   u"mjs"_s,      u"vbs"_s, u"vbe"_s,
                                  u"wsf"_s, u"ps1"_s,  u"psm1"_s, u"lnk"_s,        u"reg"_s,     u"hta"_s,  u"cpl"_s,      u"dll"_s, u"so"_s,
                                  u"deb"_s, u"rpm"_s,  u"apk"_s,  u"flatpakref"_s, u"command"_s, u"app"_s};
    return list;
}

const QStringList &riskyMimeTypes()
{
    static const QStringList list{u"application/x-executable"_s,
                                  u"application/x-sharedlib"_s,
                                  u"application/x-shellscript"_s,
                                  u"application/x-desktop"_s,
                                  u"application/x-msdownload"_s,
                                  u"application/x-ms-dos-executable"_s,
                                  u"application/x-msi"_s,
                                  u"application/java-archive"_s,
                                  u"application/vnd.appimage"_s,
                                  u"text/x-python"_s,
                                  u"text/x-perl"_s,
                                  u"application/javascript"_s,
                                  u"text/javascript"_s,
                                  u"application/x-perl"_s,
                                  u"application/x-ruby"_s,
                                  u"application/x-php"_s};
    return list;
}

bool mimeIsRisky(const QMimeType &type)
{
    if (!type.isValid()) {
        return false;
    }
    for (const QString &risky : riskyMimeTypes()) {
        if (type.inherits(risky)) {
            return true;
        }
    }
    return false;
}
}

bool isRiskyAttachment(const AttachmentData &part)
{
    // The last suffix decides: "x.exe.txt" is text, "x.pdf.exe" is an executable.
    const QString suffix = QFileInfo(part.name).suffix().toLower();
    if (riskySuffixes().contains(suffix)) {
        return true;
    }
    // The declared type, then the type from the name and the content (a script without a suffix is
    // found by its "#!" line), then the type from the name alone.
    QMimeDatabase db;
    if (mimeIsRisky(db.mimeTypeForName(part.mimeType.trimmed().toLower()))) {
        return true;
    }
    if (mimeIsRisky(db.mimeTypeForFileNameAndData(part.name, part.data))) {
        return true;
    }
    return mimeIsRisky(db.mimeTypeForFile(part.name, QMimeDatabase::MatchExtension));
}

bool isStaleAttachmentFolder(const QString &name, qint64 ageSeconds, const std::function<bool(qint64 pid)> &pidAlive)
{
    static const QRegularExpression pidPattern(u"^plasma-mail-([0-9]{1,10})-[A-Za-z0-9]{6}$"_s);
    static const QRegularExpression oldPattern(u"^plasma-mail-[A-Za-z0-9]{6}$"_s);
    const QRegularExpressionMatch match = pidPattern.match(name);
    if (match.hasMatch()) {
        bool ok = false;
        const qint64 pid = match.captured(1).toLongLong(&ok);
        return ok && pid > 0 && !pidAlive(pid);
    }
    return oldPattern.match(name).hasMatch() && ageSeconds > 24 * 3600;
}
