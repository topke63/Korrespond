// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>

#include "account.h"

using namespace Qt::StringLiterals;

class AccountTest : public QObject
{
    Q_OBJECT

private:
    static Account makeAccount(const QString &email, const QString &provider = u"yahoo"_s)
    {
        Account account;
        Account::applyPreset(account, provider);
        account.email = email;
        account.displayName = u"Name of "_s + email;
        return account;
    }

    static KSharedConfigPtr config()
    {
        return KSharedConfig::openConfig(u"korrespondrc"_s);
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Before the first openConfig(): the config file then lives in a test directory.
        QStandardPaths::setTestModeEnabled(true);
    }

    // Every test starts with an empty korrespondrc (the test file survives between runs).
    void init()
    {
        const KSharedConfigPtr cfg = config();
        for (const QString &group : cfg->groupList()) {
            cfg->deleteGroup(group);
        }
        QVERIFY(cfg->sync());
    }

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
        Account account;
        account.email = "test@example.com"_L1;
        account.save();

        Account loaded = Account::load();
        QCOMPARE(loaded.email, account.email);

        Account::remove();

        Account afterRemove = Account::load();
        QVERIFY(afterRemove.email.isEmpty());
    }

    void testLoadAllEmpty()
    {
        QVERIFY(Account::loadAll().isEmpty());
    }

    void testSaveAllLoadAllKeepsOrderAndFields()
    {
        Account yahoo = makeAccount(u"me@yahoo.com"_s);
        Account gmail = makeAccount(u"me@gmail.com"_s, u"gmail"_s);
        gmail.authMethod = u"oauth"_s;
        Account::saveAll({yahoo, gmail});

        const QList<Account> loaded = Account::loadAll();
        QCOMPARE(loaded.size(), 2);
        QCOMPARE(loaded.at(0).email, yahoo.email); // added first, listed first
        QCOMPARE(loaded.at(1).email, gmail.email);
        QCOMPARE(loaded.at(0).authMethod, u"password"_s);
        const Account &g = loaded.at(1);
        QCOMPARE(g.displayName, gmail.displayName);
        QCOMPARE(g.provider, u"gmail"_s);
        QCOMPARE(g.authMethod, u"oauth"_s);
        QCOMPARE(g.imapHost, u"imap.gmail.com"_s);
        QCOMPARE(g.imapPort, 993);
        QCOMPARE(g.smtpHost, u"smtp.gmail.com"_s);
        QCOMPARE(g.smtpPort, 465);

        // Reversed order is kept too.
        Account::saveAll({gmail, yahoo});
        QCOMPARE(Account::loadAll().first().email, gmail.email);
    }

    void testSaveAllRemovesDroppedAccounts()
    {
        Account::saveAll({makeAccount(u"a@example.com"_s), makeAccount(u"b@example.com"_s)});
        Account::saveAll({makeAccount(u"b@example.com"_s)});

        const QList<Account> loaded = Account::loadAll();
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().email, u"b@example.com"_s);
        QVERIFY(!config()->hasGroup(u"Account a@example.com"_s)); // no leftover group

        Account::saveAll({});
        QVERIFY(Account::loadAll().isEmpty());
        QVERIFY(!config()->hasGroup(u"Account b@example.com"_s));
    }

    void testMigratesSingleAccountConfig()
    {
        // A milestone 1 config: one [Account] group, written before AuthMethod existed.
        {
            KConfigGroup old = config()->group(u"Account"_s);
            old.writeEntry("Email", u"old@yahoo.com"_s);
            old.writeEntry("DisplayName", u"Old"_s);
            old.writeEntry("Provider", u"yahoo"_s);
            old.writeEntry("ImapHost", u"imap.mail.yahoo.com"_s);
            old.writeEntry("ImapPort", 993);
            QVERIFY(config()->sync());
        }

        QList<Account> loaded = Account::loadAll();
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(loaded.first().email, u"old@yahoo.com"_s);
        QCOMPARE(loaded.first().displayName, u"Old"_s);
        QCOMPARE(loaded.first().authMethod, u"password"_s); // missing key → password

        // Adding a second account rewrites the config; the old group goes away.
        loaded.append(makeAccount(u"new@gmail.com"_s, u"gmail"_s));
        Account::saveAll(loaded);
        QVERIFY(!config()->hasGroup(u"Account"_s));
        const QList<Account> after = Account::loadAll();
        QCOMPARE(after.size(), 2);
        QCOMPARE(after.at(0).email, u"old@yahoo.com"_s);
        QCOMPARE(after.at(1).email, u"new@gmail.com"_s);
    }
};

#include "accounttest.moc"

QTEST_MAIN(AccountTest)
