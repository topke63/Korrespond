// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "appsettings.h"
#include "folderutils.h"
#include "imapclient.h"
#include "messagerenderer.h"
#include "newmailtracker.h"
#include "outgoingmessage.h"
#include "storagesettings.h"

#include <QAbstractListModel>
#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <optional>
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
class SmtpClient;

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
    // The open message has remote images that are not loaded yet / the user has loaded them.
    Q_PROPERTY(bool messageHasRemoteImages READ messageHasRemoteImages NOTIFY currentMessageChanged)
    Q_PROPERTY(bool remoteImagesLoaded READ remoteImagesLoaded NOTIFY currentMessageChanged)
    Q_PROPERTY(QStringList messageAttachments READ messageAttachments NOTIFY currentMessageChanged)
    Q_PROPERTY(bool messageFlagged READ messageFlagged NOTIFY currentMessageChanged)
    Q_PROPERTY(bool messageUnread READ messageUnread NOTIFY currentMessageChanged)
    // Whether Archive / Delete can act on the open message: the account has an archive (or Gmail
    // All Mail) / Trash folder, the message is not in it already, and it is not being moved.
    Q_PROPERTY(bool canArchive READ canArchive NOTIFY currentMessageChanged)
    Q_PROPERTY(bool canDelete READ canDelete NOTIFY currentMessageChanged)
    // The open message can be moved to another folder: its account is connected and it is not
    // being moved.
    Q_PROPERTY(bool canMove READ canMove NOTIFY currentMessageChanged)
    // Reply, Reply All and Forward can start from the open message (it has been loaded).
    Q_PROPERTY(bool canCompose READ canCompose NOTIFY currentMessageChanged)
    // A message is being sent (one at a time).
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
    // Messages waiting out the undo-send delay: [{id, subject, secondsLeft}], oldest first; and the
    // newest of them (empty map when none).
    Q_PROPERTY(QVariantList pendingSends READ pendingSends NOTIFY pendingSendChanged)
    Q_PROPERTY(QVariantMap pendingSend READ pendingSend NOTIFY pendingSendChanged)
    // The most the attachments of one message may add up to, in bytes.
    Q_PROPERTY(qint64 maxAttachmentBytes READ maxAttachmentBytes CONSTANT)
    // The folders of the open message's account (for the Move to Folder menu).
    Q_PROPERTY(QAbstractListModel *messageFolderModel READ messageFolderModel NOTIFY currentMessageChanged)
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
    // True while the list shows search results instead of the folder's listing.
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchActiveChanged)
    // False when the local store could not be opened; usage, cleanup and sync then do nothing.
    Q_PROPERTY(bool storageReady READ storageReady NOTIFY accountChanged)
    // The Google OAuth client is set up (read once at startup).
    Q_PROPERTY(bool googleConfigured READ googleConfigured CONSTANT)
    // Unread messages in all accounts' inboxes (as of the last background check, kept up to date
    // as messages are read here); 0 until the first check.
    Q_PROPERTY(int unreadCount READ unreadCount NOTIFY unreadCountChanged)
    // What the app is doing right now, for the status bar: "Connecting…", "Moving message…", an
    // account's sync progress, "Checking for new mail…" or "Loading…"; empty when idle.
    Q_PROPERTY(QString activityText READ activityText NOTIFY activityChanged)
    // The last thing that happened, for the status bar: "Message sent · 17:05". Event and time
    // only, never message content; empty until something happened (not stored).
    Q_PROPERTY(QString lastActionText READ lastActionText NOTIFY lastActionChanged)
    // When a background check last finished (any account); invalid before the first one.
    Q_PROPERTY(QDateTime lastCheckedAt READ lastCheckedAt NOTIFY activityChanged)
    // General options (see AppSettings): hide to the tray when the window is closed, and the
    // background check interval in minutes.
    Q_PROPERTY(bool keepInTray READ keepInTray NOTIFY appSettingsChanged)
    Q_PROPERTY(int checkMinutes READ checkMinutes NOTIFY appSettingsChanged)
    Q_PROPERTY(bool pushEnabled READ pushEnabled NOTIFY appSettingsChanged)
    // Seconds a sent message waits before the hand-off (0 = off); applies to messages sent afterwards.
    Q_PROPERTY(int undoSendSeconds READ undoSendSeconds WRITE setUndoSendSeconds NOTIFY appSettingsChanged)
    // Widths of the folder sidebar and the message list column (logical pixels), and their limits.
    Q_PROPERTY(int sidebarWidth READ sidebarWidth NOTIFY panelWidthsChanged)
    Q_PROPERTY(int listWidth READ listWidth NOTIFY panelWidthsChanged)
    // Whether the docked folder sidebar is hidden (remembered between runs).
    Q_PROPERTY(QStringList collapsedAccounts READ collapsedAccounts NOTIFY collapsedAccountsChanged)
    // Folders hidden from the sidebar (display only): account email -> list of folder paths, and
    // whether they are shown anyway. Inbox can never be hidden. Both are saved at once.
    Q_PROPERTY(QVariantMap hiddenFolders READ hiddenFolders NOTIFY hiddenFoldersChanged)
    Q_PROPERTY(bool showHiddenFolders READ showHiddenFolders WRITE setShowHiddenFolders NOTIFY showHiddenFoldersChanged)
    Q_PROPERTY(bool sidebarCollapsed READ sidebarCollapsed WRITE setSidebarCollapsed NOTIFY sidebarCollapsedChanged)
    Q_PROPERTY(int sidebarMinWidth READ sidebarMinWidth CONSTANT)
    Q_PROPERTY(int sidebarMaxWidth READ sidebarMaxWidth CONSTANT)
    Q_PROPERTY(int sidebarDefaultWidth READ sidebarDefaultWidth CONSTANT)
    Q_PROPERTY(int listMinWidth READ listMinWidth CONSTANT)
    Q_PROPERTY(int listMaxWidth READ listMaxWidth CONSTANT)
    Q_PROPERTY(int listDefaultWidth READ listDefaultWidth CONSTANT)
    // KDE's notification settings for the app can be opened (the app's notifyrc is installed).
    Q_PROPERTY(bool notificationsConfigurable READ notificationsConfigurable CONSTANT)

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
    // The path of the folder the open message is in.
    Q_PROPERTY(QString currentMessageFolder READ currentMessageFolder NOTIFY currentMessageChanged)
    [[nodiscard]] QString currentMessageFolder() const;
    [[nodiscard]] qint64 currentUid() const;
    [[nodiscard]] QString messageSubject() const;
    [[nodiscard]] QString messageFrom() const;
    [[nodiscard]] QString messageTo() const;
    [[nodiscard]] QDateTime messageDate() const;
    [[nodiscard]] QString messageHtml() const;
    [[nodiscard]] bool messageHasRemoteImages() const;
    [[nodiscard]] bool remoteImagesLoaded() const;
    Q_INVOKABLE void loadRemoteImages();
    // Colours for the HTML of the open message; QML's theme decides, so the body matches its pane.
    Q_INVOKABLE void setMessageColours(const QColor &text, const QColor &background, const QColor &link);
    [[nodiscard]] QStringList messageAttachments() const;
    [[nodiscard]] bool messageFlagged() const;
    [[nodiscard]] bool messageUnread() const;
    [[nodiscard]] bool canArchive() const;
    [[nodiscard]] bool canDelete() const;
    [[nodiscard]] bool canMove() const;
    [[nodiscard]] bool canCompose() const;
    [[nodiscard]] bool sending() const;
    [[nodiscard]] static qint64 maxAttachmentBytes()
    {
        return OutgoingMessage::MaxAttachmentBytes;
    }
    [[nodiscard]] QAbstractListModel *messageFolderModel() const;
    [[nodiscard]] bool storageEnabled() const;
    [[nodiscard]] QString importSince() const;
    [[nodiscard]] int keepValue() const;
    [[nodiscard]] int keepUnit() const;
    [[nodiscard]] int sizeValue() const;
    [[nodiscard]] int sizeUnit() const;
    [[nodiscard]] qint64 storageUsedBytes() const;
    [[nodiscard]] bool syncing() const;
    [[nodiscard]] bool searchActive() const;
    // Searches the open folder (every account's inbox in a shared view) for text in Subject, From
    // or Body, on the server. The results are only shown; the stored listing is left alone.
    Q_INVOKABLE void search(const QString &text);
    // Ends the search and shows the folder's normal listing again.
    Q_INVOKABLE void clearSearch();
    [[nodiscard]] bool storageReady() const;
    [[nodiscard]] bool googleConfigured() const;
    [[nodiscard]] int unreadCount() const;
    [[nodiscard]] QString activityText() const;
    [[nodiscard]] QString lastActionText() const;
    [[nodiscard]] QDateTime lastCheckedAt() const;
    [[nodiscard]] bool keepInTray() const;
    [[nodiscard]] int checkMinutes() const;
    [[nodiscard]] bool pushEnabled() const;
    [[nodiscard]] int undoSendSeconds() const;
    void setUndoSendSeconds(int seconds);
    [[nodiscard]] int sidebarWidth() const;
    [[nodiscard]] int listWidth() const;
    [[nodiscard]] bool sidebarCollapsed() const;
    void setSidebarCollapsed(bool collapsed);
    // Accounts whose folder tree is folded in the sidebar (saved at once).
    [[nodiscard]] QStringList collapsedAccounts() const;
    Q_INVOKABLE bool isAccountCollapsed(const QString &email) const;
    Q_INVOKABLE void setAccountCollapsed(const QString &email, bool collapsed);
    Q_INVOKABLE void setAllAccountsCollapsed(bool collapsed);
    [[nodiscard]] QVariantMap hiddenFolders() const;
    [[nodiscard]] bool showHiddenFolders() const;
    void setShowHiddenFolders(bool show);
    Q_INVOKABLE bool isFolderHidden(const QString &account, const QString &path) const;
    // Returns false (and changes nothing) for the inbox or an unknown account.
    Q_INVOKABLE bool setFolderHidden(const QString &account, const QString &path, bool hidden);
    [[nodiscard]] int sidebarMinWidth() const;
    [[nodiscard]] int sidebarMaxWidth() const;
    [[nodiscard]] int sidebarDefaultWidth() const;
    [[nodiscard]] int listMinWidth() const;
    [[nodiscard]] int listMaxWidth() const;
    [[nodiscard]] int listDefaultWidth() const;
    [[nodiscard]] bool notificationsConfigurable() const;

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
    // Moves a saved account one place up (delta -1) or down (+1) in the list and saves the order.
    // Unknown accounts, other deltas and moves past either end are ignored. No reconnect.
    Q_INVOKABLE void moveAccount(const QString &email, int delta);
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
    // Dev only: shows a raw RFC 822 message in the reading pane through the normal parse and render
    // path, without any account, server, store or flag involved.
    void showSampleMessage(const QByteArray &raw);
    // Attachment `index` (as in messageAttachments) of the open message, from the loaded message only.
    // openAttachment writes it to a private temp folder (removed when the app quits) and opens it with
    // the default application; a risky one (executable, script, ...) is not opened: attachmentBlocked.
    Q_INVOKABLE void openAttachment(int index);
    // Writes it to the local file `target` (mode 0600); false, with errorOccurred, on failure.
    Q_INVOKABLE bool saveAttachment(int index, const QUrl &target);
    // True if opening it could run code (by file suffix, declared or detected type).
    Q_INVOKABLE bool attachmentIsRisky(int index);
    // The safe file name of attachment `index` (as in messageAttachments, but without paths or control
    // characters); empty if there is no such attachment in the open message.
    Q_INVOKABLE QString attachmentFileName(int index) const;
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
    // Shows the inbox of `account` with message `uid` open (for a click on a notification). The
    // message opens once the inbox list has arrived; showMessageRequested() then asks the window
    // to show the message column.
    Q_INVOKABLE void openNewMail(const QString &account, qint64 uid);
    // Saves the options at once: the check timer restarts with the new interval (1–60) and
    // appSettingsChanged is emitted (the tray follows keepInTray).
    Q_INVOKABLE void saveAppSettings(bool keepInTray, int checkMinutes);
    // Push (IDLE) on or off; off stops every watcher, on starts them for the connected accounts.
    Q_INVOKABLE void setPushEnabled(bool enabled);
    // Remembers the widths the user dragged the panels to (clamped to the limits); call it when
    // a drag ends. Emits panelWidthsChanged when something changed.
    Q_INVOKABLE void savePanelWidths(int sidebar, int list);
    // Opens KDE's dialog for the app's notification events.
    Q_INVOKABLE void configureNotifications();
    // Connects a failed account again, the way startup does (reads its password or token).
    Q_INVOKABLE void reconnect(const QString &account);
    // What a new message starts with when answering the open message. kind: "reply", "replyAll"
    // or "forward". The map has: account (the sending account), from ("Name <address>"), to, cc
    // (comma separated), subject, body (the cursor goes above the quoted text), inReplyTo, references
    // (a list), kind. Empty when no message is open or loaded.
    Q_INVOKABLE QVariantMap composeFor(const QString &kind) const;
    // Sends a message through the SMTP server of message["account"]. Keys: account, to, cc, bcc,
    // subject, body (plain text), inReplyTo, references (a list). Returns the id of the message;
    // answers with messageSent(id) or sendFailed(id, text, original, queued). One message at a time.
    // With the undo-send delay on (undoSendSeconds > 0) the message is queued first: sendQueued(id),
    // then after the delay the hand-off runs; undoSend(id) cancels it. Delay 0 hands off at once.
    Q_INVOKABLE int sendMessage(const QVariantMap &message);
    // Whether message id is waiting in the undo-send delay (right after sendMessage: it was queued).
    Q_INVOKABLE bool isPendingSend(int id) const;
    // Cancels the queued message id (-1 = the newest) and emits sendUndone(original map). False when
    // there is no such pending message.
    Q_INVOKABLE bool undoSend(int id = -1);
    // The one way to quit the app (tray, Ctrl+Q, window close): sends pending messages at once, waits
    // in the normal event loop (at most 60 s) until every hand-off finished or failed, then quits.
    // Calls while waiting are ignored.
    Q_INVOKABLE void requestQuit();

    // Ends the delay of every pending message now (what quitting does), handing them to SMTP in turn.
    void flushPendingSends();
    [[nodiscard]] QVariantList pendingSends() const;
    [[nodiscard]] QVariantMap pendingSend() const;
    // {path, name, size} of a local file or file URL, for the attachment chips; empty when it is not a file.
    Q_INVOKABLE QVariantMap fileInfo(const QUrl &url) const;
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
    // How many results a search shows (the most the server returns per account).
    static constexpr int searchLimit = 200;

Q_SIGNALS:
    void lastActionChanged();
    void accountChanged();
    void busyChanged();
    void currentFolderChanged();
    void currentMessageChanged();
    void storageSettingsChanged();
    void storageUsageChanged();
    void syncingChanged();
    void searchActiveChanged();
    void unreadCountChanged();
    void appSettingsChanged();
    void activityChanged();
    void sendingChanged();
    void messageSent(int id);
    // queued: the message came through the undo-send queue, so its window is gone; original is the
    // map passed to sendMessage, to reopen it with the error.
    void sendFailed(int id, const QString &message, const QVariantMap &original, bool queued);
    void pendingSendChanged();
    void sendQueued(int id);
    void sendUndone(const QVariantMap &message);
    void panelWidthsChanged();
    void sidebarCollapsedChanged();
    void collapsedAccountsChanged();
    void hiddenFoldersChanged();
    void showHiddenFoldersChanged();
    void showMessageRequested();
    // New mail found by a background check. With count == 1, sender ("Name <address>") and
    // subject describe the message; uid is the newest new message of the account's inbox.
    void newMailArrived(const QString &account, int count, const QString &sender, const QString &subject, qint64 uid);
    void errorOccurred(const QString &message);
    // An attachment was not opened because it could run code; name is its file name.
    void attachmentBlocked(const QString &name);

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
    void dropMoves(AccountSession *session, bool failed);
    void onMoveFailed(AccountSession *session, const QString &from, qint64 uid, const QString &message);
    void onMessageMoved(AccountSession *session, const QString &from, qint64 uid, const QString &to);
    // Appends the sent message to the account's Sent folder (not for Gmail, which does it itself).
    void saveSentCopy(const QString &account, const QString &provider, const QByteArray &data);
    // Background check of one account's inbox (unread count, new mail); every checkMinutes and
    // right after an account's folders arrive.
    void checkInbox(AccountSession *session);
    void retryQueuedCheck(AccountSession *session);
    void checkForNewMail();
    // Push mail (IDLE): the watcher's change signal starts a check after a short pause.
    void startPush(AccountSession *session);
    void onPushChanged(AccountSession *session);
    void forgetChecks(AccountSession *session); // drops the timing state of a session that goes away
    void onInboxChecked(AccountSession *session, const QString &folder, int unreadTotal, const QList<MessageSummary> &newestUnread);
    // Keeps unreadCount in step with what happens here: delta for a message of `folder`.
    // Loads the inbox again if the open view shows it, so new mail appears in the list.
    void refreshInboxView(AccountSession *session, const QString &inbox);
    void adjustUnread(const AccountSession *session, const QString &folder, int delta);
    void moveOpenMessage(const QString &target, const QString &doneText, const QString &failedText);
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
    // Asks the server for the headers of the open folder; remembered so the answer can be told from a search's.
    void requestHeaders(AccountSession *session, const QString &folder, ImapClient::HeaderFilter filter);
    void requestSearch(AccountSession *session, const QString &folder);
    void runSearch(); // sends the search of the open single folder
    void endSearch(); // leaves search mode without touching the list
    void mergeSharedParts();
    // The IMAP folder and filter behind a folder or shared-view path. A shared view reads each
    // account's own inbox.
    [[nodiscard]] static QString imapFolderFor(const AccountSession *session, const QString &path);
    [[nodiscard]] static ImapClient::HeaderFilter filterFor(const QString &path);
    [[nodiscard]] static bool isSharedView(const QString &path);
    void onHeadersFetched(AccountSession *session, const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages);
    void onMessageFetched(AccountSession *session, const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    void showMessage(AccountSession *session, const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    // The stored messages for a folder or shared-view path, filtered like the server's list.
    [[nodiscard]] QList<MessageSummary> storedMessages(const AccountSession *session, const QString &path) const;
    [[nodiscard]] bool storageActive() const; // storage on and the open account's store open
    // Empties the message properties; with clearUid also forgets which message is open.
    void clearMessage(bool clearUid);
    // With storage on: shows the stored folder list and All Inboxes before connecting.
    void showStoredMail();
    // Dev hook PLASMAMAIL_DEMO: fictional accounts, folders and messages, in memory only.
    void loadDemo();
    void updateStorageUsage();
    void updateSyncing();
    // Recomputes activityText(); emits activityChanged() when it or lastCheckedAt changed.
    void updateActivity();
    // Remembers what happened last, with the time; emits lastActionChanged() when the text changed.
    void setLastAction(const QString &text);

    QString m_dataDir; // AppDataLocation
    QList<AccountSession *> m_sessions; // saved accounts, in order
    AccountSession *m_pending = nullptr; // being verified by setupAccount()
    QString m_pendingPassword;
    QString m_pendingOpenAccount; // see openNewMail()
    qint64 m_pendingOpenUid = 0;
    bool m_googleConfigured = false;
    // The open shared view: each account's part of the merged list, and the accounts already asked.
    std::map<QString, QList<MessageSummary>> m_sharedParts;
    std::set<AccountSession *> m_sharedRequested;
    SmtpClient *m_smtp = nullptr; // the running send, if any
    bool m_demoBusy = false; // demo only: the simulated hand-off is running
    bool m_quitting = false; // requestQuit() is waiting for the hand-offs
    // Messages in the undo-send delay, and those whose delay ended while another send was running.
    struct PendingSend {
        int id = 0;
        QVariantMap message;
        QDateTime deadline;
        QTimer *timer = nullptr;
    };
    // A hand-off is running: the SMTP send, or the demo's simulated one.
    [[nodiscard]] bool handingOff() const;
    [[nodiscard]] bool quitReady() const;
    void continueQuit();
    void handOff(int id, const QVariantMap &message);
    void startNextHandOff();
    void finishPendingSend(int id);
    void failSend(int id, const QVariantMap &message, const QString &text);
    void finishSend(int id);
    [[nodiscard]] QString sendProblem(const QVariantMap &message) const;
    QList<PendingSend> m_pendingSends;
    QList<std::pair<int, QVariantMap>> m_handOffQueue;
    QTimer *m_pendingTick = nullptr;
    QSet<int> m_queuedIds; // sent through the queue: no window waits for the answer
    int m_nextSendId = 1;
    AppSettings m_app;
    // Dev only: PLASMAMAIL_SIDEBAR_COLLAPSED=0|1 shows that state for this run without saving it.
    std::optional<bool> m_collapsedOverride;
    QString m_activityText;
    QString m_lastAction;
    std::map<QString, QPair<QString, QString>> m_moveTexts; // moveKey() -> status texts for success, failure
    QDateTime m_lastCheckedAt;
    NewMailTracker m_newMail;
    QTimer *m_checkTimer;
    std::map<QString, int> m_unreadByAccount; // unread messages per account's inbox
    std::set<AccountSession *> m_checking; // accounts with a background check in flight
    std::set<AccountSession *> m_checkQueued; // a push change arrived during a check: one more after it
    std::map<AccountSession *, QTimer *> m_pushDebounce; // single-shot, children of their session
    std::map<AccountSession *, std::chrono::steady_clock::time_point> m_lastCheck;
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
    QString renderBody(const KMime::Message &msg, bool allowRemoteImages) const;
    QString m_messageHtml;
    bool m_messageHasRemoteImages = false; // the open message's HTML refers to remote images
    bool m_remoteImagesLoaded = false;
    QColor m_bodyText;
    QColor m_bodyBackground;
    QColor m_bodyLink;
    QStringList m_messageAttachments;
    bool m_messageFlagged = false;
    bool m_messageUnread = false;
    std::shared_ptr<KMime::Message> m_openMessage; // the loaded open message, for composeFor()
    std::unique_ptr<QTemporaryDir> m_attachmentDir; // created on first open, removed with the controller
    int m_attachmentCount = 0;
    mutable std::optional<QList<AttachmentData>> m_attachmentCache; // decoded parts of m_openMessage, built on first use
    const QList<AttachmentData> &attachmentList() const;
    StorageSettings m_storage;
    qint64 m_storageUsedBytes = 0;
    bool m_syncing = false;
    bool m_demo = false; // PLASMAMAIL_DEMO: no network, keychain or store
    std::map<QString, std::map<QString, QList<MessageSummary>>> m_demoMessages; // account -> folder -> messages
    bool m_showingStored = false; // the view came from the store; refresh it once connected
    int m_requests = 0;
    // Search results are only shown, never stored. Replies of each session come in the order asked,
    // so this tells a search's answer from a folder listing's, and an old search's from the current one's.
    struct Inflight {
        bool search;
        int generation;
    };
    std::map<AccountSession *, std::deque<Inflight>> m_inflight;
    bool m_searchActive = false;
    QString m_searchText;
    int m_searchGeneration = 0;
};
