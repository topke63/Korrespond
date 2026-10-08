// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QString>

// Connection settings for one mail account. Its email address identifies it.
// The password (or, for OAuth, the refresh token) is kept in the system keychain, keyed by the
// email address, never in the config file.
struct Account {
    QString email;
    QString displayName;
    QString provider; // "yahoo", "gmail", "outlook" or "custom"
    QString authMethod = QStringLiteral("password"); // or "oauth"

    QString imapHost;
    quint16 imapPort = 993;
    QString smtpHost;
    quint16 smtpPort = 465;

    [[nodiscard]] bool isValid() const;

    // Fills in server settings for a known provider. Returns false for unknown providers.
    static bool applyPreset(Account &account, const QString &provider);

    // Every saved account, in the order they were added. A config from before multiple
    // accounts (one [Account] group) is read as a single account.
    static QList<Account> loadAll();
    // Replaces the saved accounts with these, in this order. Removes the groups of accounts
    // that are no longer in the list, and the old [Account] group.
    static void saveAll(const QList<Account> &accounts);

    // Deprecated: the single-account API, until MailController moves to loadAll/saveAll (T85).
    static Account load();
    void save() const;
    static void remove();
};

namespace AccountSecrets
{
inline constexpr auto service = "plasma-mail";
}
