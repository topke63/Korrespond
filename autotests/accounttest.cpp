// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QStandardPaths>
#include <QTest>

#include "account.h"

using namespace Qt::StringLiterals;

class AccountTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testPresetYahoo()
    {
        Account account;
        QVERIFY(Account::applyPreset(account, "yahoo"_L1));
        QCOMPARE(account.provider, "yahoo"_L1);
        QCOMPARE(account.imapHost, "imap.mail.yahoo.com"_L1);
        QCOMPARE(account.imapPort, 993);
        QCOMPARE(account.smtpHost, "smtp.mail.yahoo.com"_L1);
        QCOMPARE(account.smtpPort, 465);
    }

    void testPresetGmail()
    {
        Account account;
        QVERIFY(Account::applyPreset(account, "gmail"_L1));
        QCOMPARE(account.provider, "gmail"_L1);
        QCOMPARE(account.imapHost, "imap.gmail.com"_L1);
        QCOMPARE(account.imapPort, 993);
        QCOMPARE(account.smtpHost, "smtp.gmail.com"_L1);
        QCOMPARE(account.smtpPort, 465);
    }

    void testPresetOutlook()
    {
        Account account;
        QVERIFY(Account::applyPreset(account, "outlook"_L1));
        QCOMPARE(account.provider, "outlook"_L1);
        QCOMPARE(account.imapHost, "outlook.office365.com"_L1);
        QCOMPARE(account.imapPort, 993);
        QCOMPARE(account.smtpHost, "smtp-mail.outlook.com"_L1);
        QCOMPARE(account.smtpPort, 587);
    }

    void testPresetUnknown()
    {
        Account account;
        QVERIFY(!Account::applyPreset(account, "unknown"_L1));
        QCOMPARE(account.provider, "unknown"_L1);
        QVERIFY(account.imapHost.isEmpty());
        QCOMPARE(account.imapPort, 0);
        QVERIFY(account.smtpHost.isEmpty());
        QCOMPARE(account.smtpPort, 0);
    }

    void testIsValid()
    {
        Account account;
        QVERIFY(!account.isValid());

        account.email = "test@example.com"_L1;
        account.imapHost = "imap.example.com"_L1;
        account.imapPort = 993;
        QVERIFY(account.isValid());

        account.email.clear();
        QVERIFY(!account.isValid());

        account.email = "test@example.com"_L1;
        account.imapHost.clear();
        QVERIFY(!account.isValid());
    }

    void testSaveLoadRoundTrip()
    {
        // Enable test mode to use temporary paths
        QStandardPaths::setTestModeEnabled(true);

        Account account;
        account.email = "test@example.com"_L1;
        account.displayName = "Test User"_L1;
        account.provider = "yahoo"_L1;
        account.imapHost = "imap.mail.yahoo.com"_L1;
        account.imapPort = 993;
        account.smtpHost = "smtp.mail.yahoo.com"_L1;
        account.smtpPort = 465;

        account.save();

        Account loaded = Account::load();
        QCOMPARE(loaded.email, account.email);
        QCOMPARE(loaded.displayName, account.displayName);
        QCOMPARE(loaded.provider, account.provider);
        QCOMPARE(loaded.imapHost, account.imapHost);
        QCOMPARE(loaded.imapPort, account.imapPort);
        QCOMPARE(loaded.smtpHost, account.smtpHost);
        QCOMPARE(loaded.smtpPort, account.smtpPort);
    }

    void testRemove()
    {
        // Enable test mode to use temporary paths
        QStandardPaths::setTestModeEnabled(true);

        Account account;
        account.email = "test@example.com"_L1;
        account.save();

        Account loaded = Account::load();
        QCOMPARE(loaded.email, account.email);

        Account::remove();

        Account afterRemove = Account::load();
        QVERIFY(afterRemove.email.isEmpty());
    }
};

#include "accounttest.moc"

QTEST_MAIN(AccountTest)
