// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "account.h"
#include "addresshistory.h"
#include "folderutils.h"
#include "imapclient.h"
#include "imapidlewatcher.h"
#include "storagesettings.h"

#include <QList>
#include <QObject>

#include <memory>

class FolderModel;
class MessageStore;
class OAuthClient;
class SyncManager;

// Everything that belongs to one account: its IMAP connection, folder list, local store and
// sync. With storage on it saves the folder list and starts a sync by itself whenever the
// server's folder list arrives. The signals of the client are forwarded as they are.
class AccountSession : public QObject
{
    Q_OBJECT
public:
    // storeDir is this account's own store folder (see MessageStore::accountDir()).
    // demo: dev hook (KORRESPOND_DEMO); the store is never opened, so nothing is written.
    AccountSession(const Account &account, const QString &storeDir, const StorageSettings &storage, QObject *parent = nullptr, bool demo = false);
    ~AccountSession() override;

    // Where the connection to the server stands: Idle until a connect starts, Connecting while the
    // login / sign-in runs, Connected once the server has answered, Failed after a fatal error
    // (lastError() has the text; "connection lost" is one) until a later answer arrives.
    enum ConnectionState {
        Idle,
        Connecting,
        Connected,
        Failed
    };
    [[nodiscard]] ConnectionState connectionState() const;
    [[nodiscard]] QString lastError() const; // of the last Failed state, else empty
    // Google refused the saved sign-in (revoked, or expired after 7 days in a project in "Testing"
    // mode): the user has to sign in again. Until then no login is tried with the dead token, so
    // connectWithSecret() does nothing. A new sign-in or access token clears it.
    [[nodiscard]] bool needsSignIn() const;
    // "idle", "connecting", "connected" or "failed".
    [[nodiscard]] static QString stateName(ConnectionState state);

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
    // What the running sync is doing, e.g. "Syncing Inbox 120 / 340"; empty when none runs.
    [[nodiscard]] QString syncText() const;

    // Logs in (secret = password, or for OAuth accounts the access token) and lists the folders.
    void connectWithSecret(const QString &secret);
    // Google (OAuth) accounts: opens the browser to sign in, then logs in and lists the folders.
    // refreshToken() has the token to keep in the keychain once this has succeeded.
    // `withContacts` also asks for the contacts scopes (contactsGranted() tells what came back).
    void signInWithGoogle(bool withContacts = false);
    // Google accounts at startup: gets an access token from the saved refresh token, then logs in
    // and lists the folders. Later renewals reach the client by themselves.
    void connectWithRefreshToken(const QString &refreshToken);
    [[nodiscard]] QString refreshToken() const; // empty until signed in
    // The newest access token of a Google account (empty before the first), for the People API.
    [[nodiscard]] QString accessToken() const;
    // The last sign-in of this run was granted the contacts scopes (else the saved account says).
    [[nodiscard]] bool contactsGranted() const;
    // Signs in with Google again, in the browser, asking for the contacts scopes too. The mail
    // connection is not touched. Answers with contactsAuthorized() (refreshToken() is then the
    // token to keep) or contactsAuthFailed().
    void requestContactsAccess();
    // Push mail: an own connection in IDLE on INBOX (see ImapIdleWatcher). It logs in with the
    // secret the main connection uses now (kept in the client; renewed OAuth tokens included).
    // startWatcher() does nothing while a watcher runs; it is stopped whenever the main
    // connection fails, closes or reconnects, so the next folder list starts a fresh one.
    void startWatcher();
    void stopWatcher();
    [[nodiscard]] bool idling() const; // the watcher is in IDLE
    [[nodiscard]] ImapIdleWatcher::State pushState() const;
    // Fills the folder model from the stored folder list, before connecting. False when storage
    // is off or nothing is stored.
    bool showStoredFolders();
    // Dev hook: shows these folders as if the server had listed them (no signal, no store).
    void showDemoFolders(const QList<FolderInfo> &folders);

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
    // A new access token for a Google account (every renewal).
    void accessTokenChanged();
    void contactsAuthorized();
    void contactsAuthFailed(const QString &message);
    // Addresses of a message that was just stored (see SyncManager::addressesSeen).
    void addressesSeen(const QList<SeenAddress> &addresses, qint64 whenMs);
    // Emitted after the session saved the list and filled folderModel().
    void foldersListed(const QList<FolderInfo> &folders);
    void headersFetched(const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages);
    void messageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    void messageMoved(const QString &from, qint64 uid, const QString &to);
    void inboxChecked(const QString &folder, int unreadTotal, const QList<MessageSummary> &newestUnread);
    void errorOccurred(const QString &message);
    void moveFailed(const QString &folder, qint64 uid, const QString &message);
    void sessionLost(const QString &message);
    void jobsDropped();
    void syncingChanged();
    void connectionStateChanged();
    void needsSignInChanged();
    void syncTextChanged();
    void storageUsageChanged();
    void inboxChanged(); // the watcher heard of a change in INBOX
    void pushStateChanged();

private:
    friend class AccountSessionTest;

    void setNeedsSignIn(bool needed);
    void onFoldersListed(const QList<FolderInfo> &folders);
    void setConnectionState(ConnectionState state, const QString &error = {});
    void setSyncText(const QString &text);
    void markConnected(); // the server answered
    OAuthClient *oauth();
    void onAccessToken(const QString &accessToken);
    void updateStorageUsage();
    void setSyncing(bool syncing);

    Account m_account;
    OAuthClient *m_oauth = nullptr; // created for Google accounts
    OAuthClient *m_contactsOauth = nullptr; // the sign-in that asks for the contacts scopes
    QString m_refreshToken;
    QString m_accessToken;
    QString m_contactsToken; // from m_contactsOauth
    ConnectionState m_connectionState = Idle;
    QString m_lastError;
    bool m_needsSignIn = false;
    QString m_syncText;
    bool m_loggedIn = false; // the first access token has been used to connect
    ImapClient *m_client;
    ImapIdleWatcher *m_watcher = nullptr; // created by startWatcher()
    FolderModel *m_folderModel;
    std::unique_ptr<MessageStore> m_store;
    SyncManager *m_sync;
    StorageSettings m_storage;
    bool m_demo = false; // KORRESPOND_DEMO: never connects
    bool m_storeReady = false;
    QList<FolderInfo> m_folders;
    qint64 m_storageUsedBytes = 0;
    bool m_syncing = false;
};
