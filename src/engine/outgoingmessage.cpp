// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "outgoingmessage.h"

#include <KMime/Message>

#include <KLocalizedString>

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>

#include <memory>

using namespace Qt::StringLiterals;

static QString joined(const QStringList &list)
{
    return list.join(u", "_s);
}

static QStringList bareAddresses(const QList<QByteArray> &addresses)
{
    QStringList result;
    for (const QByteArray &address : addresses) {
        result.append(QString::fromLatin1(address));
    }
    return result;
}

OutgoingMessage::Built OutgoingMessage::build(const QDateTime &now) const
{
    KMime::Message message;
    // The sender: "Name <address>".
    message.from()->fromUnicodeString(fromName.isEmpty() ? fromAddress : u"%1 <%2>"_s.arg(fromName, fromAddress));
    if (!to.isEmpty()) {
        message.to()->fromUnicodeString(joined(to));
    }
    if (!cc.isEmpty()) {
        message.cc()->fromUnicodeString(joined(cc));
    }
    if (!bcc.isEmpty()) {
        message.bcc()->fromUnicodeString(joined(bcc));
    }
    message.subject()->fromUnicodeString(subject);
    message.date()->setDateTime(now);
    const qsizetype at = fromAddress.lastIndexOf(u'@');
    message.messageID()->generate(at >= 0 ? fromAddress.mid(at + 1).toLatin1() : QByteArray("localhost"));

    const auto ident = [](const QString &id) {
        QByteArray bytes = id.trimmed().toLatin1();
        if (bytes.startsWith('<')) {
            bytes.remove(0, 1);
        }
        if (bytes.endsWith('>')) {
            bytes.chop(1);
        }
        return bytes;
    };
    if (!inReplyTo.trimmed().isEmpty()) {
        message.inReplyTo()->appendIdentifier(ident(inReplyTo));
    }
    for (const QString &reference : references) {
        if (!reference.trimmed().isEmpty()) {
            message.references()->appendIdentifier(ident(reference));
        }
    }

    // Read the attachments first, so a bad one fails before any work.
    struct Attachment {
        QString name;
        QByteArray mimeType;
        QByteArray data;
    };
    QList<Attachment> files;
    qint64 total = 0; // bytes read so far
    for (const QString &path : attachments) {
        const QFileInfo info(path);
        if (total + info.size() > MaxAttachmentBytes) {
            Built failed;
            failed.error = i18n("Attachments are larger than 25 MB.");
            return failed;
        }
        QFile file(path);
        if (!info.isFile() || !file.open(QIODevice::ReadOnly)) {
            Built failed;
            failed.error = i18n("Could not read the attachment \"%1\".", info.fileName());
            return failed;
        }
        Attachment attachment;
        attachment.name = info.fileName();
        attachment.mimeType = QMimeDatabase().mimeTypeForFile(info).name().toLatin1();
        // Read at most one byte past the limit, so a file that grows or reports no size is still refused.
        attachment.data = file.read(MaxAttachmentBytes - total + 1);
        if (file.error() != QFile::NoError) {
            Built failed;
            failed.error = i18n("Could not read the attachment \"%1\".", info.fileName());
            return failed;
        }
        total += attachment.data.size();
        if (total > MaxAttachmentBytes) {
            Built failed;
            failed.error = i18n("Attachments are larger than 25 MB.");
            return failed;
        }
        files.append(attachment);
    }

    auto mimeVersion = std::make_unique<KMime::Headers::MIMEVersion>();
    mimeVersion->from7BitString("1.0");
    message.setHeader(std::move(mimeVersion));
    if (files.isEmpty() && !html.isEmpty()) {
        message.contentType()->setMimeType("multipart/alternative");
        message.contentType()->setBoundary(KMime::multiPartBoundary());
        // The plain text first: the last part is the preferred one.
        auto text = std::make_unique<KMime::Content>();
        text->contentType()->setMimeType("text/plain");
        text->contentType()->setCharset("utf-8");
        text->contentTransferEncoding()->setEncoding(KMime::Headers::CEquPr);
        text->setBody(body.toUtf8());
        message.appendContent(std::move(text));
        auto formatted = std::make_unique<KMime::Content>();
        formatted->contentType()->setMimeType("text/html");
        formatted->contentType()->setCharset("utf-8");
        formatted->contentTransferEncoding()->setEncoding(KMime::Headers::CEquPr);
        formatted->setBody(html.toUtf8());
        message.appendContent(std::move(formatted));
    } else if (files.isEmpty()) {
        message.contentType()->setMimeType("text/plain");
        message.contentType()->setCharset("utf-8");
        message.contentTransferEncoding()->setEncoding(KMime::Headers::CEquPr);
        // KMime encodes the body (quoted-printable) when it assembles the message.
        message.setBody(body.toUtf8());
    } else {
        message.contentType()->setMimeType("multipart/mixed");
        message.contentType()->setBoundary(KMime::multiPartBoundary());
        auto text = std::make_unique<KMime::Content>();
        text->contentType()->setMimeType("text/plain");
        text->contentType()->setCharset("utf-8");
        text->contentTransferEncoding()->setEncoding(KMime::Headers::CEquPr);
        text->setBody(body.toUtf8());
        if (html.isEmpty()) {
            message.appendContent(std::move(text));
        } else {
            // The text is one alternative group: plain first, then the HTML.
            auto alternative = std::make_unique<KMime::Content>();
            alternative->contentType()->setMimeType("multipart/alternative");
            alternative->contentType()->setBoundary(KMime::multiPartBoundary());
            alternative->appendContent(std::move(text));
            auto formatted = std::make_unique<KMime::Content>();
            formatted->contentType()->setMimeType("text/html");
            formatted->contentType()->setCharset("utf-8");
            formatted->contentTransferEncoding()->setEncoding(KMime::Headers::CEquPr);
            formatted->setBody(html.toUtf8());
            alternative->appendContent(std::move(formatted));
            message.appendContent(std::move(alternative));
        }
        for (const Attachment &attachment : files) {
            auto part = std::make_unique<KMime::Content>();
            part->contentType()->setMimeType(attachment.mimeType);
            part->contentType()->setName(attachment.name);
            part->contentTransferEncoding()->setEncoding(KMime::Headers::CEbase64);
            part->contentDisposition()->setDisposition(KMime::Headers::CDattachment);
            part->contentDisposition()->setFilename(attachment.name);
            part->setBody(attachment.data);
            message.appendContent(std::move(part));
        }
    }

    Built built;
    built.recipients = bareAddresses(message.to()->addresses()) + bareAddresses(message.cc()->addresses()) + bareAddresses(message.bcc()->addresses());
    // Bcc recipients are in the envelope only.
    message.removeHeader<KMime::Headers::Bcc>();
    message.assemble();
    built.data = message.encodedContent(KMime::NewlineType::CRLF);
    return built;
}
