// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>

#include "oauthsettings.h"

using namespace Qt::StringLiterals;

class OAuthSettingsTest : public QObject
{
    Q_OBJECT

private:
    static KConfigGroup group()
    {
        return KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"GoogleOAuth"_s);
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
        qunsetenv("PLASMAMAIL_GOOGLE_CLIENT_ID");
        qunsetenv("PLASMAMAIL_GOOGLE_CLIENT_SECRET");
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
        qputenv("PLASMAMAIL_GOOGLE_CLIENT_ID", "env-id");
        GoogleOAuthSettings settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientId, u"env-id"_s);
        QCOMPARE(settings.clientSecret, u"config-secret"_s); // each value on its own

        qputenv("PLASMAMAIL_GOOGLE_CLIENT_SECRET", "env-secret");
        settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientSecret, u"env-secret"_s);
    }

    void testEmptyEnvironmentDoesNotHideConfig()
    {
        writeConfig(u"config-id"_s, u"config-secret"_s);
        qputenv("PLASMAMAIL_GOOGLE_CLIENT_ID", "");
        qputenv("PLASMAMAIL_GOOGLE_CLIENT_SECRET", "  ");
        const GoogleOAuthSettings settings = GoogleOAuthSettings::load();
        QCOMPARE(settings.clientId, u"config-id"_s);
        QCOMPARE(settings.clientSecret, u"config-secret"_s);
    }

    void testEnvironmentOnly()
    {
        qputenv("PLASMAMAIL_GOOGLE_CLIENT_ID", "env-id");
        qputenv("PLASMAMAIL_GOOGLE_CLIENT_SECRET", "env-secret");
        QVERIFY(GoogleOAuthSettings::load().isConfigured());
    }
};

QTEST_GUILESS_MAIN(OAuthSettingsTest)

#include "oauthsettingstest.moc"
