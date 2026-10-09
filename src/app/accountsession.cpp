// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "accountsession.h"

#include "foldermodel.h"
#include "messagestore.h"
#include "oauthclient.h"
#include "retention.h"
#include "syncmanager.h"

#include <KLocalizedString>

#include <QDateTime>

AccountSession::AccountSession(const Account &account, const QString &storeDir, const StorageSettings &storage, QObject *parent)
    : QObject(parent)
    , m_account(account)
    , m_client(new ImapClient(this))
    , m_folderModel(new FolderModel(this))
    , m_store(std::make_unique<MessageStore>(storeDir))
    , m_sync(new SyncManager(m_client, m_store.get(), this))
    , m_storage(storage)
{
    // Connected before anyone else can, so the session has saved the list when its own
    // foldersListed() goes out.
    connect(m_client, &ImapClient::foldersListed, this, &AccountSession::onFoldersListed);
    connect(m_client, &ImapClient::headersFetched, this, &AccountSession::headersFetched);
    connect(m_client, &ImapClient::messageFetched, this, &AccountSession::messageFetched);
    connect(m_client, &ImapClient::messageMoved, this, &AccountSession::messageMoved);
    connect(m_client, &ImapClient::inboxChecked, this, &AccountSession::inboxChecked);
    connect(m_client, &ImapClient::errorOccurred, this, &AccountSession::errorOccurred);
    // Any answer from the server means the connection works; a fatal error (the session is gone)
    // means it does not. A refused command (e.g. no such folder) leaves the session, and so the state.
    connect(m_client, &ImapClient::headersFetched, this, &AccountSession::markConnected);
    connect(m_client, &ImapClient::messageFetched, this, &AccountSession::markConnected);
    connect(m_client, &ImapClient::messageMoved, this, &AccountSession::markConnected);
    connect(m_client, &ImapClient::inboxChecked, this, &AccountSession::markConnected);
    connect(m_client, &ImapClient::errorOccurred, this, [this](const QString &message) {
        if (!m_client->hasSession()) {
            setConnectionState(Failed, message);
        }
    });

    // Store errors are logged, not shown: the mail itself still works from the server.
    connect(m_sync, &SyncManager::progress, this, &AccountSession::updateStorageUsage);
    connect(m_sync, &SyncManager::progress, this, [this](const QString &folder, int done, int total) {
        setSyncText(i18nc("@info:status sync progress: folder, messages done of total", "Syncing %1 %2 / %3", m_folderModel->displayName(folder), done, total));
    });
    connect(m_sync, &SyncManager::syncFinished, this, [this] {
        setSyncing(false);
        updateStorageUsage();
    });
    connect(m_sync, &SyncManager::aborted, this, [this] {
        setSyncing(false);
        updateStorageUsage();
    });
    connect(m_sync, &SyncManager::errorOccurred, this, [this](const QString &message) {
        qWarning("Local storage sync failed for %s: %s", qPrintable(m_account.email), qPrintable(message));
        setSyncing(false);
        updateStorageUsage();
    });

    m_sync->setSettings(m_storage);
    m_storeReady = m_store->open();
    if (!m_storeReady) {
        qWarning("Cannot open the local message store of %s: %s", qPrintable(m_account.email), qPrintable(m_store->lastError()));
    }
    updateStorageUsage();
}

// SyncManager uses the store, so it goes first.
AccountSession::~AccountSession()
{
    delete m_sync;
}

AccountSession::ConnectionState AccountSession::connectionState() const
{
    return m_connectionState;
}

QString AccountSession::lastError() const
{
    return m_lastError;
}

QString AccountSession::stateName(ConnectionState state)
{
    switch (state) {
    case Connecting:
        return QStringLiteral("connecting");
    case Connected:
        return QStringLiteral("connected");
    case Failed:
        return QStringLiteral("failed");
    case Idle:
        break;
    }
    return QStringLiteral("idle");
}

void AccountSession::setSyncText(const QString &text)
{
    if (text != m_syncText) {
        m_syncText = text;
        Q_EMIT syncTextChanged();
    }
}

QString AccountSession::syncText() const
{
    return m_syncText;
}

void AccountSession::setConnectionState(ConnectionState state, const QString &error)
{
    if (state == m_connectionState && error == m_lastError) {
        return;
    }
    m_connectionState = state;
    m_lastError = state == Failed ? error : QString();
    Q_EMIT connectionStateChanged();
}

void AccountSession::markConnected()
{
    // Answers that arrive while idle (stored mail, a stale answer) say nothing about the connection.
    if (m_connectionState == Connecting || m_connectionState == Failed) {
        setConnectionState(Connected);
    }
}

const Account &AccountSession::account() const
{
    return m_account;
}

ImapClient *AccountSession::client() const
{
    return m_client;
}

MessageStore *AccountSession::store() const
{
    return m_store.get();
}

FolderModel *AccountSession::folderModel() const
{
    return m_folderModel;
}

QList<FolderInfo> AccountSession::folders() const
{
    return m_folders;
}

bool AccountSession::storageReady() const
{
    return m_storeReady;
}

bool AccountSession::storageActive() const
{
    return m_storeReady && m_storage.enabled;
}

qint64 AccountSession::storageUsedBytes() const
{
    return m_storageUsedBytes;
}

bool AccountSession::syncing() const
{
    return m_syncing;
}

OAuthClient *AccountSession::oauth()
{
    if (!m_oauth) {
        m_oauth = new OAuthClient(this);
        connect(m_oauth, &OAuthClient::signedIn, this, [this](const QString &accessToken, const QString &refreshToken) {
            m_refreshToken = refreshToken;
            onAccessToken(accessToken);
        });
        connect(m_oauth, &OAuthClient::accessTokenChanged, this, &AccountSession::onAccessToken);
        connect(m_oauth, &OAuthClient::failed, this, &AccountSession::errorOccurred);
        connect(m_oauth, &OAuthClient::failed, this, [this](const QString &message) {
            setConnectionState(Failed, message);
        });
    }
    return m_oauth;
}

// The first token connects; later ones only replace the secret for future logins.
void AccountSession::onAccessToken(const QString &accessToken)
{
    if (m_loggedIn) {
        m_client->setAccessToken(accessToken);
        return;
    }
    m_loggedIn = true;
    connectWithSecret(accessToken);
}

void AccountSession::signInWithGoogle()
{
    m_loggedIn = false;
    setConnectionState(Connecting);
    oauth()->signIn(m_account.email);
}

void AccountSession::connectWithRefreshToken(const QString &refreshToken)
{
    m_loggedIn = false;
    setConnectionState(Connecting);
    m_refreshToken = refreshToken;
    oauth()->refresh(refreshToken);
}

QString AccountSession::refreshToken() const
{
    return m_refreshToken;
}

void AccountSession::connectWithSecret(const QString &secret)
{
    setConnectionState(Connecting);
    // setAccount() disconnects, which a running sync would never hear about.
    m_sync->stop();
    setSyncing(false);
    m_client->setAccount(m_account, secret);
    m_client->listFolders();
}

bool AccountSession::showStoredFolders()
{
    if (!storageActive()) {
        return false;
    }
    const QList<FolderInfo> stored = m_store->folders();
    if (stored.isEmpty()) {
        return false;
    }
    // Not m_folders: sync waits for the server's own list.
    m_folderModel->setFolders(stored);
    return true;
}

void AccountSession::onFoldersListed(const QList<FolderInfo> &folders)
{
    markConnected();
    m_folders = folders;
    m_folderModel->setFolders(folders);
    if (storageActive() && !m_store->setFolders(folders)) {
        qWarning("Cannot store the folder list of %s: %s", qPrintable(m_account.email), qPrintable(m_store->lastError()));
    }
    Q_EMIT foldersListed(folders);
    startSync();
}

void AccountSession::setStorageSettings(const StorageSettings &storage)
{
    m_storage = storage;
    m_sync->setSettings(storage);
    if (!storage.enabled && m_syncing) {
        m_sync->stop(); // no point downloading what is deleted next
        setSyncing(false);
    }
    applyStorageLimits();
    // The folder list may have arrived while storage was still off.
    if (storageActive() && !m_folders.isEmpty() && !m_store->setFolders(m_folders)) {
        qWarning("Cannot store the folder list of %s: %s", qPrintable(m_account.email), qPrintable(m_store->lastError()));
    }
    startSync();
}

void AccountSession::applyStorageLimits()
{
    if (!m_storeReady) {
        return;
    }
    // A running sync applies the limits itself when it finishes each folder.
    if (!m_syncing) {
        const RetentionResult result = applyRetention(*m_store, m_storage, QDateTime::currentDateTime());
        if (!result.ok) {
            qWarning("Local storage cleanup failed for %s: %s", qPrintable(m_account.email), qPrintable(m_store->lastError()));
        }
    }
    updateStorageUsage();
}

void AccountSession::clearStore()
{
    m_sync->stop();
    setSyncing(false);
    if (m_storeReady && !m_store->clear()) {
        qWarning("Cannot delete the local copies of %s: %s", qPrintable(m_account.email), qPrintable(m_store->lastError()));
    }
    updateStorageUsage();
}

void AccountSession::startSync()
{
    if (!m_storeReady || !m_storage.enabled || m_syncing || m_folders.isEmpty()) {
        return;
    }
    setSyncing(true);
    m_sync->syncAll(m_folders);
}

void AccountSession::updateStorageUsage()
{
    const qint64 used = m_storeReady ? m_store->totalSize() : 0;
    if (used != m_storageUsedBytes) {
        m_storageUsedBytes = used;
        Q_EMIT storageUsageChanged();
    }
}

void AccountSession::setSyncing(bool syncing)
{
    if (!syncing) {
        setSyncText({}); // a sync that has ended has nothing to report
    }
    if (syncing != m_syncing) {
        m_syncing = syncing;
        Q_EMIT syncingChanged();
    }
}
