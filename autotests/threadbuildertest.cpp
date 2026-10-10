// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTest>
#include <QTimeZone>

#include "threadbuilder.h"

using namespace Qt::StringLiterals;

class ThreadBuilderTest : public QObject
{
    Q_OBJECT

private:
    static MessageSummary
    message(qint64 uid, int day, const QString &subject, const QString &id = {}, const QString &replyTo = {}, const QStringList &refs = {})
    {
        MessageSummary m;
        m.uid = uid;
        m.subject = subject;
        m.date = QDateTime(QDate(2026, 1, 1).addDays(day - 1), QTime(12, 0), QTimeZone::UTC);
        m.messageId = id;
        m.inReplyTo = replyTo;
        m.references = refs;
        return m;
    }

private Q_SLOTS:
    void testNormalisedSubject()
    {
        QCOMPARE(ThreadBuilder::normalisedSubject(u"Re: Fwd: AW:  Lunch   on  FRIDAY "_s), u"lunch on friday"_s);
        QCOMPARE(ThreadBuilder::normalisedSubject(u"SV: re[2]: Plan"_s), u"plan"_s);
        QCOMPARE(ThreadBuilder::normalisedSubject(u"Regarding Re: x"_s), u"regarding re: x"_s); // not a prefix
        QCOMPARE(ThreadBuilder::normalisedSubject(u"Fwd:"_s), QString());
        QCOMPARE(ThreadBuilder::normalisedSubject(QString()), QString());
    }

    void testReferencesChain()
    {
        const auto ids = ThreadBuilder::build({
            message(1, 1, u"Plan"_s, u"a"_s),
            message(2, 2, u"Re: Plan"_s, u"b"_s, u"a"_s, {u"a"_s}),
            message(3, 3, u"Re: Re: Plan"_s, u"c"_s, u"b"_s, {u"a"_s, u"b"_s}),
            message(4, 3, u"Other"_s, u"d"_s),
        });
        QCOMPARE(ids.value(1), u"m:a"_s);
        QCOMPARE(ids.value(2), ids.value(1));
        QCOMPARE(ids.value(3), ids.value(1));
        QVERIFY(ids.value(4) != ids.value(1));
    }

    void testMissingParentJoinsSiblings()
    {
        // Two replies to a message that is not in this folder (e.g. it is in Sent).
        const auto ids = ThreadBuilder::build({
            message(1, 1, u"Re: Offer"_s, u"x"_s, u"gone"_s, {u"gone"_s}),
            message(2, 2, u"Re: Offer"_s, u"y"_s, u"gone"_s, {u"gone"_s}),
            message(3, 2, u"Different"_s, u"z"_s, u"elsewhere"_s, {u"elsewhere"_s}),
        });
        QCOMPARE(ids.value(1), ids.value(2));
        QVERIFY(ids.value(3) != ids.value(1));
    }

    void testBrokenReferencesFallBackToSubject()
    {
        // The reply lost its References/In-Reply-To (some clients do); the subject joins it.
        const auto ids = ThreadBuilder::build({
            message(1, 1, u"Invoice 42"_s, u"a"_s),
            message(2, 3, u"Re: Invoice 42"_s, u"b"_s),
        });
        QCOMPARE(ids.value(1), ids.value(2));
    }

    void testSameSubjectMonthsApartIsNotOneThread()
    {
        const auto ids = ThreadBuilder::build({
            message(1, 1, u"Newsletter"_s, u"a"_s),
            message(2, 5, u"Newsletter"_s, u"b"_s), // 4 days later: joins
            message(3, 100, u"Newsletter"_s, u"c"_s), // 95 days later: its own thread
        });
        QCOMPARE(ids.value(1), ids.value(2));
        QVERIFY(ids.value(3) != ids.value(1));
    }

    void testLinkedMessagesAreNotPulledInBySubject()
    {
        // Two real conversations with the same subject: their headers keep them apart.
        const auto ids = ThreadBuilder::build({
            message(1, 1, u"Hello"_s, u"a1"_s),
            message(2, 2, u"Re: Hello"_s, u"a2"_s, u"a1"_s, {u"a1"_s}),
            message(3, 2, u"Hello"_s, u"b1"_s),
            message(4, 3, u"Re: Hello"_s, u"b2"_s, u"b1"_s, {u"b1"_s}),
        });
        QCOMPARE(ids.value(1), ids.value(2));
        QCOMPARE(ids.value(3), ids.value(4));
        QVERIFY(ids.value(1) != ids.value(3));
    }

    void testGmailThreadIdWins()
    {
        MessageSummary a = message(1, 1, u"One"_s, u"a"_s);
        MessageSummary b = message(2, 2, u"Completely different"_s, u"b"_s);
        a.gmThreadId = 77;
        b.gmThreadId = 77;
        MessageSummary c = message(3, 3, u"Re: One"_s, u"c"_s, u"a"_s, {u"a"_s}); // no Gmail id: by headers
        MessageSummary d = message(4, 3, u"One"_s, u"d"_s);
        d.gmThreadId = 88; // same subject, another Gmail thread: stays apart
        const auto ids = ThreadBuilder::build({a, b, c, d});
        QCOMPARE(ids.value(1), u"g:77"_s);
        QCOMPARE(ids.value(2), u"g:77"_s);
        QVERIFY(ids.value(3) != ids.value(4));
        QCOMPARE(ids.value(4), u"g:88"_s);
    }

    void testIdsDoNotDependOnInputOrder()
    {
        QList<MessageSummary> list = {
            message(1, 1, u"Plan"_s, u"a"_s),
            message(2, 2, u"Re: Plan"_s, u"b"_s, u"a"_s, {u"a"_s}),
            message(3, 9, u"Solo"_s),
            message(4, 3, u"Re: Plan"_s, u"c"_s, u"b"_s, {u"a"_s, u"b"_s}),
        };
        const auto forward = ThreadBuilder::build(list);
        std::reverse(list.begin(), list.end());
        QCOMPARE(ThreadBuilder::build(list), forward);
        QCOMPARE(forward.value(3), u"u:3"_s); // no Message-ID: named by uid
    }

    void testSelfReferenceAndEmpty()
    {
        QVERIFY(ThreadBuilder::build({}).isEmpty());
        const auto ids = ThreadBuilder::build({message(1, 1, u"x"_s, u"a"_s, u"a"_s, {u"a"_s})});
        QCOMPARE(ids.value(1), u"m:a"_s);
    }

    void testUndatedMessageSubject()
    {
        // No date: the subject fallback needs dates, so these stay apart rather than guessing.
        MessageSummary a = message(1, 1, u"Same"_s);
        MessageSummary b = message(2, 1, u"Re: Same"_s);
        a.date = {};
        b.date = {};
        const auto ids = ThreadBuilder::build({a, b});
        QVERIFY(ids.value(1) != ids.value(2));
    }
};

QTEST_GUILESS_MAIN(ThreadBuilderTest)

#include "threadbuildertest.moc"
