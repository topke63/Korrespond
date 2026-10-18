// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "attachmentpreview.h"

#include <QSet>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace
{
// Text-like application/* types that are plain text underneath.
const QSet<QString> textApplicationTypes = {
    u"application/json"_s,
    u"application/xml"_s,
    u"application/x-yaml"_s,
    u"application/yaml"_s,
    u"application/toml"_s,
    u"application/x-sh"_s, // shown as text only; a risky one never gets here
    u"application/javascript"_s,
    u"application/sql"_s,
    u"application/x-subrip"_s,
};

using AttachmentPreview::Kind;

Kind kindForSuffix(const QString &suffix)
{
    static const QSet<QString> images = {u"png"_s, u"jpg"_s, u"jpeg"_s, u"gif"_s, u"webp"_s, u"bmp"_s, u"svg"_s, u"ico"_s};
    static const QSet<QString> text = {u"txt"_s,   u"md"_s,   u"markdown"_s, u"csv"_s,  u"tsv"_s, u"log"_s, u"json"_s, u"xml"_s,  u"yaml"_s,
                                       u"yml"_s,   u"toml"_s, u"ini"_s,      u"conf"_s, u"cfg"_s, u"srt"_s, u"vtt"_s,  u"tex"_s,  u"diff"_s,
                                       u"patch"_s, u"c"_s,    u"h"_s,        u"cpp"_s,  u"hpp"_s, u"rs"_s,  u"go"_s,   u"java"_s, u"cs"_s,
                                       u"py"_s,    u"js"_s,   u"ts"_s,       u"sh"_s,   u"sql"_s, u"css"_s, u"html"_s, u"htm"_s,  u"qml"_s};
    static const QSet<QString> audio = {u"mp3"_s, u"ogg"_s, u"oga"_s, u"opus"_s, u"flac"_s, u"wav"_s, u"m4a"_s, u"aac"_s};
    static const QSet<QString> video = {u"mp4"_s, u"webm"_s, u"mkv"_s, u"mov"_s, u"m4v"_s, u"ogv"_s, u"avi"_s};
    if (images.contains(suffix)) {
        return Kind::Image;
    }
    if (suffix == "pdf"_L1) {
        return Kind::Pdf;
    }
    if (text.contains(suffix)) {
        return Kind::Text;
    }
    if (audio.contains(suffix)) {
        return Kind::Audio;
    }
    if (video.contains(suffix)) {
        return Kind::Video;
    }
    return Kind::None;
}

// By the declared type alone; None when it says nothing useful.
Kind kindForMime(const QString &mime)
{
    if (mime.startsWith(u"image/"_s)) {
        return Kind::Image;
    }
    if (mime == "application/pdf"_L1) {
        return Kind::Pdf;
    }
    if (mime.startsWith(u"text/"_s) || textApplicationTypes.contains(mime)) {
        return Kind::Text;
    }
    if (mime.startsWith(u"audio/"_s)) {
        return Kind::Audio;
    }
    if (mime.startsWith(u"video/"_s)) {
        return Kind::Video;
    }
    return Kind::None;
}
}

AttachmentPreview::Kind AttachmentPreview::kindFor(const QString &fileName, const QString &mimeType)
{
    const Kind byType = kindForMime(mimeType.trimmed().toLower());
    if (byType != Kind::None) {
        return byType;
    }
    const qsizetype dot = fileName.lastIndexOf(u'.');
    return dot >= 0 ? kindForSuffix(fileName.mid(dot + 1).toLower()) : Kind::None;
}

QString AttachmentPreview::kindName(Kind kind)
{
    switch (kind) {
    case Kind::Image:
        return u"image"_s;
    case Kind::Pdf:
        return u"pdf"_s;
    case Kind::Text:
        return u"text"_s;
    case Kind::Audio:
        return u"audio"_s;
    case Kind::Video:
        return u"video"_s;
    case Kind::None:
        break;
    }
    return u"none"_s;
}
