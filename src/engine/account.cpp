// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "account.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

static KSharedConfigPtr config()
{
    return KSharedConfig::openConfig(u"korrespondrc"_s);
}

// The group of the single-account config from milestone 1.
static KConfigGroup accountGroup()
{
    return config()->group(u"Account"_s);
}

static KConfigGroup accountGroup(const QString &email)
{
    return config()->group(u"Account "_s + email);
}

static Account readAccount(const KConfigGroup &group)
{
    Account account;
    account.email = group.readEntry("Email", QString());
    account.displayName = group.readEntry("DisplayName", QString());
    account.provider = group.readEntry("Provider", u"custom"_s);
    account.authMethod = group.readEntry("AuthMethod", u"password"_s);
    account.imapHost = group.readEntry("ImapHost", QString());
    account.imapPort = group.readEntry("ImapPort", 993);
    account.smtpHost = group.readEntry("SmtpHost", QString());
    account.smtpPort = group.readEntry("SmtpPort", 465);
    account.contacts = group.readEntry("Contacts", false);
    return account;
}

static void writeAccount(KConfigGroup &group, const Account &account)
{
    group.writeEntry("Email", account.email);
    group.writeEntry("DisplayName", account.displayName);
    group.writeEntry("Provider", account.provider);
    group.writeEntry("AuthMethod", account.authMethod);
    group.writeEntry("ImapHost", account.imapHost);
    group.writeEntry("ImapPort", int(account.imapPort));
    group.writeEntry("SmtpHost", account.smtpHost);
    group.writeEntry("SmtpPort", int(account.smtpPort));
    group.writeEntry("Contacts", account.contacts);
}

bool Account::isValid() const
{
    return !email.isEmpty() && !imapHost.isEmpty() && imapPort != 0;
}

bool Account::applyPreset(Account &account, const QString &provider)
{
    account.provider = provider;
    account.imapHost.clear();
    account.imapPort = 0;
    account.smtpHost.clear();
    account.smtpPort = 0;
    if (provider == "yahoo"_L1) {
        account.imapHost = u"imap.mail.yahoo.com"_s;
        account.imapPort = 993;
        account.smtpHost = u"smtp.mail.yahoo.com"_s;
        account.smtpPort = 465;
        return true;
    }
    if (provider == "gmail"_L1) {
        account.imapHost = u"imap.gmail.com"_s;
        account.imapPort = 993;
        account.smtpHost = u"smtp.gmail.com"_s;
        account.smtpPort = 465;
        return true;
    }
    if (provider == "outlook"_L1) {
        account.imapHost = u"outlook.office365.com"_s;
        account.imapPort = 993;
        account.smtpHost = u"smtp-mail.outlook.com"_s;
        account.smtpPort = 587;
        return true;
    }
    return false;
}

QList<Account> Account::loadAll()
{
    const QStringList order = config()->group(u"Accounts"_s).readEntry("Order", QStringList());
    QList<Account> accounts;
    if (order.isEmpty()) {
        // Milestone 1 config: at most one account, in [Account].
        const Account single = readAccount(accountGroup());
        if (!single.email.isEmpty()) {
            accounts.append(single);
        }
        return accounts;
    }
    for (const QString &email : order) {
        const KConfigGroup group = accountGroup(email);
        if (group.exists()) {
            accounts.append(readAccount(group));
        }
    }
    return accounts;
}

void Account::saveAll(const QList<Account> &accounts)
{
    KConfigGroup list = config()->group(u"Accounts"_s);
    QStringList order;
    for (const Account &account : accounts) {
        KConfigGroup group = accountGroup(account.email);
        writeAccount(group, account);
        order.append(account.email);
    }
    for (const QString &email : list.readEntry("Order", QStringList())) {
        if (!order.contains(email)) {
            accountGroup(email).deleteGroup();
        }
    }
    list.writeEntry("Order", order);
    accountGroup().deleteGroup();
    config()->sync();
}

Account Account::load()
{
    return readAccount(accountGroup());
}

void Account::save() const
{
    KConfigGroup group = accountGroup();
    writeAccount(group, *this);
    group.sync();
}

void Account::remove()
{
    KConfigGroup group = accountGroup();
    group.deleteGroup();
    group.sync();
}
