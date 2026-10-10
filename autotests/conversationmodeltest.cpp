// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "conversationmodel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>
#include <QTimeZone>

using namespace Qt::StringLiterals;

static MessageSummary message(qint64 uid, int day, bool unread, const QString &from = u"Alice <alice@example.com>"_s)
{
    MessageSummary m;
    m.account = u"me@example.org"_s;
    m.uid = uid;
    m.subject = u"Plan"_s;
    m.from = from;
    m.unread = unread;
    m.date = QDateTime(QDate(2026, 10, day), QTime(9, 0), QTimeZone::UTC);
    return m;
}

class ConversationModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testOrderAndExpansion()
    {
        ConversationModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        // Given newest first, as the list has them.
        model.setMessages({message(4, 5, false), message(3, 4, true), message(2, 3, false), message(1, 2, false)});
        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(model.data(model.index(0), ConversationModel::UidRole).toLongLong(), qint64(1)); // oldest first
        QCOMPARE(model.data(model.index(3), ConversationModel::UidRole).toLongLong(), qint64(4));
        // Unread (uid 3) and the newest (uid 4) are open; read older ones are one line.
        QVERIFY(!model.isExpanded(0));
        QVERIFY(!model.isExpanded(1));
        QVERIFY(model.isExpanded(2));
        QVERIFY(model.isExpanded(3));
        QVERIFY(!model.isLoaded(2));
    }

    void testBodyAndExpand()
    {
        ConversationModel model;
        model.setMessages({message(2, 3, false), message(1, 2, false)});
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setExpanded(0, true);
        QVERIFY(model.isExpanded(0));
        QCOMPARE(changed.count(), 1);
        model.setExpanded(0, true); // no change, no signal
        QCOMPARE(changed.count(), 1);

        model.setBody(0, nullptr, u"<p>hi</p>"_s, true, {u"a.pdf"_s});
        QVERIFY(model.isLoaded(0));
        QCOMPARE(model.data(model.index(0), ConversationModel::HtmlRole).toString(), u"<p>hi</p>"_s);
        QVERIFY(model.data(model.index(0), ConversationModel::HasRemoteImagesRole).toBool());
        QVERIFY(!model.data(model.index(0), ConversationModel::ImagesLoadedRole).toBool());
        model.setHtml(0, u"<p>hi again</p>"_s, true);
        QVERIFY(model.data(model.index(0), ConversationModel::ImagesLoadedRole).toBool());
        model.setFailed(1);
        QVERIFY(model.isLoaded(1));
        QVERIFY(model.data(model.index(1), ConversationModel::HtmlRole).toString().isEmpty());
    }

    void testFlagsAndRemove()
    {
        ConversationModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setMessages({message(2, 3, true), message(1, 2, false)});
        model.setUnread(u"me@example.org"_s, 2, false);
        QVERIFY(!model.data(model.index(1), ConversationModel::UnreadRole).toBool());
        model.setFlagged(u"me@example.org"_s, 1, true);
        QVERIFY(model.data(model.index(0), ConversationModel::FlaggedRole).toBool());
        model.setUnread(u"other@example.org"_s, 1, true); // another account: untouched
        QVERIFY(!model.data(model.index(0), ConversationModel::UnreadRole).toBool());

        QCOMPARE(model.removeMessage(u"me@example.org"_s, 1), 0);
        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.removeMessage(u"me@example.org"_s, 1), -1);
        QCOMPARE(model.indexOf(u"me@example.org"_s, 2), 0);
    }

    void testFromName()
    {
        ConversationModel model;
        model.setMessages({message(1, 2, false, u"\"Doe, John\" <j@example.com>"_s), message(2, 3, false, u"bob@example.com"_s)});
        QCOMPARE(model.data(model.index(0), ConversationModel::FromNameRole).toString(), u"Doe, John"_s);
        QCOMPARE(model.data(model.index(1), ConversationModel::FromNameRole).toString(), u"bob@example.com"_s);
    }
};

QTEST_GUILESS_MAIN(ConversationModelTest)

#include "conversationmodeltest.moc"
