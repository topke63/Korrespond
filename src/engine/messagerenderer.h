// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

namespace KMime
{
class Message;
}

// Content-Security-Policy put in front of every rendered message: no scripts, no remote
// resources of any kind; only inline styles and embedded (data:/cid:) images.
// With allowRemoteImages, https: is added to img-src (and nothing else), for one render the user asked for.
QString messageContentSecurityPolicy(bool allowRemoteImages = false);

// True if the HTML refers to a remote image: <img>/<source>/<input type=image> src or srcset,
// a background attribute, or CSS url(), pointing at http(s):// or //host. cid: and data: do not count.
// A simple scan; false positives only show the "load images" bar.
bool htmlHasRemoteImages(const QString &html);

// Turns a message body into HTML that is safe to show in a web view.
// Uses the text/html part if the message has one, otherwise the text/plain part, escaped,
// with http(s) URLs made clickable and shown in the given colors.
// HTML mail without colours of its own gets the theme colours (and the link colour, made readable
// on the background); HTML mail that sets its own colours is shown on a white page.
// True when the message's HTML part refers to remote images; false for plain-text-only mail.
bool messageHasRemoteImages(const KMime::Message &msg);

// Length of the leading DOCTYPE token of a mail's HTML, counted from the start of the string
// (including an optional BOM and ASCII whitespace before it); 0 if the HTML does not start with
// exactly such a token: `<!DOCTYPE html ...>` with no '<' or '>' inside. Anything else in front
// (comment, tag, text, NUL) gives 0. Only then does messageBodyHtml() emit its own DOCTYPE first.
int leadingDoctypeLength(const QString &html);

QString messageBodyHtml(const KMime::Message &msg, const QColor &text, const QColor &background, const QColor &link, bool allowRemoteImages = false);

// True if the HTML sets its own text or background colours: a CSS color / background /
// background-color property (not border-color etc.), a bgcolor attribute, text/link/vlink/alink/
// background on <body>, or <font color=...>.
bool htmlHasOwnColours(const QString &html);

// WCAG contrast ratio (1..21) between two colours.
double contrastRatio(const QColor &a, const QColor &b);

// The link colour if it has a contrast of at least 4.5:1 on the background, else a lighter or
// darker variant of it that does.
QColor readableLinkColour(const QColor &link, const QColor &background);

// A short plain-text excerpt of the body for the message list: the text/plain part, else the
// text of the HTML part; quoted ("> ") lines skipped, whitespace collapsed, at most maxLength
// characters (an ellipsis marks a cut). Empty when there is no text.
QString messagePreview(const KMime::Message &msg, int maxLength = 150);

// Display names of the message's attachments, in order: the Content-Disposition filename,
// else the Content-Type name, else a translated "Unnamed attachment".
QStringList attachmentNames(const KMime::Message &msg);

// A file name that is safe to use inside a chosen folder: only the last path component (split on
// '/' and '\\'), control characters removed, trimmed. "." , ".." and empty become "attachment".
QString safeAttachmentFileName(const QString &raw);

struct AttachmentData {
    QString name; // already passed through safeAttachmentFileName()
    QString mimeType; // application/octet-stream when the part has none
    QByteArray data; // decoded content
};

// The message's attachments with their decoded content, in the same order and with the same
// part selection as attachmentNames().
QList<AttachmentData> attachmentParts(const KMime::Message &msg);

// True if opening the attachment could run code: its file suffix (the last one, case-insensitive)
// is an executable or script suffix, or its declared or detected MIME type is one of those kinds.
// "report.exe.pdf" is judged by ".pdf" and is not risky.
bool isRiskyAttachment(const AttachmentData &part);

// Decides whether a folder of the temp directory is a leftover of an earlier run, from its name
// ("korrespond-<pid>-XXXXXX", or the older "korrespond-XXXXXX" without a pid, X = 6 letters or
// digits) and age in seconds. Pid folders are stale when that process is gone; old-style ones
// when older than a day. Anything else is never stale.
bool isStaleAttachmentFolder(const QString &name, qint64 ageSeconds, const std::function<bool(qint64 pid)> &pidAlive);
