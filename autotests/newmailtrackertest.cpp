// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include <QTimeZone>

#include "newmailtracker.h"

using namespace Qt::StringLiterals;

static const QDateTime now(QDate(2026, 10, 9), QTime(12, 0), QTimeZone::UTC);

static MessageSummary message(qint64 uid, bool unread = true, const QDateTime &date = now.addSecs(-60))
{
    MessageSummary summary;
    summary.account = u"me@example.org"_s;
    summary.uid = uid;
    summary.subject = u"Message %1"_s.arg(uid);
    summary.unread = unread;
    summary.date = date;
    return summary;
}

static QList<qint64> uids(const QList<MessageSummary> &messages)
{
    QList<qint64> result;
    for (const MessageSummary &m : messages) {
        result.append(m.uid);
    }
    return result;
}

class NewMailTrackerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFirstCheckOnlyRecords()
    {
        NewMailTracker tracker;
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5), message(4)}, now).isEmpty());
        // What was there at the first check is not announced later either.
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5), message(4)}, now).isEmpty());
    }

    void testNewUnreadMessagesAreFoundNewestFirst()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5), message(4)}, now);
        const QList<MessageSummary> found = tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(7), message(5), message(6), message(4)}, now);
        QCOMPARE(uids(found), (QList<qint64>{7, 6}));
        // Announced once.
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(7), message(6), message(5)}, now).isEmpty());
    }

    void testReadMessagesAreNotAnnouncedButRecorded()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5)}, now);
        // 6 was already read (e.g. on another device); 7 is unread.
        QCOMPARE(uids(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(7), message(6, false), message(5)}, now)), (QList<qint64>{7}));
        // 6 turning unread later does not make it new: its uid is not above the highest seen.
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(7), message(6, true), message(5)}, now).isEmpty());
    }

    void testOldMessagesAreNotAnnounced()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5)}, now);
        const QDateTime lastWeek = now.addDays(-7);
        const QDateTime hoursAgo = now.addSecs(-5 * 3600);
        const QList<MessageSummary> found =
            tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(8, true, lastWeek), message(7, true, hoursAgo), message(6, true, QDateTime())}, now);
        QCOMPARE(uids(found), (QList<qint64>{7, 6})); // the one from last week is not news; a missing date counts as recent
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(8, true, lastWeek)}, now).isEmpty()); // and stays quiet
    }

    void testUidValidityChangeStartsOver()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(500)}, now);
        // The folder was rebuilt: uids start again from 1. Nothing is announced, but later new mail is.
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 2, {message(3), message(2)}, now).isEmpty());
        QCOMPARE(uids(tracker.newMessages(u"a"_s, u"INBOX"_s, 2, {message(4), message(3)}, now)), (QList<qint64>{4}));
    }

    void testUnknownUidValidityIsNotAChange()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 7, {message(5)}, now);
        QCOMPARE(uids(tracker.newMessages(u"a"_s, u"INBOX"_s, 0, {message(6), message(5)}, now)), (QList<qint64>{6}));
    }

    void testAccountsAndFoldersAreIndependent()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5)}, now);
        // The first check of another account or folder records only, even with the same uids.
        QVERIFY(tracker.newMessages(u"b"_s, u"INBOX"_s, 1, {message(9)}, now).isEmpty());
        QVERIFY(tracker.newMessages(u"a"_s, u"Other"_s, 1, {message(9)}, now).isEmpty());
        QCOMPARE(uids(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(6), message(5)}, now)), (QList<qint64>{6}));
        QCOMPARE(uids(tracker.newMessages(u"b"_s, u"INBOX"_s, 1, {message(10), message(9)}, now)), (QList<qint64>{10}));
    }

    void testForgetAnAccount()
    {
        NewMailTracker tracker;
        tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(5)}, now);
        tracker.newMessages(u"ab"_s, u"INBOX"_s, 1, {message(5)}, now);
        tracker.forget(u"a"_s);
        // "a" starts from scratch (records only); "ab" is untouched.
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(9)}, now).isEmpty());
        QCOMPARE(uids(tracker.newMessages(u"ab"_s, u"INBOX"_s, 1, {message(6), message(5)}, now)), (QList<qint64>{6}));
    }

    void testEmptyList()
    {
        NewMailTracker tracker;
        QVERIFY(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {}, now).isEmpty());
        QCOMPARE(uids(tracker.newMessages(u"a"_s, u"INBOX"_s, 1, {message(1)}, now)),
                 (QList<qint64>{1})); // an empty inbox at the first check: everything after is new
    }
};

QTEST_GUILESS_MAIN(NewMailTrackerTest)

#include "newmailtrackertest.moc"
