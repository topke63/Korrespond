// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailcontroller.h"

#include "foldermodel.h"
#include "imapclient.h"
#include "secretstore.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

MailController::MailController(QObject *parent)
    : QObject(parent)
    , m_client(new ImapClient(this))
    , m_secrets(new SecretStore(QString::fromLatin1(AccountSecrets::service), this))
    , m_folderModel(new FolderModel(this))
{
    connect(m_client, &ImapClient::foldersListed, this, &MailController::onFoldersListed);
    connect(m_client, &ImapClient::errorOccurred, this, [this](const QString &message) {
        // A failed connection or login fails every queued job, so nothing is in flight any more.
        resetRequests();
        if (m_pendingAccount.isValid()) {
            // Verification of a new account failed: forget it, keep whatever was there before.
            m_pendingAccount = {};
            m_pendingPassword.clear();
            if (m_account.isValid()) {
                connectSavedAccount(); // back to the account that was set up before
            }
        }
        Q_EMIT errorOccurred(message);
    });

    m_account = Account::load();
    connectSavedAccount();
}

MailController::~MailController() = default;

bool MailController::hasAccount() const
{
    return m_account.isValid();
}

QString MailController::accountEmail() const
{
    return m_account.email;
}

bool MailController::busy() const
{
    return m_requests > 0;
}

QAbstractListModel *MailController::folderModel() const
{
    return m_folderModel;
}

void MailController::setupAccount(const QString &provider,
                                  const QString &email,
                                  const QString &displayName,
                                  const QString &password,
                                  const QString &imapHost,
                                  int imapPort,
                                  const QString &smtpHost,
                                  int smtpPort)
{
    Account account;
    // applyPreset() clears the server fields first, so custom values are set afterwards.
    if (!Account::applyPreset(account, provider)) {
        account.imapHost = imapHost.trimmed();
        account.imapPort = quint16(imapPort);
        account.smtpHost = smtpHost.trimmed();
        account.smtpPort = quint16(smtpPort);
    }
    account.email = email.trimmed();
    account.displayName = displayName.trimmed();

    if (!account.isValid() || imapPort <= 0 || imapPort > 65535) {
        Q_EMIT errorOccurred(i18n("Please enter an email address and a valid IMAP server and port."));
        return;
    }
    if (password.isEmpty()) {
        Q_EMIT errorOccurred(i18n("Please enter the password."));
        return;
    }

    m_pendingAccount = account;
    m_pendingPassword = password;
    connectAccount(account, password);
}

void MailController::removeAccount()
{
    m_client->disconnectFromServer();
    resetRequests();
    m_pendingAccount = {};
    m_pendingPassword.clear();
    if (m_account.isValid()) {
        connect(
            m_secrets,
            &SecretStore::passwordDeleted,
            this,
            [this](const QString &key, const QString &error) {
                if (!error.isEmpty()) {
                    Q_EMIT errorOccurred(i18n("Could not delete the password for %1 from the keychain: %2", key, error));
                }
            },
            Qt::SingleShotConnection);
        m_secrets->deletePassword(m_account.email);
    }
    Account::remove();
    m_account = {};
    m_folderModel->setFolders({});
    Q_EMIT accountChanged();
}

void MailController::connectSavedAccount()
{
    if (!m_account.isValid()) {
        return;
    }
    connect(
        m_secrets,
        &SecretStore::passwordRead,
        this,
        [this](const QString &key, const QString &password, const QString &error) {
            endRequest();
            if (key != m_account.email) {
                return; // the account changed meanwhile
            }
            if (!error.isEmpty()) {
                Q_EMIT errorOccurred(i18n("Could not read the password for %1 from the keychain: %2", key, error));
                return;
            }
            connectAccount(m_account, password);
        },
        Qt::SingleShotConnection);
    beginRequest();
    m_secrets->readPassword(m_account.email);
}

void MailController::connectAccount(const Account &account, const QString &password)
{
    m_client->setAccount(account, password);
    beginRequest();
    m_client->listFolders();
}

void MailController::onFoldersListed(const QList<FolderInfo> &folders)
{
    endRequest();
    m_folderModel->setFolders(folders);

    if (!m_pendingAccount.isValid()) {
        return;
    }
    // The login worked: now store the new account. If another account was set up before,
    // its password is removed from the keychain.
    const Account previous = m_account;
    const Account account = m_pendingAccount;
    m_pendingAccount = {};
    const QString password = std::exchange(m_pendingPassword, QString());

    connect(
        m_secrets,
        &SecretStore::passwordWritten,
        this,
        [this, previous, account](const QString &key, const QString &error) {
            if (!error.isEmpty()) {
                Q_EMIT errorOccurred(i18n("Could not save the password for %1 in the keychain: %2", key, error));
                return;
            }
            if (previous.isValid() && previous.email != account.email) {
                m_secrets->deletePassword(previous.email);
            }
            account.save();
            m_account = account;
            Q_EMIT accountChanged();
        },
        Qt::SingleShotConnection);
    m_secrets->writePassword(account.email, password);
}

void MailController::beginRequest()
{
    if (m_requests++ == 0) {
        Q_EMIT busyChanged();
    }
}

void MailController::endRequest()
{
    if (m_requests > 0 && --m_requests == 0) {
        Q_EMIT busyChanged();
    }
}

void MailController::resetRequests()
{
    if (m_requests > 0) {
        m_requests = 0;
        Q_EMIT busyChanged();
    }
}
