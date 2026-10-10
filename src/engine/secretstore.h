// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QString>

// Asynchronous access to the system keychain (KWallet or Secret Service) via QtKeychain.
// Every request answers with exactly one signal; an empty `error` means success.
// Secrets are never stored unencrypted: QtKeychain's insecure file fallback stays off.
class SecretStore : public QObject
{
    Q_OBJECT
public:
    // `service` groups the entries in the keychain; the app uses AccountSecrets::service.
    explicit SecretStore(const QString &service, QObject *parent = nullptr);

    [[nodiscard]] QString service() const;

    // Entries stored by an older version under another service name: readPassword() looks there
    // when the key is missing, and copies what it finds to `service`. Nothing is deleted.
    void setLegacyService(const QString &legacyService);

    void writePassword(const QString &key, const QString &password);
    void readPassword(const QString &key);
    // Deleting a key that doesn't exist counts as success.
    void deletePassword(const QString &key);

Q_SIGNALS:
    void passwordWritten(const QString &key, const QString &error);
    void passwordRead(const QString &key, const QString &password, const QString &error);
    void passwordDeleted(const QString &key, const QString &error);

private:
    void readLegacyPassword(const QString &key);

    QString m_service;
    QString m_legacyService;
};
