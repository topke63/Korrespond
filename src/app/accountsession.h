// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "folderutils.h"
#include "imapclient.h"
#include "storagesettings.h"

#include <QList>
#include <QObject>

#include <memory>

class FolderModel;
class MessageStore;
class SyncManager;

// Everything that belongs to one account: its IMAP connection, folder list, local store and
// sync. With storage on it saves the folder list and starts a sync by itself whenever the
// server's folder list arrives. The signals of the client are forwarded as they are.
class AccountSession : public QObject
{
    Q_OBJECT
public:
    // storeDir is this account's own store folder (see MessageStore::accountDir()).
    AccountSession(const Account &account, const QString &storeDir, const StorageSettings &storage, QObject *parent = nullptr);
    ~AccountSession() override;

    [[nodiscard]] const Account &account() const;
    [[nodiscard]] ImapClient *client() const;
    [[nodiscard]] MessageStore *store() const;
    [[nodiscard]] FolderModel *folderModel() const;
    // The server's last folder list; empty before the first answer.
    [[nodiscard]] QList<FolderInfo> folders() const;
    [[nodiscard]] bool storageReady() const; // the store could be opened
    [[nodiscard]] bool storageActive() const; // storage on and the store open
    [[nodiscard]] qint64 storageUsedBytes() const; // 0 when storage is off or not ready
    [[nodiscard]] bool syncing() const;

    // Logs in (secret = password, or for OAuth accounts the access token) and lists the folders.
    void connectWithSecret(const QString &secret);
    // Fills the folder model from the stored folder list, before connecting. False when storage
    // is off or nothing is stored.
    bool showStoredFolders();

    // Applies new settings to the local copies (turning storage off deletes nothing here; see
    // clearStore()) and starts a sync when storage is on.
    void setStorageSettings(const StorageSettings &storage);
    // Removes the local copies that break the limits.
    void applyStorageLimits();
    // Deletes every local copy, e.g. when the account is removed.
    void clearStore();
    // Copies the chosen folders into local storage, if storage is on and no sync is running.
    void startSync();

Q_SIGNALS:
    // Emitted after the session saved the list and filled folderModel().
    void foldersListed(const QList<FolderInfo> &folders);
    void headersFetched(const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages);
    void messageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    void errorOccurred(const QString &message);
    void syncingChanged();
    void storageUsageChanged();

private:
    void onFoldersListed(const QList<FolderInfo> &folders);
    void updateStorageUsage();
    void setSyncing(bool syncing);

    Account m_account;
    ImapClient *m_client;
    FolderModel *m_folderModel;
    std::unique_ptr<MessageStore> m_store;
    SyncManager *m_sync;
    StorageSettings m_storage;
    bool m_storeReady = false;
    QList<FolderInfo> m_folders;
    qint64 m_storageUsedBytes = 0;
    bool m_syncing = false;
};
