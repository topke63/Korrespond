// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "accountlistmodel.h"
#include "accountsession.h"
#include "oauthclient.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class AccountListModelTest : public QObject
{
    Q_OBJECT

private:
    QStringList order(const AccountListModel &model) const
    {
        QStringList emails;
        for (int row = 0; row < model.rowCount(); ++row) {
            emails << model.data(model.index(row), AccountListModel::EmailRole).toString();
        }
        return emails;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testMoveRow()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QList<AccountSession *> sessions;
        for (const QString &name : {u"a"_s, u"b"_s, u"c"_s}) {
            Account account;
            account.email = name + u"@example.com"_s;
            sessions.append(new AccountSession(account, dir.filePath(name), StorageSettings(), this));
        }
        AccountListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setSessions(sessions);
        const QStringList abc{u"a@example.com"_s, u"b@example.com"_s, u"c@example.com"_s};
        QCOMPARE(order(model), abc);

        QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

        QVERIFY(model.moveRow(0, 1)); // down
        QCOMPARE(order(model), (QStringList{u"b@example.com"_s, u"a@example.com"_s, u"c@example.com"_s}));
        QCOMPARE(moved.count(), 1);

        QVERIFY(model.moveRow(2, 1)); // up
        QCOMPARE(order(model), (QStringList{u"b@example.com"_s, u"c@example.com"_s, u"a@example.com"_s}));
        QCOMPARE(moved.count(), 2);

        QVERIFY(!model.moveRow(2, 3)); // past the end
        QVERIFY(!model.moveRow(0, -1));
        QVERIFY(!model.moveRow(1, 1));
        QCOMPARE(order(model), (QStringList{u"b@example.com"_s, u"c@example.com"_s, u"a@example.com"_s}));
        QCOMPARE(moved.count(), 2);
        QCOMPARE(reset.count(), 0);

        // The folder models travel with their rows.
        QCOMPARE(model.session(2)->account().email, u"a@example.com"_s);
        qDeleteAll(sessions);
    }

    void testSignInState()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QList<AccountSession *> sessions;
        for (const QString &name : {u"a"_s, u"b"_s}) {
            Account account;
            account.email = name + u"@example.com"_s;
            sessions.append(new AccountSession(account, dir.filePath(name), StorageSettings(), this));
        }
        AccountListModel model;
        model.setSessions(sessions);
        const auto state = [&model](int row) {
            return model.data(model.index(row), AccountListModel::ConnectionStateRole).toString();
        };
        QCOMPARE(state(0), u"idle"_s);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        // Google refuses the saved sign-in of the first account: that row says "signin", whatever
        // the connection state is; the other row is untouched.
        Q_EMIT sessions.at(0)->oauth()->signInExpired();
        QCOMPARE(state(0), u"signin"_s);
        QCOMPARE(state(1), u"idle"_s);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).value<QModelIndex>().row(), 0);
        QVERIFY(changed.at(0).at(2).value<QList<int>>().contains(AccountListModel::ConnectionStateRole));

        // A new token brings the real state back.
        Q_EMIT sessions.at(0)->oauth()->accessTokenChanged(u"t"_s, QDateTime::currentDateTime().addSecs(3600));
        QVERIFY(state(0) != u"signin"_s);
        qDeleteAll(sessions);
    }

    void testPushText()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QList<AccountSession *> sessions;
        for (const QString &name : {u"a"_s, u"b"_s, u"c"_s}) {
            Account account;
            account.email = name + u"@example.com"_s;
            sessions.append(new AccountSession(account, dir.filePath(name), StorageSettings(), this));
        }
        AccountListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setSessions(sessions);
        const QString off = u"Push: off"_s;

        // A fresh session has no watcher: Stopped reads as off, with push enabled by default.
        QCOMPARE(model.roleNames().value(AccountListModel::PushTextRole), QByteArray("pushText"));
        for (int row = 0; row < model.rowCount(); ++row) {
            QCOMPARE(model.data(model.index(row), AccountListModel::PushTextRole).toString(), off);
        }

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        // Turning push off changes the text of every row, with the push role only.
        model.setPushEnabled(false);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.at(0).at(0).value<QModelIndex>().row(), 0);
        QCOMPARE(changed.at(0).at(1).value<QModelIndex>().row(), 2);
        QCOMPARE(changed.at(0).at(2).value<QList<int>>(), (QList<int>{AccountListModel::PushTextRole}));
        for (int row = 0; row < model.rowCount(); ++row) {
            QCOMPARE(model.data(model.index(row), AccountListModel::PushTextRole).toString(), off);
        }

        // The same value again changes nothing.
        model.setPushEnabled(false);
        QCOMPARE(changed.count(), 1);

        // Turning it on signals every row again; a fresh session still reads as off.
        model.setPushEnabled(true);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.at(1).at(0).value<QModelIndex>().row(), 0);
        QCOMPARE(changed.at(1).at(1).value<QModelIndex>().row(), 2);
        QCOMPARE(changed.at(1).at(2).value<QList<int>>(), (QList<int>{AccountListModel::PushTextRole}));
        QCOMPARE(model.data(model.index(0), AccountListModel::PushTextRole).toString(), off);
        model.setPushEnabled(true);
        QCOMPARE(changed.count(), 2);

        // The sessions' state texts (Connecting, Idling, ...) need a server: not covered here.
        qDeleteAll(sessions);
    }
};

QTEST_GUILESS_MAIN(AccountListModelTest)
#include "accountlistmodeltest.moc"
