// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "folderutils.h"
#include "imapclient.h"
#include "storagesettings.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

#include <map>
#include <memory>
#include <set>

namespace KMime
{
class Message;
}
class AccountListModel;
class AccountSession;
class FolderModel;
class MessageListModel;
class MessageStore;
class SecretStore;

// The only object the QML UI talks to. Owns one AccountSession per saved account. The QML API
// below works on the first account (the one the UI shows); `accounts` lists all of them.
class MailController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasAccount READ hasAccount NOTIFY accountChanged)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountChanged)
    Q_PROPERTY(QString accountDisplayName READ accountDisplayName NOTIFY accountChanged)
    Q_PROPERTY(QString accountProvider READ accountProvider NOTIFY accountChanged) // readable, e.g. "Yahoo"
    Q_PROPERTY(QString accountServer READ accountServer NOTIFY accountChanged) // "host:port" of the IMAP server
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    // Every saved account, in the order they were added.
    Q_PROPERTY(QAbstractListModel *accounts READ accounts CONSTANT)
    // The folders of the first account; empty without an account.
    Q_PROPERTY(QAbstractListModel *folderModel READ folderModel NOTIFY accountChanged)
    Q_PROPERTY(QAbstractListModel *messageModel READ messageModel CONSTANT)
    // Email of the account whose folder is open; empty for the shared views.
    Q_PROPERTY(QString currentAccount READ currentAccount NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderChanged)
    // Views over every account's inbox, shown above the accounts: list of {path, name, iconName}.
    Q_PROPERTY(QVariantList sharedFolders READ sharedFolders CONSTANT)
    // Email of the account the open message belongs to.
    Q_PROPERTY(QString currentMessageAccount READ currentMessageAccount NOTIFY currentMessageChanged)
    Q_PROPERTY(qint64 currentUid READ currentUid NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageSubject READ messageSubject NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageFrom READ messageFrom NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageTo READ messageTo NOTIFY currentMessageChanged)
    Q_PROPERTY(QDateTime messageDate READ messageDate NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageHtml READ messageHtml NOTIFY currentMessageChanged)
    Q_PROPERTY(QStringList messageAttachments READ messageAttachments NOTIFY currentMessageChanged)
    Q_PROPERTY(bool messageFlagged READ messageFlagged NOTIFY currentMessageChanged)
    Q_PROPERTY(bool messageUnread READ messageUnread NOTIFY currentMessageChanged)
    // Whether Archive / Delete can act on the open message: the account has an archive (or Gmail
    // All Mail) / Trash folder, the message is not in it already, and it is not being moved.
    Q_PROPERTY(bool canArchive READ canArchive NOTIFY currentMessageChanged)
    Q_PROPERTY(bool canDelete READ canDelete NOTIFY currentMessageChanged)
    // Local storage settings. importSince is an ISO date ("yyyy-MM-dd"), empty = all mail; the
    // units are StorageSettings::KeepUnit / SizeUnit as ints.
    Q_PROPERTY(bool storageEnabled READ storageEnabled NOTIFY storageSettingsChanged)
    Q_PROPERTY(QString importSince READ importSince NOTIFY storageSettingsChanged)
    Q_PROPERTY(int keepValue READ keepValue NOTIFY storageSettingsChanged)
    Q_PROPERTY(int keepUnit READ keepUnit NOTIFY storageSettingsChanged)
    Q_PROPERTY(int sizeValue READ sizeValue NOTIFY storageSettingsChanged)
    Q_PROPERTY(int sizeUnit READ sizeUnit NOTIFY storageSettingsChanged)
    // Bytes the local copies take on disk; 0 when storage is off or not ready.
    Q_PROPERTY(qint64 storageUsedBytes READ storageUsedBytes NOTIFY storageUsageChanged)
    // True while mail is being copied from the server into local storage.
    Q_PROPERTY(bool syncing READ syncing NOTIFY syncingChanged)
    // False when the local store could not be opened; usage, cleanup and sync then do nothing.
    Q_PROPERTY(bool storageReady READ storageReady NOTIFY accountChanged)
    // The Google OAuth client is set up (read once at startup).
    Q_PROPERTY(bool googleConfigured READ googleConfigured CONSTANT)

public:
    explicit MailController(QObject *parent = nullptr);
    ~MailController() override;

    [[nodiscard]] bool hasAccount() const;
    [[nodiscard]] QString accountEmail() const;
    [[nodiscard]] QString accountDisplayName() const;
    [[nodiscard]] QString accountProvider() const;
    [[nodiscard]] QString accountServer() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QAbstractListModel *accounts() const;
    [[nodiscard]] QAbstractListModel *folderModel() const;
    [[nodiscard]] QAbstractListModel *messageModel() const;
    [[nodiscard]] QString currentAccount() const;
    [[nodiscard]] QString currentFolder() const;
    [[nodiscard]] QString currentFolderName() const;
    [[nodiscard]] QVariantList sharedFolders() const;
    [[nodiscard]] QString currentMessageAccount() const;
    [[nodiscard]] qint64 currentUid() const;
    [[nodiscard]] QString messageSubject() const;
    [[nodiscard]] QString messageFrom() const;
    [[nodiscard]] QString messageTo() const;
    [[nodiscard]] QDateTime messageDate() const;
    [[nodiscard]] QString messageHtml() const;
    [[nodiscard]] QStringList messageAttachments() const;
    [[nodiscard]] bool messageFlagged() const;
    [[nodiscard]] bool messageUnread() const;
    [[nodiscard]] bool canArchive() const;
    [[nodiscard]] bool canDelete() const;
    [[nodiscard]] bool storageEnabled() const;
    [[nodiscard]] QString importSince() const;
    [[nodiscard]] int keepValue() const;
    [[nodiscard]] int keepUnit() const;
    [[nodiscard]] int sizeValue() const;
    [[nodiscard]] int sizeUnit() const;
    [[nodiscard]] qint64 storageUsedBytes() const;
    [[nodiscard]] bool syncing() const;
    [[nodiscard]] bool storageReady() const;
    [[nodiscard]] bool googleConfigured() const;

    // Checks the credentials by logging in and listing folders; only then stores the password
    // in the keychain and adds the account after the existing ones (an email that is already
    // added is refused). On failure nothing is saved and errorOccurred is emitted. provider: "yahoo", "gmail", "outlook" or "custom" (custom uses
    // the host/port arguments; the others use their presets).
    Q_INVOKABLE void setupAccount(const QString &provider,
                                  const QString &email,
                                  const QString &displayName,
                                  const QString &password,
                                  const QString &imapHost = {},
                                  int imapPort = 993,
                                  const QString &smtpHost = {},
                                  int smtpPort = 465);
    // Adds a Gmail account with Google's browser sign-in (OAuth2, no password): opens the browser,
    // then verifies by logging in and listing folders, keeps the refresh token in the keychain
    // and adds the account. Needs googleConfigured. On failure nothing is saved and
    // errorOccurred is emitted.
    Q_INVOKABLE void setupGoogleAccount(const QString &email, const QString &displayName);
    // Gives up an account setup that is still waiting (e.g. for the browser).
    Q_INVOKABLE void cancelSetup();
    // Disconnects one account and deletes its password from the keychain, its local copies and
    // its entry in the config. The other accounts keep running.
    Q_INVOKABLE void removeAccount(const QString &account);
    // Removes the first account; kept until the settings pass the account (T96).
    Q_INVOKABLE void removeAccount();
    // Shows the newest messages of `path` of `account` (clears the list first); for a shared view
    // path the account is ignored (pass an empty string). With storage on, the stored
    // copies show at once and the server's list replaces them when it arrives.
    Q_INVOKABLE void selectFolder(const QString &account, const QString &path);
    // Reloads the folder list and the current folder's messages.
    Q_INVOKABLE void refresh();
    // Shows message `uid` of the current folder, from its local copy when there is one, else
    // fetched from the server; marks it read if it was unread (on the server and locally).
    // The message properties stay empty until it has arrived.
    Q_INVOKABLE void openMessage(const QString &account, qint64 uid);
    // Flags or unflags message `uid` of the current list (or the open message): updates the list
    // and the open message at once, then stores the flag on the server.
    Q_INVOKABLE void setFlagged(const QString &account, qint64 uid, bool flagged);
    // Marks message `uid` of `account` unread or read: updates the list, the local copy and the
    // open message at once, then stores the \Seen flag on the server. An unread message stays
    // unread while it is open.
    Q_INVOKABLE void setUnread(const QString &account, qint64 uid, bool unread);
    // Move the open message to the account's archive / Trash folder, or to any folder of its
    // account (`folderPath`). The server moves it first; only then it leaves the list and the
    // local copies, and the next message opens (or none, after the last one). On failure
    // nothing changes and errorOccurred is emitted. Mail is never deleted for good.
    Q_INVOKABLE void archiveMessage();
    Q_INVOKABLE void deleteMessage();
    Q_INVOKABLE void moveMessage(const QString &folderPath);
    // Saves the local storage settings (arguments as the properties), applies them to the local
    // copies at once (turning storage off deletes them) and starts a sync when storage is on.
    Q_INVOKABLE void saveStorageSettings(bool enabled, const QString &importSince, int keepValue, int keepUnit, int sizeValue, int sizeUnit);
    // The values storage starts with when first turned on, as a map with the property names.
    Q_INVOKABLE QVariantMap defaultStorageSettings() const;
    // Applies the storage limits now, removing local copies that are too old or too many.
    Q_INVOKABLE void cleanUpNow();

    // Reserved paths of the shared views (not valid IMAP folder names we would ever list).
    static constexpr auto allInboxesPath = "plasmamail:inboxes";
    static constexpr auto unreadPath = "plasmamail:unread";
    static constexpr auto flaggedPath = "plasmamail:flagged";

    // How many of the newest messages a folder shows.
    static constexpr int messageLimit = 50;

Q_SIGNALS:
    void accountChanged();
    void busyChanged();
    void currentFolderChanged();
    void currentMessageChanged();
    void storageSettingsChanged();
    void storageUsageChanged();
    void syncingChanged();
    void errorOccurred(const QString &message);

private:
    // The session the UI shows: the first saved account, or while the very first account is
    // being verified, that one. nullptr without either.
    [[nodiscard]] AccountSession *uiSession() const;
    [[nodiscard]] AccountSession *sessionFor(const QString &account) const; // saved accounts only
    // The session of the open folder's account; the UI session for the shared views.
    [[nodiscard]] AccountSession *currentSession() const;
    [[nodiscard]] QString currentEmail() const; // empty without a session
    AccountSession *createSession(const Account &account);
    // Stops a session and deletes it; with deleteStore also its local copies and folder.
    void discardSession(AccountSession *session, bool deleteStore);
    [[nodiscard]] bool isKnown(const AccountSession *session) const;
    // Reads the password of every saved password account from the keychain and connects them.
    void connectSavedAccounts();
    void onSecretRead(const QString &key, const QString &password, const QString &error);
    void onSessionError(AccountSession *session, const QString &message);
    void onMessageMoved(AccountSession *session, const QString &from, qint64 uid, const QString &to);
    void moveOpenMessage(const QString &target);
    [[nodiscard]] static QString moveKey(const QString &account, const QString &folder, qint64 uid);
    void onSessionFolders(AccountSession *session);
    void beginRequest();
    void endRequest();
    void resetRequests();
    // Shows the uiSession()'s folder list: keeps the current folder if it still exists, else
    // opens All Inboxes.
    void applyFolderList();
    void saveAccounts() const; // writes the sessions' accounts to the config, in order
    void fetchSharedPart(AccountSession *session);
    void mergeSharedParts();
    // The IMAP folder and filter behind a folder or shared-view path. A shared view reads each
    // account's own inbox.
    [[nodiscard]] static QString imapFolderFor(const AccountSession *session, const QString &path);
    [[nodiscard]] static ImapClient::HeaderFilter filterFor(const QString &path);
    [[nodiscard]] static bool isSharedView(const QString &path);
    void onHeadersFetched(AccountSession *session, const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages);
    void onMessageFetched(AccountSession *session, const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    void showMessage(AccountSession *session, const QString &folder, qint64 uid, const KMime::Message &message);
    // The stored messages for a folder or shared-view path, filtered like the server's list.
    [[nodiscard]] QList<MessageSummary> storedMessages(const AccountSession *session, const QString &path) const;
    [[nodiscard]] bool storageActive() const; // storage on and the open account's store open
    // Empties the message properties; with clearUid also forgets which message is open.
    void clearMessage(bool clearUid);
    // With storage on: shows the stored folder list and All Inboxes before connecting.
    void showStoredMail();
    void updateStorageUsage();
    void updateSyncing();

    QString m_dataDir; // AppDataLocation
    QList<AccountSession *> m_sessions; // saved accounts, in order
    AccountSession *m_pending = nullptr; // being verified by setupAccount()
    QString m_pendingPassword;
    bool m_googleConfigured = false;
    // The open shared view: each account's part of the merged list, and the accounts already asked.
    std::map<QString, QList<MessageSummary>> m_sharedParts;
    std::set<AccountSession *> m_sharedRequested;
    std::set<QString> m_moving; // moveKey()s of messages the server is moving
    std::set<AccountSession *> m_awaitingFolders; // sessions with a folder-list request in flight (counted as busy)
    AccountListModel *m_accounts;
    FolderModel *m_noFolders; // shown when there is no session
    SecretStore *m_secrets;
    MessageListModel *m_messageModel;
    QString m_currentAccount; // email of the open folder's account; empty for a shared view
    QString m_currentFolder; // an IMAP folder path or a shared-view path
    qint64 m_currentUid = 0; // 0 when no message is open
    QString m_messageAccount; // account of the open message
    QString m_messageFolder; // folder of the open message
    QString m_messageSubject;
    QString m_messageFrom;
    QString m_messageTo;
    QDateTime m_messageDate;
    QString m_messageHtml;
    QStringList m_messageAttachments;
    bool m_messageFlagged = false;
    bool m_messageUnread = false;
    StorageSettings m_storage;
    qint64 m_storageUsedBytes = 0;
    bool m_syncing = false;
    bool m_showingStored = false; // the view came from the store; refresh it once connected
    int m_requests = 0;
};
