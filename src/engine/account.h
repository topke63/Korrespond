#pragma once

#include <QString>

// Connection settings for a single mail account.
// The password (or app password) is kept in the system keychain, never in the config file.
struct Account {
    QString email;
    QString displayName;
    QString provider; // "yahoo", "gmail", "outlook" or "custom"

    QString imapHost;
    quint16 imapPort = 993;
    QString smtpHost;
    quint16 smtpPort = 465;

    [[nodiscard]] bool isValid() const;

    // Fills in server settings for a known provider. Returns false for unknown providers.
    static bool applyPreset(Account &account, const QString &provider);

    // Single-account storage for now; multiple accounts come later.
    static Account load();
    void save() const;
    static void remove();
};

namespace AccountSecrets
{
inline constexpr auto service = "plasma-mail";
}
