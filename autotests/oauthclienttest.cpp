// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTest>

#include "oauthclient.h"

using namespace Qt::StringLiterals;

// Google's error answers, without talking to Google: the handler is reached through the friend
// declaration, the "token in use" and "signing in" states are set by hand.
class OAuthClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testErrorCodes()
    {
        QVERIFY(OAuthClient::isExpiredSignInError(u"invalid_grant"_s));
        QVERIFY(!OAuthClient::isExpiredSignInError(u"access_denied"_s));
        QVERIFY(!OAuthClient::isExpiredSignInError(QString()));
    }

    void testRefusedSavedTokenSaysSignInExpired()
    {
        OAuthClient client;
        client.m_tokenInUse = true; // what refresh() sets
        QSignalSpy failed(&client, &OAuthClient::failed);
        QSignalSpy expired(&client, &OAuthClient::signInExpired);
        QVERIFY(!client.signInHasExpired());

        client.handleServerError(u"invalid_grant"_s, u"Token has been expired or revoked."_s);

        QCOMPARE(failed.count(), 1);
        QVERIFY(failed.first().first().toString().contains(u"sign in again"_s, Qt::CaseInsensitive));
        QCOMPARE(expired.count(), 1);
        QVERIFY(client.signInHasExpired());
        QVERIFY(!client.m_tokenInUse);
    }

    void testInteractiveSignInRefusedIsNotAnExpiry()
    {
        OAuthClient client;
        client.m_signingIn = true;
        QSignalSpy failed(&client, &OAuthClient::failed);
        QSignalSpy expired(&client, &OAuthClient::signInExpired);

        client.handleServerError(u"invalid_grant"_s, QString());

        QCOMPARE(failed.count(), 1);
        QCOMPARE(expired.count(), 0);
        QVERIFY(!client.signInHasExpired());
    }

    void testOtherErrorsAreFailuresOnly()
    {
        OAuthClient client;
        client.m_tokenInUse = true;
        QSignalSpy failed(&client, &OAuthClient::failed);
        QSignalSpy expired(&client, &OAuthClient::signInExpired);

        client.handleServerError(u"access_denied"_s, QString());
        client.handleServerError(u"server_error"_s, u"try later"_s);

        QCOMPARE(failed.count(), 2);
        QVERIFY(failed.at(1).first().toString().contains(u"try later"_s));
        QCOMPARE(expired.count(), 0);
        QVERIFY(!client.signInHasExpired());
    }

    void testNothingReportedWhenIdle()
    {
        OAuthClient client;
        QSignalSpy failed(&client, &OAuthClient::failed);
        QSignalSpy expired(&client, &OAuthClient::signInExpired);

        client.handleServerError(u"invalid_grant"_s, QString());

        QCOMPARE(failed.count(), 0);
        QCOMPARE(expired.count(), 0);
    }
};

QTEST_GUILESS_MAIN(OAuthClientTest)

#include "oauthclienttest.moc"
