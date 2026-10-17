// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "accountsession.h"
#include "oauthclient.h"

using namespace Qt::StringLiterals;

// The "sign in again" state of a Google account, driven through the OAuth client's own signals.
class AccountSessionTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testExpiredSignInStopsLoginsUntilANewToken()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        Account account;
        account.email = u"me@example.com"_s;
        account.provider = u"gmail"_s;
        account.authMethod = u"oauth"_s;
        account.imapHost = u"imap.invalid"_s;
        AccountSession session(account, dir.filePath(u"store"_s), StorageSettings());
        QVERIFY(!session.needsSignIn());
        QSignalSpy changed(&session, &AccountSession::needsSignInChanged);

        Q_EMIT session.oauth()->signInExpired();
        QVERIFY(session.needsSignIn());
        QCOMPARE(changed.count(), 1);
        Q_EMIT session.oauth()->signInExpired(); // again: no second change
        QCOMPARE(changed.count(), 1);

        // A login with the dead token is not even started.
        session.connectWithSecret(u"old-token"_s);
        QCOMPARE(session.connectionState(), AccountSession::Idle);

        // A fresh access token clears it and the login goes ahead.
        Q_EMIT session.oauth()->accessTokenChanged(u"new-token"_s, QDateTime::currentDateTime().addSecs(3600));
        QVERIFY(!session.needsSignIn());
        QCOMPARE(changed.count(), 2);
        QCOMPARE(session.connectionState(), AccountSession::Connecting);
    }
};

QTEST_GUILESS_MAIN(AccountSessionTest)

#include "accountsessiontest.moc"
