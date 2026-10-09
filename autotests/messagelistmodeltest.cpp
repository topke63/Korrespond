// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagelistmodel.h"

#include <QAbstractItemModelTester>
#include <QLocale>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

using namespace Qt::StringLiterals;

static MessageSummary summary(qint64 uid, const QString &subject, bool unread = false, const QDateTime &date = {})
{
    MessageSummary msg;
    msg.account = u"me@example.org"_s;
    msg.uid = uid;
    msg.subject = subject;
    msg.from = u"Sender %1 <sender%1@example.com>"_s.arg(uid);
    msg.date = date;
    msg.unread = unread;
    return msg;
}

class MessageListModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testModelConsistency()
    {
        MessageListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setMessages({summary(3, u"c"_s, true), summary(2, u"b"_s), summary(1, u"a"_s)});
        model.setUnread(u"me@example.org"_s, 3, false);
        model.clear();
        model.setMessages({summary(1, u"a"_s)});
        QCOMPARE(model.rowCount(), 1);
    }

    void testRoles()
    {
        const QDateTime date(QDate(2026, 10, 1), QTime(10, 0), QTimeZone::UTC);
        MessageListModel model;
        model.setMessages({summary(42, u"Hello"_s, true, date), summary(7, QString())});

        const QModelIndex first = model.index(0);
        QCOMPARE(first.data(MessageListModel::UidRole).toLongLong(), 42);
        QCOMPARE(first.data(MessageListModel::SubjectRole).toString(), u"Hello"_s);
        QCOMPARE(first.data(Qt::DisplayRole).toString(), u"Hello"_s);
        QCOMPARE(first.data(MessageListModel::FromRole).toString(), u"Sender 42 <sender42@example.com>"_s);
        QCOMPARE(first.data(MessageListModel::DateRole).toDateTime(), date);
        QCOMPARE(first.data(MessageListModel::UnreadRole).toBool(), true);
        QCOMPARE(first.data(MessageListModel::FlaggedRole).toBool(), false);

        QCOMPARE(model.index(1).data(MessageListModel::SubjectRole).toString(), u"(No subject)"_s);

        const QHash<int, QByteArray> names = model.roleNames();
        QCOMPARE(names.value(MessageListModel::UidRole), "uid");
        QCOMPARE(names.value(MessageListModel::AccountRole), "account");
        QCOMPARE(names.value(MessageListModel::SubjectRole), "subject");
        QCOMPARE(names.value(MessageListModel::FromRole), "from");
        QCOMPARE(names.value(MessageListModel::DateRole), "date");
        QCOMPARE(names.value(MessageListModel::DateTextRole), "dateText");
        QCOMPARE(names.value(MessageListModel::UnreadRole), "unread");
        QCOMPARE(names.value(MessageListModel::FlaggedRole), "flagged");
    }

    void testFlagged()
    {
        MessageSummary flagged = summary(5, u"Starred"_s);
        flagged.flagged = true;
        MessageListModel model;
        model.setMessages({flagged});
        QCOMPARE(model.index(0).data(MessageListModel::FlaggedRole).toBool(), true);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setFlagged(u"me@example.org"_s, 5, false);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(2).value<QList<int>>(), QList<int>{MessageListModel::FlaggedRole});
        QCOMPARE(model.index(0).data(MessageListModel::FlaggedRole).toBool(), false);
        model.setFlagged(u"me@example.org"_s, 5, false); // no change, no signal
        model.setFlagged(u"me@example.org"_s, 99, true); // unknown uid is ignored
        QCOMPARE(changed.count(), 1);
    }

    void testDateText()
    {
        const QDate today(2026, 10, 8);
        const QDateTime morning(today, QTime(9, 30));
        QCOMPARE(MessageListModel::dateText(morning, today), QLocale().toString(QTime(9, 30), QLocale::ShortFormat));

        const QDateTime lastWeek(QDate(2026, 10, 1), QTime(9, 30));
        QCOMPARE(MessageListModel::dateText(lastWeek, today), QLocale().toString(QDate(2026, 10, 1), QLocale::ShortFormat));

        QCOMPARE(MessageListModel::dateText(QDateTime(), today), QString());
    }

    void testDateTextUsesLocalTime()
    {
        // 23:30 at UTC-12 is already the next day (or later) in every time zone ahead of it.
        const QDateTime farWest(QDate(2026, 10, 7), QTime(23, 30), QTimeZone(-12 * 3600));
        const QDateTime local = farWest.toLocalTime();
        QCOMPARE(MessageListModel::dateText(farWest, local.date()), QLocale().toString(local.time(), QLocale::ShortFormat));
    }

    void testSetUnreadEmitsOneChange()
    {
        MessageListModel model;
        model.setMessages({summary(1, u"a"_s, true), summary(2, u"b"_s, false)});
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

        model.setUnread(u"me@example.org"_s, 1, false);
        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toModelIndex(), model.index(0));
        QCOMPARE(args.at(1).toModelIndex(), model.index(0));
        QCOMPARE(args.at(2).value<QList<int>>(), QList<int>{MessageListModel::UnreadRole});
        QCOMPARE(model.index(0).data(MessageListModel::UnreadRole).toBool(), false);

        model.setUnread(u"me@example.org"_s, 1, false); // unchanged: no signal
        model.setUnread(u"me@example.org"_s, 999, true); // unknown uid: no signal
        QCOMPARE(spy.count(), 0);
    }

    void testMessageLookup()
    {
        MessageListModel model;
        model.setMessages({summary(5, u"Found"_s)});
        QCOMPARE(model.message(u"me@example.org"_s, 5).subject, u"Found"_s);
        QCOMPARE(model.message(u"me@example.org"_s, 999).uid, qint64(0));
    }

    void testSameUidInTwoAccounts()
    {
        MessageSummary yahoo = summary(7, u"From Yahoo"_s, true);
        yahoo.account = u"me@yahoo.com"_s;
        MessageSummary gmail = summary(7, u"From Gmail"_s, true);
        gmail.account = u"me@gmail.com"_s;
        MessageListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setMessages({yahoo, gmail});
        QCOMPARE(model.rowCount(), 2);

        QCOMPARE(model.index(0).data(MessageListModel::AccountRole).toString(), u"me@yahoo.com"_s);
        QCOMPARE(model.index(1).data(MessageListModel::AccountRole).toString(), u"me@gmail.com"_s);
        QCOMPARE(model.message(u"me@yahoo.com"_s, 7).subject, u"From Yahoo"_s);
        QCOMPARE(model.message(u"me@gmail.com"_s, 7).subject, u"From Gmail"_s);
        QCOMPARE(model.message(u"other@example.org"_s, 7).uid, qint64(0));
    }

    void testChangesOnlyTouchTheGivenAccount()
    {
        MessageSummary yahoo = summary(7, u"From Yahoo"_s, true);
        yahoo.account = u"me@yahoo.com"_s;
        MessageSummary gmail = summary(7, u"From Gmail"_s, true);
        gmail.account = u"me@gmail.com"_s;
        MessageListModel model;
        model.setMessages({yahoo, gmail});
        QSignalSpy spy(&model, &QAbstractItemModel::dataChanged);

        model.setUnread(u"me@gmail.com"_s, 7, false);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toModelIndex(), model.index(1));
        QVERIFY(model.index(0).data(MessageListModel::UnreadRole).toBool());
        QVERIFY(!model.index(1).data(MessageListModel::UnreadRole).toBool());

        spy.clear();
        model.setFlagged(u"me@yahoo.com"_s, 7, true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toModelIndex(), model.index(0));
        QVERIFY(model.index(0).data(MessageListModel::FlaggedRole).toBool());
        QVERIFY(!model.index(1).data(MessageListModel::FlaggedRole).toBool());

        spy.clear();
        model.setUnread(u"other@example.org"_s, 7, false); // no such account: nothing changes
        QCOMPARE(spy.count(), 0);
    }

    void testRemoveMessage()
    {
        MessageSummary gmail = summary(2, u"Gmail two"_s);
        gmail.account = u"me@gmail.com"_s;
        MessageListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setMessages({summary(1, u"a"_s), summary(2, u"b"_s), gmail, summary(3, u"c"_s)});
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);

        // The same uid in another account is another message.
        QCOMPARE(model.removeMessage(u"me@example.org"_s, 2), 1);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(model.rowCount(), 3);
        QCOMPARE(model.messageAt(1).account, u"me@gmail.com"_s);
        QCOMPARE(model.messageAt(2).uid, qint64(3));

        QCOMPARE(model.removeMessage(u"me@example.org"_s, 2), -1); // already gone
        QCOMPARE(removed.count(), 1);
        QCOMPARE(model.messageAt(99).uid, qint64(0));
        QCOMPARE(model.messageAt(-1).uid, qint64(0));
    }

    void testClear()
    {
        MessageListModel model;
        model.setMessages({summary(1, u"a"_s)});
        model.clear();
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_GUILESS_MAIN(MessageListModelTest)
#include "messagelistmodeltest.moc"
