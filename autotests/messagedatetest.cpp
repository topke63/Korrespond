// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KMime/Message>

#include <QTest>
#include <QTimeZone>

#include "imapclient.h"

using namespace Qt::StringLiterals;

class MessageDateTest : public QObject
{
    Q_OBJECT

private:
    static QDateTime dateOf(const QByteArray &head)
    {
        KMime::Message message;
        message.setContent(head + "\n\nBody\n");
        message.parse();
        return messageDate(message);
    }

    static QDateTime utc(int day, int hour, int minute, int second)
    {
        return QDateTime(QDate(2026, 10, day), QTime(hour, minute, second), QTimeZone::UTC);
    }

private Q_SLOTS:
    void testThreadingHeaders()
    {
        KMime::Message message;
        message.setContent(
            "Message-ID: <c@example.org>\n"
            "In-Reply-To: <b@example.org>\n"
            "References: <a@example.org>\n <b@example.org> <a@example.org>\n"
            "Subject: Re: Hi\n\nBody\n");
        message.parse();
        MessageSummary summary;
        fillThreadingHeaders(message, summary);
        QCOMPARE(summary.messageId, u"c@example.org"_s);
        QCOMPARE(summary.inReplyTo, u"b@example.org"_s);
        QCOMPARE(summary.references, (QStringList{u"a@example.org"_s, u"b@example.org"_s}));
        QCOMPARE(summary.gmThreadId, qint64(0));
    }

    void testGmailThreadId()
    {
        QCOMPARE(gmailThreadId(QVariant(u"1812345678901234567"_s)), qint64(1812345678901234567));
        QCOMPARE(gmailThreadId(QVariant(QByteArray("42"))), qint64(42));
        QCOMPARE(gmailThreadId(QVariant(qint64(7))), qint64(7));
        QCOMPARE(gmailThreadId(QVariant()), qint64(0));
        QCOMPARE(gmailThreadId(QVariant(u"abc"_s)), qint64(0));
        QCOMPARE(gmailThreadId(QVariant(u"-5"_s)), qint64(0));
    }

    void testThreadingHeadersMissing()
    {
        KMime::Message message;
        message.setContent("Subject: Hi\n\nBody\n");
        message.parse();
        MessageSummary summary;
        summary.messageId = u"old"_s;
        fillThreadingHeaders(message, summary);
        QVERIFY(summary.messageId.isEmpty());
        QVERIFY(summary.inReplyTo.isEmpty());
        QVERIFY(summary.references.isEmpty());
    }

    void testDateHeader()
    {
        const QDateTime date = dateOf(
            "Received: from a.example by b.example; Thu, 8 Oct 2026 11:30:40 +0000\n"
            "Date: Thu, 8 Oct 2026 13:30:12 +0200\n"
            "Subject: Hi");
        QCOMPARE(date.toUTC(), utc(8, 11, 30, 12)); // Date wins over Received
    }

    void testNewestReceivedWithoutDate()
    {
        // Yahoo webmail mail: no Date; the first Received header is the newest hop.
        const QDateTime date = dateOf(
            "Received: from mx.example\n by inbox.example with SMTP; Thu, 8 Oct 2026 11:30:33 +0000 (UTC)\n"
            "Received: from out.example by mx.example; Thu, 8 Oct 2026 11:30:30 +0000\n"
            "Subject: Hi");
        QCOMPARE(date.toUTC(), utc(8, 11, 30, 33));
    }

    void testSkipsUnparsableReceived()
    {
        const QDateTime date = dateOf(
            "Received: by local.example (no date here)\n"
            "Received: from x by y; garbage\n"
            "Received: from out.example by mx.example; Wed, 7 Oct 2026 09:00:00 -0400\n"
            "Subject: Hi");
        QCOMPARE(date.toUTC(), utc(7, 13, 0, 0));
    }

    void testNoDate()
    {
        QVERIFY(!dateOf("From: me@example.org\nSubject: Sent from webmail").isValid());
    }
};

QTEST_GUILESS_MAIN(MessageDateTest)

#include "messagedatetest.moc"
