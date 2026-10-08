// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailcontroller.h"

#include "foldermodel.h"
#include "imapclient.h"
#include "messagelistmodel.h"
#include "messagerenderer.h"
#include "messagestore.h"
#include "retention.h"
#include "secretstore.h"
#include "syncmanager.h"

#include <KLocalizedString>
#include <KMime/Message>

#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

MailController::MailController(QObject *parent)
    : QObject(parent)
    , m_client(new ImapClient(this))
    , m_secrets(new SecretStore(QString::fromLatin1(AccountSecrets::service), this))
    , m_folderModel(new FolderModel(this))
    , m_messageModel(new MessageListModel(this))
    , m_store(std::make_unique<MessageStore>(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)))
    , m_sync(new SyncManager(m_client, m_store.get(), this))
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

    // Store errors are logged, not shown: the mail itself still works from the server.
    connect(m_sync, &SyncManager::progress, this, &MailController::updateStorageUsage);
    connect(m_sync, &SyncManager::syncFinished, this, [this] {
        setSyncing(false);
        updateStorageUsage();
    });
    connect(m_sync, &SyncManager::aborted, this, [this] {
        setSyncing(false);
        updateStorageUsage();
    });
    connect(m_sync, &SyncManager::errorOccurred, this, [this](const QString &message) {
        qWarning("Local storage sync failed: %s", qPrintable(message));
        setSyncing(false);
        updateStorageUsage();
    });

    m_storage = StorageSettings::load();
    m_sync->setSettings(m_storage);
    m_storeReady = m_store->open();
    if (!m_storeReady) {
        qWarning("Cannot open the local message store: %s", qPrintable(m_store->lastError()));
    }
    updateStorageUsage();
    m_account = Account::load();
    connectSavedAccount();
}

// SyncManager uses the store, so it goes first.
MailController::~MailController()
{
    delete m_sync;
}

bool MailController::hasAccount() const
{
    return m_account.isValid();
}

QString MailController::accountEmail() const
{
    return m_account.email;
}

QString MailController::accountDisplayName() const
{
    return m_account.displayName;
}

QString MailController::accountProvider() const
{
    if (m_account.provider == "yahoo"_L1) {
        return i18nc("@info mail provider", "Yahoo");
    }
    if (m_account.provider == "gmail"_L1) {
        return i18nc("@info mail provider", "Gmail");
    }
    if (m_account.provider == "outlook"_L1) {
        return i18nc("@info mail provider", "Outlook");
    }
    return i18nc("@info mail provider", "Custom IMAP");
}

QString MailController::accountServer() const
{
    return m_account.isValid() ? u"%1:%2"_s.arg(m_account.imapHost).arg(m_account.imapPort) : QString();
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
    if (m_currentFolder == QLatin1StringView(allInboxesPath)) {
        return i18nc("@title shared folder", "All Inboxes");
    }
    if (m_currentFolder == QLatin1StringView(unreadPath)) {
        return i18nc("@title shared folder", "Unread");
    }
    if (m_currentFolder == QLatin1StringView(flaggedPath)) {
        return i18nc("@title shared folder", "Flagged");
    }
    return m_currentFolder.isEmpty() ? QString() : m_folderModel->displayName(m_currentFolder);
}

QVariantList MailController::sharedFolders() const
{
    const auto entry = [](const char *path, const QString &name, const QString &iconName) {
        return QVariantMap{{u"path"_s, QLatin1StringView(path)}, {u"name"_s, name}, {u"iconName"_s, iconName}};
    };
    return {
        entry(allInboxesPath, i18nc("@title shared folder", "All Inboxes"), u"mail-folder-inbox"_s),
        entry(unreadPath, i18nc("@title shared folder", "Unread"), u"mail-mark-unread"_s),
        entry(flaggedPath, i18nc("@title shared folder", "Flagged"), u"flag"_s),
    };
}

bool MailController::isSharedView(const QString &path)
{
    return path == QLatin1StringView(allInboxesPath) || path == QLatin1StringView(unreadPath) || path == QLatin1StringView(flaggedPath);
}

QString MailController::imapFolderFor(const QString &path) const
{
    return isSharedView(path) ? m_folderModel->inboxPath() : path;
}

ImapClient::HeaderFilter MailController::filterFor(const QString &path)
{
    if (path == QLatin1StringView(unreadPath)) {
        return ImapClient::Unread;
    }
    if (path == QLatin1StringView(flaggedPath)) {
        return ImapClient::Flagged;
    }
    return ImapClient::AllMessages;
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

bool MailController::messageFlagged() const
{
    return m_messageFlagged;
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
    m_sync->setSettings(settings);
    if (!settings.enabled && m_syncing) {
        m_sync->stop(); // no point downloading what is deleted next
        setSyncing(false);
    }
    Q_EMIT storageSettingsChanged();
    applyStorageLimits();
    startSync();
}

qint64 MailController::storageUsedBytes() const
{
    return m_storageUsedBytes;
}

bool MailController::syncing() const
{
    return m_syncing;
}

bool MailController::storageReady() const
{
    return m_storeReady;
}

void MailController::cleanUpNow()
{
    applyStorageLimits();
}

void MailController::applyStorageLimits()
{
    if (!m_storeReady) {
        return;
    }
    // A running sync applies the limits itself when it finishes each folder.
    if (!m_syncing) {
        const RetentionResult result = applyRetention(*m_store, m_storage, QDateTime::currentDateTime());
        if (!result.ok) {
            qWarning("Local storage cleanup failed: %s", qPrintable(m_store->lastError()));
        }
    }
    updateStorageUsage();
}

void MailController::startSync()
{
    // Only for the saved account: a new one is synced once it has been saved.
    if (!m_storeReady || !m_storage.enabled || m_syncing || m_folders.isEmpty() || !m_account.isValid() || m_pendingAccount.isValid()) {
        return;
    }
    setSyncing(true);
    m_sync->syncAll(m_folders);
}

void MailController::updateStorageUsage()
{
    const qint64 used = m_storeReady ? m_store->totalSize() : 0;
    if (used != m_storageUsedBytes) {
        m_storageUsedBytes = used;
        Q_EMIT storageUsageChanged();
    }
}

void MailController::setSyncing(bool syncing)
{
    if (syncing != m_syncing) {
        m_syncing = syncing;
        Q_EMIT syncingChanged();
    }
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
    const QString folder = imapFolderFor(path);
    if (folder.isEmpty()) {
        return; // a shared view before the folders (and so the inbox) are known
    }
    if (storageActive() && m_messageModel->rowCount() == 0) {
        m_messageModel->setMessages(storedMessages(path));
    }
    beginRequest();
    m_client->fetchHeaders(folder, messageLimit, filterFor(path));
}

bool MailController::storageActive() const
{
    return m_storeReady && m_storage.enabled;
}

QList<MessageSummary> MailController::storedMessages(const QString &path) const
{
    const QString folder = imapFolderFor(path);
    const ImapClient::HeaderFilter filter = filterFor(path);
    if (filter == ImapClient::AllMessages) {
        return m_store->messages(folder, messageLimit);
    }
    QList<MessageSummary> result;
    for (const MessageSummary &message : m_store->messages(folder, -1)) {
        if (filter == ImapClient::Unread ? message.unread : message.flagged) {
            result.append(message);
            if (result.size() == messageLimit) {
                break;
            }
        }
    }
    return result;
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

void MailController::onHeadersFetched(const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages)
{
    endRequest();
    if (folder != imapFolderFor(m_currentFolder) || filter != filterFor(m_currentFolder)) {
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
    m_messageFolder = imapFolderFor(m_currentFolder);
    clearMessage(false);
    m_messageFlagged = m_messageModel->message(uid).flagged; // the parsed message carries no IMAP flags
    Q_EMIT currentMessageChanged(); // the flag shows while the message is still loading
    if (storageActive()) {
        if (const QByteArray raw = m_store->rawMessage(m_messageFolder, uid); !raw.isEmpty()) {
            // Parsed like KIMAP does with fetched messages.
            KMime::Message message;
            message.setContent(KMime::CRLFtoLF(raw));
            message.parse();
            showMessage(m_messageFolder, uid, message);
            return;
        }
    }
    beginRequest();
    m_client->fetchMessage(m_messageFolder, uid);
}

void MailController::setFlagged(qint64 uid, bool flagged)
{
    if (uid <= 0) {
        return;
    }
    // The open message may belong to a folder the list no longer shows.
    const QString folder = uid == m_currentUid ? m_messageFolder : imapFolderFor(m_currentFolder);
    if (folder.isEmpty()) {
        return;
    }
    m_messageModel->setFlagged(uid, flagged);
    if (uid == m_currentUid && m_messageFlagged != flagged) {
        m_messageFlagged = flagged;
        Q_EMIT currentMessageChanged();
    }
    m_client->setFlagged(folder, uid, flagged);
}

void MailController::onMessageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message)
{
    endRequest();
    if (uid != m_currentUid || folder != m_messageFolder || !message) {
        return; // another message was opened while this one was loading
    }
    showMessage(folder, uid, *message);
}

void MailController::showMessage(const QString &folder, qint64 uid, const KMime::Message &msg)
{
    // The const getters return nullptr for missing headers instead of creating empty ones.
    m_messageSubject = msg.subject() ? msg.subject()->asUnicodeString() : QString();
    m_messageFrom = msg.from() ? msg.from()->asUnicodeString() : QString(); // "Name <address>"
    m_messageTo = msg.to() ? msg.to()->displayString() : QString();
    m_messageDate = ::messageDate(msg); // falls back to the Received date
    const QPalette palette = QGuiApplication::palette();
    m_messageHtml = messageBodyHtml(msg, palette.color(QPalette::Text), palette.color(QPalette::Base));
    m_messageAttachments = attachmentNames(msg);
    Q_EMIT currentMessageChanged();

    if (folder == imapFolderFor(m_currentFolder) && m_messageModel->message(uid).unread) {
        m_client->markSeen(folder, uid);
        m_messageModel->setUnread(uid, false);
        if (storageActive() && m_store->contains(folder, uid)) {
            m_store->setUnread(folder, uid, false);
        }
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
    m_messageFlagged = false;
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
    m_sync->stop();
    setSyncing(false);
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
    m_folders.clear();
    // The local copies belong to the removed account.
    if (m_storeReady && !m_store->clear()) {
        qWarning("Cannot delete the local copies: %s", qPrintable(m_store->lastError()));
    }
    updateStorageUsage();
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
    // setAccount() disconnects, which a running sync would never hear about.
    m_sync->stop();
    setSyncing(false);
    m_client->setAccount(account, password);
    beginRequest();
    m_client->listFolders();
}

void MailController::onFoldersListed(const QList<FolderInfo> &folders)
{
    endRequest();
    m_folders = folders;
    m_folderModel->setFolders(folders);
    const bool currentStillExists = isSharedView(m_currentFolder) || std::any_of(folders.cbegin(), folders.cend(), [this](const FolderInfo &f) {
        return f.path == m_currentFolder;
    });
    if (!currentStillExists) {
        // First list after connecting, or the folder was deleted elsewhere: show All Inboxes.
        m_currentFolder.clear();
        m_messageModel->clear();
        clearMessage(true);
        Q_EMIT currentFolderChanged();
        selectFolder(QLatin1StringView(allInboxesPath));
    } else {
        Q_EMIT currentFolderChanged(); // the display name may have changed
    }

    if (!m_pendingAccount.isValid()) {
        startSync();
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
                // The local copies are the previous account's mail.
                if (m_storeReady && !m_store->clear()) {
                    qWarning("Cannot delete the local copies: %s", qPrintable(m_store->lastError()));
                }
                updateStorageUsage();
            }
            account.save();
            m_account = account;
            Q_EMIT accountChanged();
            startSync();
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
