// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailcontroller.h"

#include "foldermodel.h"
#include "imapclient.h"
#include "messagelistmodel.h"
#include "messagerenderer.h"
#include "secretstore.h"

#include <KLocalizedString>
#include <KMime/Message>

#include <QGuiApplication>
#include <QPalette>

#include <algorithm>

using namespace Qt::StringLiterals;

MailController::MailController(QObject *parent)
    : QObject(parent)
    , m_client(new ImapClient(this))
    , m_secrets(new SecretStore(QString::fromLatin1(AccountSecrets::service), this))
    , m_folderModel(new FolderModel(this))
    , m_messageModel(new MessageListModel(this))
{
    connect(m_client, &ImapClient::foldersListed, this, &MailController::onFoldersListed);
    connect(m_client, &ImapClient::headersFetched, this, &MailController::onHeadersFetched);
    connect(m_client, &ImapClient::messageFetched, this, &MailController::onMessageFetched);
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

    m_storage = StorageSettings::load();
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

QAbstractListModel *MailController::messageModel() const
{
    return m_messageModel;
}

QString MailController::currentFolder() const
{
    return m_currentFolder;
}

QString MailController::currentFolderName() const
{
    return m_currentFolder.isEmpty() ? QString() : m_folderModel->displayName(m_currentFolder);
}

qint64 MailController::currentUid() const
{
    return m_currentUid;
}

QString MailController::messageSubject() const
{
    return m_messageSubject;
}

QString MailController::messageFrom() const
{
    return m_messageFrom;
}

QString MailController::messageTo() const
{
    return m_messageTo;
}

QDateTime MailController::messageDate() const
{
    return m_messageDate;
}

QString MailController::messageHtml() const
{
    return m_messageHtml;
}

QStringList MailController::messageAttachments() const
{
    return m_messageAttachments;
}

bool MailController::storageEnabled() const
{
    return m_storage.enabled;
}

QString MailController::importSince() const
{
    return m_storage.importSince.isValid() ? m_storage.importSince.toString(Qt::ISODate) : QString();
}

int MailController::keepValue() const
{
    return m_storage.keepValue;
}

int MailController::keepUnit() const
{
    return m_storage.keepUnit;
}

int MailController::sizeValue() const
{
    return m_storage.sizeValue;
}

int MailController::sizeUnit() const
{
    return m_storage.sizeUnit;
}

void MailController::saveStorageSettings(bool enabled, const QString &importSince, int keepValue, int keepUnit, int sizeValue, int sizeUnit)
{
    StorageSettings settings;
    settings.enabled = enabled;
    settings.importSince = QDate::fromString(importSince, Qt::ISODate); // empty → null = all mail
    settings.keepValue = std::max(1, keepValue);
    settings.keepUnit = static_cast<StorageSettings::KeepUnit>(std::clamp(keepUnit, int(StorageSettings::Days), int(StorageSettings::Forever)));
    settings.sizeValue = std::max(1, sizeValue);
    settings.sizeUnit = static_cast<StorageSettings::SizeUnit>(std::clamp(sizeUnit, int(StorageSettings::MB), int(StorageSettings::Unlimited)));
    settings.save();
    m_storage = settings;
    Q_EMIT storageSettingsChanged();
}

QVariantMap MailController::defaultStorageSettings() const
{
    const StorageSettings settings = StorageSettings::defaults(QDate::currentDate());
    return {
        {u"importSince"_s, settings.importSince.toString(Qt::ISODate)},
        {u"keepValue"_s, settings.keepValue},
        {u"keepUnit"_s, int(settings.keepUnit)},
        {u"sizeValue"_s, settings.sizeValue},
        {u"sizeUnit"_s, int(settings.sizeUnit)},
    };
}

void MailController::selectFolder(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    if (path != m_currentFolder) {
        m_currentFolder = path;
        m_messageModel->clear();
        clearMessage(true); // the open message belongs to the folder being left
        Q_EMIT currentFolderChanged();
    }
    beginRequest();
    m_client->fetchHeaders(path, messageLimit);
}

void MailController::refresh()
{
    if (!m_account.isValid() && !m_pendingAccount.isValid()) {
        return;
    }
    beginRequest();
    m_client->listFolders();
    if (!m_currentFolder.isEmpty()) {
        selectFolder(m_currentFolder);
    }
}

void MailController::onHeadersFetched(const QString &folder, const QList<MessageSummary> &messages)
{
    endRequest();
    if (folder != m_currentFolder) {
        return; // the user switched folders while this was loading
    }
    m_messageModel->setMessages(messages);
}

void MailController::openMessage(qint64 uid)
{
    if (uid <= 0 || m_currentFolder.isEmpty()) {
        return;
    }
    m_currentUid = uid;
    m_messageFolder = m_currentFolder;
    clearMessage(false);
    beginRequest();
    m_client->fetchMessage(m_messageFolder, uid);
}

void MailController::onMessageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message)
{
    endRequest();
    if (uid != m_currentUid || folder != m_messageFolder || !message) {
        return; // another message was opened while this one was loading
    }

    // The const getters return nullptr for missing headers instead of creating empty ones.
    const KMime::Message &msg = *message;
    m_messageSubject = msg.subject() ? msg.subject()->asUnicodeString() : QString();
    m_messageFrom = msg.from() ? msg.from()->displayString() : QString();
    m_messageTo = msg.to() ? msg.to()->displayString() : QString();
    m_messageDate = msg.date() ? msg.date()->dateTime() : QDateTime();
    const QPalette palette = QGuiApplication::palette();
    m_messageHtml = messageBodyHtml(msg, palette.color(QPalette::Text), palette.color(QPalette::Base));
    m_messageAttachments = attachmentNames(msg);
    Q_EMIT currentMessageChanged();

    if (folder == m_currentFolder && m_messageModel->message(uid).unread) {
        m_client->markSeen(folder, uid);
        m_messageModel->setUnread(uid, false);
    }
}

void MailController::clearMessage(bool clearUid)
{
    if (clearUid) {
        m_currentUid = 0;
        m_messageFolder.clear();
    }
    m_messageSubject.clear();
    m_messageFrom.clear();
    m_messageTo.clear();
    m_messageDate = {};
    m_messageHtml.clear();
    m_messageAttachments.clear();
    Q_EMIT currentMessageChanged();
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
    m_messageModel->clear();
    clearMessage(true);
    if (!m_currentFolder.isEmpty()) {
        m_currentFolder.clear();
        Q_EMIT currentFolderChanged();
    }
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
    const bool currentStillExists = std::any_of(folders.cbegin(), folders.cend(), [this](const FolderInfo &f) {
        return f.path == m_currentFolder;
    });
    if (!currentStillExists) {
        // First list after connecting, or the folder was deleted elsewhere: show the inbox.
        m_currentFolder.clear();
        m_messageModel->clear();
        clearMessage(true);
        Q_EMIT currentFolderChanged();
        selectFolder(m_folderModel->inboxPath());
    } else {
        Q_EMIT currentFolderChanged(); // the display name may have changed
    }

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
