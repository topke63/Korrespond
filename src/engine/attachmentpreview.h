// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>

// What the attachment preview window can show. Pure functions, no I/O. Whether a file is risky to
// open is decided elsewhere (isRiskyAttachment); the caller asks that first.
namespace AttachmentPreview
{
enum class Kind {
    Image,
    Pdf,
    Text,
    Audio,
    Video,
    None, // no preview: the window offers Open and Save As only
};

// The kind of preview for a file, from its MIME type; when the type is missing, generic
// (application/octet-stream) or not one of the known ones, from the file suffix.
Kind kindFor(const QString &fileName, const QString &mimeType);

// "image", "pdf", "text", "audio", "video" or "none" (what QML reads).
QString kindName(Kind kind);

struct TextPreview {
    QString text;
    bool truncated = false; // the file was longer than the limit
};

// The text of a file for the preview: at most `maxBytes` of it (cut on a character boundary),
// decoded as UTF-8 (a BOM is dropped) or, when the bytes are not valid UTF-8, as Latin-1. Control
// characters other than tab and newline (NUL included) become U+FFFD; line ends are \n.
TextPreview textFor(const QByteArray &data, qsizetype maxBytes = 1024 * 1024);
}
