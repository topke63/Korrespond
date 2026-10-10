// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>

#include "account.h"
#include "oauthclient.h"
#include "oauthsettings.h"

using namespace Qt::StringLiterals;

class OAuthSettingsTest : public QObject
{
    Q_OBJECT

private:
    static KConfigGroup group()
    {
        return KSharedConfig::openConfig(u"korrespondrc"_s)->group(u"GoogleOAuth"_s);
    }

    static void writeConfig(const QString &id, const QString &secret)
    {
        KConfigGroup g = group();
        g.writeEntry("ClientId", id);
        g.writeEntry("ClientSecret", secret);
        g.sync();
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Before the first openConfig(): the config file then lives in a test directory.
        QStandardPaths::setTestModeEnabled(true);
    }

    // Every test starts without config values or environment variables.
    void init()
    {
        KConfigGroup g = group();
        g.deleteGroup();
        g.sync();
        qunsetenv("KORRESPOND_GOOGLE_CLIENT_ID");
        qunsetenv("KORRESPOND_GOOGLE_CLIENT_SECRET");
    }

    void testNothingSet()
    {
        const GoogleOAuthSettings settings = GoogleOAuthSettings::load();
        QVERIFY(settings.clientId.isEmpty());
        QVERIFY(settings.clientSecret.isEmpty());
        QVERIFY(!settings.isConfigured());
    }

    void testFromConfig()
    {
        writeConfig(u" id-123.apps.googleusercontent.com "_s, u"secret-abc\n"_s); // pasted with spaces
        const GoogleOAuthSettings settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientId, u"id-123.apps.googleusercontent.com"_s);
        QCOMPARE(settings.clientSecret, u"secret-abc"_s);
        QVERIFY(settings.isConfigured());
    }

    void testNeedsBothValues()
    {
        writeConfig(u"id-123"_s, QString());
        QVERIFY(!GoogleOAuthSettings::load().isConfigured());
        writeConfig(QString(), u"secret-abc"_s);
        QVERIFY(!GoogleOAuthSettings::load().isConfigured());
    }

    void testEnvironmentWins()
    {
        writeConfig(u"config-id"_s, u"config-secret"_s);
        qputenv("KORRESPOND_GOOGLE_CLIENT_ID", "env-id");
        GoogleOAuthSettings settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientId, u"env-id"_s);
        QCOMPARE(settings.clientSecret, u"config-secret"_s); // each value on its own

        qputenv("KORRESPOND_GOOGLE_CLIENT_SECRET", "env-secret");
        settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientSecret, u"env-secret"_s);
    }

    void testEmptyEnvironmentDoesNotHideConfig()
    {
        writeConfig(u"config-id"_s, u"config-secret"_s);
        qputenv("KORRESPOND_GOOGLE_CLIENT_ID", "");
        qputenv("KORRESPOND_GOOGLE_CLIENT_SECRET", "  ");
        const GoogleOAuthSettings settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientId, u"config-id"_s);
        QCOMPARE(settings.clientSecret, u"config-secret"_s);
    }

    void testScopeTokens()
    {
        QCOMPARE(OAuthClient::scopeTokens(false), QStringList{u"https://mail.google.com/"_s});
        const QStringList withContacts = OAuthClient::scopeTokens(true);
        QCOMPARE(withContacts.size(), 3);
        QVERIFY(withContacts.contains(u"https://mail.google.com/"_s));
        QVERIFY(withContacts.contains(u"https://www.googleapis.com/auth/contacts.readonly"_s));
        QVERIFY(withContacts.contains(u"https://www.googleapis.com/auth/contacts.other.readonly"_s));
        QVERIFY(OAuthClient::hasContactsScope(withContacts));
        QVERIFY(!OAuthClient::hasContactsScope(OAuthClient::scopeTokens(false)));
        QVERIFY(!OAuthClient::hasContactsScope({}));
    }

    void testAccountRemembersContactsScope()
    {
        Account account;
        account.email = u"me@gmail.com"_s;
        account.provider = u"gmail"_s;
        account.authMethod = u"oauth"_s;
        account.imapHost = u"imap.gmail.com"_s;
        QVERIFY(!account.contacts);
        Account plain = account;
        plain.email = u"other@gmail.com"_s;
        account.contacts = true;
        Account::saveAll({account, plain});
        const QList<Account> loaded = Account::loadAll();
        QCOMPARE(loaded.size(), 2);
        QVERIFY(loaded.at(0).contacts);
        QVERIFY(!loaded.at(1).contacts); // an account saved before has no contacts access
    }

    void testEnvironmentOnly()
    {
        qputenv("KORRESPOND_GOOGLE_CLIENT_ID", "env-id");
        qputenv("KORRESPOND_GOOGLE_CLIENT_SECRET", "env-secret");
        QVERIFY(GoogleOAuthSettings::load().isConfigured());
    }
};

QTEST_GUILESS_MAIN(OAuthSettingsTest)

#include "oauthsettingstest.moc"
