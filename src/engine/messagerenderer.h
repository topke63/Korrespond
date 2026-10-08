// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QColor>
#include <QString>
#include <QStringList>

namespace KMime
{
class Message;
}

// Content-Security-Policy put in front of every rendered message: no scripts, no remote
// resources of any kind; only inline styles and embedded (data:/cid:) images.
inline constexpr auto messageContentSecurityPolicy = "default-src 'none'; style-src 'unsafe-inline'; img-src data: cid:";

// Turns a message body into HTML that is safe to show in a web view.
// Uses the text/html part if the message has one, otherwise the text/plain part, escaped,
// with http(s) URLs made clickable and shown in the given colors.
QString messageBodyHtml(const KMime::Message &msg, const QColor &text, const QColor &background);

// Display names of the message's attachments, in order: the Content-Disposition filename,
// else the Content-Type name, else a translated "Unnamed attachment".
QStringList attachmentNames(const KMime::Message &msg);
