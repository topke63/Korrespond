// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "outgoingmessage.h"

#include <KMime/Message>

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

    auto mimeVersion = std::make_unique<KMime::Headers::MIMEVersion>();
    mimeVersion->from7BitString("1.0");
    message.setHeader(std::move(mimeVersion));
    message.contentType()->setMimeType("text/plain");
    message.contentType()->setCharset("utf-8");
    message.contentTransferEncoding()->setEncoding(KMime::Headers::CEquPr);
    // KMime encodes the body (quoted-printable) when it assembles the message.
    message.setBody(body.toUtf8());

    Built built;
    built.recipients = bareAddresses(message.to()->addresses()) + bareAddresses(message.cc()->addresses()) + bareAddresses(message.bcc()->addresses());
    // Bcc recipients are in the envelope only.
    message.removeHeader<KMime::Headers::Bcc>();
    message.assemble();
    built.data = message.encodedContent(KMime::NewlineType::CRLF);
    return built;
}
