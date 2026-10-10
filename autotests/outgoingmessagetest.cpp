// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KMime/Message>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "outgoingmessage.h"

using namespace Qt::StringLiterals;

static const QDateTime now(QDate(2026, 10, 9), QTime(12, 30), QTimeZone::UTC);

static OutgoingMessage sample()
{
    OutgoingMessage message;
    message.fromName = u"Miroslav Topalović"_s;
    message.fromAddress = u"topke63@yahoo.com"_s;
    message.to = {u"alice@example.org"_s, u"Bob Jones <bob@example.org>"_s};
    message.cc = {u"carol@example.org"_s};
    message.bcc = {u"secret@example.org"_s};
    message.subject = u"Čaj za dvoje"_s;
    message.body = u"Zdravo,\nsee you at 5 = fine?"_s;
    return message;
}

// Parses the built bytes back the way the app parses received mail.
static std::unique_ptr<KMime::Message> parse(const QByteArray &data)
{
    auto message = std::make_unique<KMime::Message>();
    message->setContent(KMime::CRLFtoLF(data));
    message->parse();
    return message;
}

class OutgoingMessageTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testHeadersAndRecipients()
    {
        const OutgoingMessage::Built built = sample().build(now);
        const auto message = parse(built.data);

        QCOMPARE(message->from()->asUnicodeString(), u"Miroslav Topalović <topke63@yahoo.com>"_s);
        QCOMPARE(message->subject()->asUnicodeString(), u"Čaj za dvoje"_s);
        QCOMPARE(message->to()->addresses(), (QList<QByteArray>{"alice@example.org", "bob@example.org"}));
        QCOMPARE(message->cc()->addresses(), (QList<QByteArray>{"carol@example.org"}));
        QVERIFY(message->date()->dateTime().isValid());
        QCOMPARE(message->date()->dateTime().toUTC(), now);
        QVERIFY(message->messageID()->identifier().endsWith("@yahoo.com"));
        QCOMPARE(message->contentType()->mimeType(), QByteArray("text/plain"));
        QCOMPARE(message->contentType()->charset(), QByteArray("utf-8"));

        // The envelope has everyone, bare; the Bcc header is not in the message.
        QCOMPARE(built.recipients, (QStringList{u"alice@example.org"_s, u"bob@example.org"_s, u"carol@example.org"_s, u"secret@example.org"_s}));
        QVERIFY(!built.data.contains("secret@example.org"));
        QVERIFY(!built.data.toLower().contains("\r\nbcc:"));
        QVERIFY(built.data.contains("MIME-Version: 1.0"));
    }

    void testBodyRoundTrip()
    {
        const OutgoingMessage::Built built = sample().build(now);
        const auto message = parse(built.data);
        QCOMPARE(message->decodedText(), u"Zdravo,\nsee you at 5 = fine?"_s);
    }

    void testBodyWithNonAsciiAndLongLines()
    {
        OutgoingMessage message = sample();
        message.body = u"Čaj je gotov 😀\n"_s + QString(300, u'x') + u"\n\nPozdrav,\nMiroslav"_s;
        const OutgoingMessage::Built built = message.build(now);
        QCOMPARE(parse(built.data)->decodedText(), message.body);
        // Over the wire: short lines only.
        for (const QByteArray &line : built.data.split('\n')) {
            QVERIFY(line.size() <= 78);
        }
    }

    void testLineEndingsAreCrLf()
    {
        const QByteArray data = sample().build(now).data;
        QVERIFY(data.contains("\r\n"));
        // No LF without a CR before it.
        for (qsizetype i = 0; i < data.size(); ++i) {
            if (data.at(i) == '\n') {
                QVERIFY2(i > 0 && data.at(i - 1) == '\r', qPrintable(u"bare LF at %1"_s.arg(i)));
            }
        }
        // Nothing but 7-bit characters goes on the wire.
        for (const char c : data) {
            QVERIFY(static_cast<unsigned char>(c) < 128);
        }
    }

    void testReplyHeaders()
    {
        OutgoingMessage message = sample();
        message.inReplyTo = u"<orig-3@example.org>"_s;
        message.references = {u"root-1@example.org"_s, u"<mid-2@example.org>"_s, u"orig-3@example.org"_s};
        const auto parsed = parse(message.build(now).data);
        QCOMPARE(parsed->inReplyTo()->identifiers(), (QList<QByteArray>{"orig-3@example.org"}));
        QCOMPARE(parsed->references()->identifiers(), (QList<QByteArray>{"root-1@example.org", "mid-2@example.org", "orig-3@example.org"}));
    }

    void testNoReplyHeadersForANewMessage()
    {
        const QByteArray data = sample().build(now).data.toLower();
        QVERIFY(!data.contains("in-reply-to:"));
        QVERIFY(!data.contains("references:"));
    }

    void testSenderWithoutNameAndWithoutRecipientsOfAKind()
    {
        OutgoingMessage message;
        message.fromAddress = u"me@example.org"_s;
        message.to = {u"you@example.org"_s};
        message.subject = u"Hi"_s;
        message.body = u"Body"_s;
        const OutgoingMessage::Built built = message.build(now);
        const auto parsed = parse(built.data);
        QCOMPARE(parsed->from()->asUnicodeString(), u"me@example.org"_s);
        QVERIFY(static_cast<const KMime::Message *>(parsed.get())->cc() == nullptr);
        QCOMPARE(built.recipients, (QStringList{u"you@example.org"_s}));
    }

    void testSeveralAddressesInOneField()
    {
        OutgoingMessage message = sample();
        message.to = {u"a@example.org, b@example.org"_s};
        message.cc.clear();
        message.bcc.clear();
        const OutgoingMessage::Built built = message.build(now);
        QCOMPARE(built.recipients, (QStringList{u"a@example.org"_s, u"b@example.org"_s}));
    }

    void testNoAttachmentsIsNotMultipart()
    {
        const auto parsed = parse(sample().build(now).data);
        QCOMPARE(parsed->contentType()->mimeType(), QByteArray("text/plain"));
        QVERIFY(parsed->attachments().isEmpty());
    }

    void testAttachments()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QByteArray binary;
        for (int i = 0; i < 256; ++i) {
            binary.append(static_cast<char>(i));
        }
        const QString textName = u"résumé ü.txt"_s;
        const QByteArray text = "Hello, résumé\n";
        const auto write = [&](const QString &name, const QByteArray &bytes) {
            QFile file(dir.filePath(name));
            Q_ASSERT(file.open(QIODevice::WriteOnly));
            file.write(bytes);
            return file.fileName();
        };
        OutgoingMessage message = sample();
        message.attachments = {write(textName, text), write(u"data.bin"_s, binary)};
        const OutgoingMessage::Built built = message.build(now);
        QVERIFY2(built.error.isEmpty(), qPrintable(built.error));
        for (const char c : built.data) {
            QVERIFY(static_cast<unsigned char>(c) < 128);
        }

        const auto parsed = parse(built.data);
        QCOMPARE(parsed->contentType()->mimeType(), QByteArray("multipart/mixed"));
        QVERIFY(!parsed->contentType()->boundary().isEmpty());
        QVERIFY(parsed->textContent());
        QCOMPARE(parsed->textContent()->decodedText(), u"Zdravo,\nsee you at 5 = fine?"_s);
        const auto attachments = parsed->attachments();
        QCOMPARE(attachments.size(), 2);
        QCOMPARE(attachments.at(0)->contentDisposition()->filename(), textName);
        QCOMPARE(attachments.at(0)->contentType()->mimeType(), QByteArray("text/plain"));
        QCOMPARE(attachments.at(0)->decodedBody(), text);
        QCOMPARE(attachments.at(1)->contentDisposition()->filename(), u"data.bin"_s);
        QCOMPARE(attachments.at(1)->contentType()->mimeType(), QByteArray("application/octet-stream"));
        QCOMPARE(attachments.at(1)->decodedBody(), binary);
        QCOMPARE(built.recipients.size(), 4);
    }

    void testMissingAttachment()
    {
        OutgoingMessage message = sample();
        message.attachments = {u"/nonexistent-dir/secret name.pdf"_s};
        const OutgoingMessage::Built built = message.build(now);
        QVERIFY(built.data.isEmpty());
        QVERIFY(built.error.contains(u"secret name.pdf"_s));
        QVERIFY(!built.error.contains(u"nonexistent-dir"_s));
    }

    void testTooLarge()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(u"big.bin"_s));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.resize(OutgoingMessage::MaxAttachmentBytes + 1)); // sparse
        file.close();
        OutgoingMessage message = sample();
        message.attachments = {file.fileName()};
        const OutgoingMessage::Built built = message.build(now);
        QVERIFY(built.data.isEmpty());
        QVERIFY(built.error.contains(u"25 MB"_s));
    }
};

QTEST_GUILESS_MAIN(OutgoingMessageTest)

#include "outgoingmessagetest.moc"
