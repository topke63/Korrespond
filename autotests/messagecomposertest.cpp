// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KMime/Message>

#include <QTest>

#include "messagecomposer.h"

using namespace Qt::StringLiterals;
using namespace MessageComposer;

static std::unique_ptr<KMime::Message> parse(const QByteArray &raw)
{
    auto message = std::make_unique<KMime::Message>();
    message->setContent(KMime::CRLFtoLF(raw));
    message->parse();
    return message;
}

static QByteArray mail(const QByteArray &headers, const QByteArray &body = "Hello Bob,\nlunch?\n")
{
    return "Date: Thu, 08 Oct 2026 10:00:00 +0000\r\nMessage-ID: <orig-3@example.org>\r\nSubject: Lunch\r\nContent-Type: text/plain; charset=utf-8\r\n"
        + headers + "\r\n" + body;
}

class MessageComposerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testReplySubject_data()
    {
        QTest::addColumn<QString>("subject");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << u"Lunch"_s << u"Re: Lunch"_s;
        QTest::newRow("already Re") << u"Re: Lunch"_s << u"Re: Lunch"_s;
        QTest::newRow("capitals") << u"RE: Lunch"_s << u"RE: Lunch"_s;
        QTest::newRow("counted") << u"Re[2]: Lunch"_s << u"Re[2]: Lunch"_s;
        QTest::newRow("spaces") << u"  Lunch  "_s << u"Re: Lunch"_s;
        QTest::newRow("a word starting with re") << u"Reservation"_s << u"Re: Reservation"_s;
        QTest::newRow("empty") << QString() << u"Re: "_s;
    }

    void testReplySubject()
    {
        QFETCH(QString, subject);
        QFETCH(QString, expected);
        QCOMPARE(replySubject(subject), expected);
    }

    void testForwardSubject_data()
    {
        QTest::addColumn<QString>("subject");
        QTest::addColumn<QString>("expected");
        QTest::newRow("plain") << u"Lunch"_s << u"Fwd: Lunch"_s;
        QTest::newRow("already Fwd") << u"Fwd: Lunch"_s << u"Fwd: Lunch"_s;
        QTest::newRow("Fw") << u"FW: Lunch"_s << u"FW: Lunch"_s;
        QTest::newRow("a reply") << u"Re: Lunch"_s << u"Fwd: Re: Lunch"_s;
        QTest::newRow("a word starting with fw") << u"Fwoosh"_s << u"Fwd: Fwoosh"_s;
    }

    void testForwardSubject()
    {
        QFETCH(QString, subject);
        QFETCH(QString, expected);
        QCOMPARE(forwardSubject(subject), expected);
    }

    void testReplyGoesToTheSender()
    {
        const auto original =
            parse(mail("From: Alice <alice@example.org>\r\nTo: Bob <bob@example.org>, Carol <carol@example.org>\r\nCc: dave@example.org\r\n"));
        const Recipients r = replyRecipients(*original, {u"bob@example.org"_s}, false);
        QCOMPARE(r.to, (QStringList{u"Alice <alice@example.org>"_s}));
        QVERIFY(r.cc.isEmpty());
    }

    void testReplyToHeaderWins()
    {
        const auto original = parse(mail("From: Alice <alice@example.org>\r\nReply-To: list@example.org\r\nTo: bob@example.org\r\n"));
        QCOMPARE(replyRecipients(*original, {u"bob@example.org"_s}, false).to, (QStringList{u"list@example.org"_s}));
    }

    void testReplyAllLeavesOutMeAndRepeats()
    {
        const auto original =
            parse(mail("From: Alice <alice@example.org>\r\nTo: Bob <bob@example.org>, Carol <carol@example.org>, ALICE@example.org\r\nCc: dave@example.org, "
                       "CAROL@EXAMPLE.ORG, bob@example.org\r\n"));
        const Recipients r = replyRecipients(*original, {u"Bob@Example.org"_s}, true);
        QCOMPARE(r.to, (QStringList{u"Alice <alice@example.org>"_s, u"Carol <carol@example.org>"_s}));
        QCOMPARE(r.cc, (QStringList{u"dave@example.org"_s}));
    }

    void testReplyToMyOwnMessageGoesToTheRecipients()
    {
        const auto original = parse(mail("From: Bob <bob@example.org>\r\nTo: Alice <alice@example.org>\r\nCc: carol@example.org\r\n"));
        QCOMPARE(replyRecipients(*original, {u"bob@example.org"_s}, false).to, (QStringList{u"Alice <alice@example.org>"_s}));
        const Recipients all = replyRecipients(*original, {u"bob@example.org"_s}, true);
        QCOMPARE(all.to, (QStringList{u"Alice <alice@example.org>"_s}));
        QCOMPARE(all.cc, (QStringList{u"carol@example.org"_s}));
    }

    void testQuotedReply()
    {
        const auto original = parse(mail("From: Alice <alice@example.org>\r\nTo: bob@example.org\r\n", "Hello Bob,\n\nlunch?\n"));
        const QString quoted = quotedReply(*original);
        const QStringList lines = quoted.split(u'\n');
        QVERIFY(lines.first().contains(u"Alice"_s));
        QVERIFY(lines.first().endsWith(u"wrote:"_s) || lines.first().contains(u"wrote"_s));
        QCOMPARE(lines.mid(1), (QStringList{u"> Hello Bob,"_s, u">"_s, u"> lunch?"_s, QString()}));
    }

    void testQuotedReplyOfHtmlOnlyMail()
    {
        const QByteArray html =
            "Date: Thu, 08 Oct 2026 10:00:00 +0000\r\nFrom: alice@example.org\r\nContent-Type: text/html; charset=utf-8\r\n\r\n<html><body><p>Hi "
            "<b>Bob</b></p></body></html>";
        const QString quoted = quotedReply(*parse(html));
        QVERIFY(quoted.contains(u"> Hi Bob"_s));
        QVERIFY(!quoted.contains(u"<b>"_s));
    }

    void testForwardedBody()
    {
        const auto original = parse(mail("From: Alice <alice@example.org>\r\nTo: Bob <bob@example.org>\r\n"));
        const QString body = forwardedBody(*original);
        QVERIFY(body.startsWith(u"----------"_s));
        QVERIFY(body.contains(u"From: Alice <alice@example.org>\n"_s));
        QVERIFY(body.contains(u"Subject: Lunch\n"_s));
        QVERIFY(body.contains(u"To: Bob <bob@example.org>\n"_s));
        QVERIFY(body.endsWith(u"Hello Bob,\nlunch?"_s)); // KMime drops the final newline
    }

    void testThreadIds()
    {
        const auto original = parse(mail("From: a@example.org\r\nReferences: <root-1@example.org> <mid-2@example.org>\r\n"));
        const ThreadIds ids = threadIds(*original);
        QCOMPARE(ids.inReplyTo, u"orig-3@example.org"_s);
        QCOMPARE(ids.references, (QStringList{u"root-1@example.org"_s, u"mid-2@example.org"_s, u"orig-3@example.org"_s}));
    }

    void testThreadIdsWhenTheReferencesAlreadyEndWithTheMessage()
    {
        const auto original = parse(mail("From: a@example.org\r\nReferences: <root-1@example.org> <orig-3@example.org>\r\n"));
        QCOMPARE(threadIds(*original).references, (QStringList{u"root-1@example.org"_s, u"orig-3@example.org"_s}));
    }

    void testThreadIdsWithoutAMessageId()
    {
        const auto original = parse("From: a@example.org\r\nSubject: x\r\n\r\nbody");
        const ThreadIds ids = threadIds(*original);
        QVERIFY(ids.inReplyTo.isEmpty());
        QVERIFY(ids.references.isEmpty());
    }
};

QTEST_GUILESS_MAIN(MessageComposerTest)

#include "messagecomposertest.moc"
