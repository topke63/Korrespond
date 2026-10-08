#include "account.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

static KConfigGroup accountGroup()
{
    return KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"Account"_s);
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

Account Account::load()
{
    const KConfigGroup group = accountGroup();
    Account account;
    account.email = group.readEntry("Email", QString());
    account.displayName = group.readEntry("DisplayName", QString());
    account.provider = group.readEntry("Provider", u"custom"_s);
    account.imapHost = group.readEntry("ImapHost", QString());
    account.imapPort = group.readEntry("ImapPort", 993);
    account.smtpHost = group.readEntry("SmtpHost", QString());
    account.smtpPort = group.readEntry("SmtpPort", 465);
    return account;
}

void Account::save() const
{
    KConfigGroup group = accountGroup();
    group.writeEntry("Email", email);
    group.writeEntry("DisplayName", displayName);
    group.writeEntry("Provider", provider);
    group.writeEntry("ImapHost", imapHost);
    group.writeEntry("ImapPort", int(imapPort));
    group.writeEntry("SmtpHost", smtpHost);
    group.writeEntry("SmtpPort", int(smtpPort));
    group.sync();
}

void Account::remove()
{
    KConfigGroup group = accountGroup();
    group.deleteGroup();
    group.sync();
}
