// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailcontroller.h"

#include "accountlistmodel.h"
#include "accountsession.h"
#include "appsettings.h"
#include "foldermodel.h"
#include "folderutils.h"
#include "imapclient.h"
#include "messagelistmodel.h"
#include "messagerenderer.h"
#include "messagestore.h"
#include "oauthsettings.h"
#include "secretstore.h"

#include <KLocalizedString>
#include <KMime/Message>
#include <KNotifyConfigWidget>

#include <QDir>
#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

MailController::MailController(QObject *parent)
    : QObject(parent)
    , m_dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_checkTimer(new QTimer(this))
    , m_accounts(new AccountListModel(this))
    , m_noFolders(new FolderModel(this))
    , m_secrets(new SecretStore(QString::fromLatin1(AccountSecrets::service), this))
    , m_messageModel(new MessageListModel(this))
{
    connect(m_secrets, &SecretStore::passwordRead, this, &MailController::onSecretRead);
    m_googleConfigured = GoogleOAuthSettings::load().isConfigured();
    connect(m_checkTimer, &QTimer::timeout, this, &MailController::checkForNewMail);
    m_app = AppSettings::load();
    m_checkTimer->start(std::chrono::minutes(m_app.checkMinutes));

    m_storage = StorageSettings::load();
    const QList<Account> accounts = Account::loadAll();
    // The single store of the first version belongs to the first account.
    if (!accounts.isEmpty() && !MessageStore::moveLegacyStore(m_dataDir, MessageStore::accountDir(m_dataDir, accounts.first().email))) {
        qWarning("Cannot move the old local store into the folder of %s", qPrintable(accounts.first().email));
    }
    for (const Account &account : accounts) {
        m_sessions.append(createSession(account));
    }
    m_accounts->setSessions(m_sessions);
    updateStorageUsage();
    updateSyncing();
    showStoredMail();
    connectSavedAccounts();
}

// Sessions are children of the controller; their sync uses their store, which they handle.
MailController::~MailController() = default;

bool MailController::hasAccount() const
{
    return !m_sessions.isEmpty();
}

QString MailController::accountEmail() const
{
    return m_sessions.isEmpty() ? QString() : m_sessions.first()->account().email;
}

QString MailController::accountDisplayName() const
{
    return m_sessions.isEmpty() ? QString() : m_sessions.first()->account().displayName;
}

QString MailController::accountProvider() const
{
    return m_sessions.isEmpty() ? QString() : AccountListModel::providerName(m_sessions.first()->account().provider);
}

QString MailController::accountServer() const
{
    if (m_sessions.isEmpty()) {
        return {};
    }
    const Account &account = m_sessions.first()->account();
    return u"%1:%2"_s.arg(account.imapHost).arg(account.imapPort);
}

AccountSession *MailController::uiSession() const
{
    return !m_sessions.isEmpty() ? m_sessions.first() : m_pending;
}

AccountSession *MailController::sessionFor(const QString &account) const
{
    const auto it = std::find_if(m_sessions.cbegin(), m_sessions.cend(), [&account](const AccountSession *session) {
        return session->account().email.compare(account, Qt::CaseInsensitive) == 0;
    });
    return it == m_sessions.cend() ? nullptr : *it;
}

// The account whose folder is open; the first one while a shared view is open (T89 merges them).
AccountSession *MailController::currentSession() const
{
    AccountSession *session = sessionFor(m_currentAccount);
    return session ? session : uiSession();
}

QString MailController::currentEmail() const
{
    const AccountSession *session = currentSession();
    return session ? session->account().email : QString();
}

QString MailController::currentAccount() const
{
    return m_currentAccount;
}

AccountSession *MailController::createSession(const Account &account)
{
    auto session = new AccountSession(account, MessageStore::accountDir(m_dataDir, account.email), m_storage, this);
    connect(session, &AccountSession::foldersListed, this, [this, session] {
        onSessionFolders(session);
    });
    connect(session,
            &AccountSession::headersFetched,
            this,
            [this, session](const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages) {
                onHeadersFetched(session, folder, filter, messages);
            });
    connect(session, &AccountSession::messageFetched, this, [this, session](const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message) {
        onMessageFetched(session, folder, uid, message);
    });
    connect(session, &AccountSession::errorOccurred, this, [this, session](const QString &message) {
        onSessionError(session, message);
    });
    connect(session, &AccountSession::messageMoved, this, [this, session](const QString &from, qint64 uid, const QString &to) {
        onMessageMoved(session, from, uid, to);
    });
    connect(session, &AccountSession::inboxChecked, this, [this, session](const QString &folder, int unreadTotal, const QList<MessageSummary> &newestUnread) {
        onInboxChecked(session, folder, unreadTotal, newestUnread);
    });
    connect(session, &AccountSession::syncingChanged, this, &MailController::updateSyncing);
    connect(session, &AccountSession::storageUsageChanged, this, &MailController::updateStorageUsage);
    return session;
}

bool MailController::isKnown(const AccountSession *session) const
{
    return session == m_pending || m_sessions.contains(const_cast<AccountSession *>(session));
}

void MailController::discardSession(AccountSession *session, bool deleteStore)
{
    m_sharedRequested.erase(session);
    if (m_awaitingFolders.erase(session) > 0) {
        endRequest();
    }
    session->client()->disconnectFromServer();
    if (deleteStore) {
        session->clearStore();
        // The folder goes once the session has closed its database.
        const QString dir = MessageStore::accountDir(m_dataDir, session->account().email);
        connect(session, &QObject::destroyed, this, [dir] {
            QDir(dir).removeRecursively();
        });
    }
    session->deleteLater();
}

bool MailController::busy() const
{
    return m_requests > 0;
}

QAbstractListModel *MailController::accounts() const
{
    return m_accounts;
}

QAbstractListModel *MailController::folderModel() const
{
    const AccountSession *session = uiSession();
    return session ? session->folderModel() : m_noFolders;
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
    const AccountSession *session = currentSession();
    return m_currentFolder.isEmpty() || !session ? QString() : session->folderModel()->displayName(m_currentFolder);
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

QString MailController::imapFolderFor(const AccountSession *session, const QString &path)
{
    return isSharedView(path) ? (session ? session->folderModel()->inboxPath() : QString()) : path;
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

QString MailController::currentMessageFolder() const
{
    return m_messageFolder;
}

QAbstractListModel *MailController::messageFolderModel() const
{
    const AccountSession *session = sessionFor(m_messageAccount);
    return session ? static_cast<QAbstractListModel *>(session->folderModel()) : m_noFolders;
}

QString MailController::currentMessageAccount() const
{
    return m_messageAccount;
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

bool MailController::messageUnread() const
{
    return m_messageUnread;
}

bool MailController::canMove() const
{
    const AccountSession *session = sessionFor(m_messageAccount);
    return session && m_currentUid > 0 && !session->folders().isEmpty() && !m_moving.contains(moveKey(m_messageAccount, m_messageFolder, m_currentUid));
}

bool MailController::canArchive() const
{
    const AccountSession *session = sessionFor(m_messageAccount);
    if (!session || m_currentUid <= 0 || m_moving.contains(moveKey(m_messageAccount, m_messageFolder, m_currentUid))) {
        return false;
    }
    const QString target = archiveTarget(session->folders());
    return !target.isEmpty() && target != m_messageFolder;
}

bool MailController::canDelete() const
{
    const AccountSession *session = sessionFor(m_messageAccount);
    if (!session || m_currentUid <= 0 || m_moving.contains(moveKey(m_messageAccount, m_messageFolder, m_currentUid))) {
        return false;
    }
    // Deleting from Trash would be for good, which this app doesn't do.
    const QString trash = folderForRole(session->folders(), u"trash"_s);
    return !trash.isEmpty() && trash != m_messageFolder;
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
    // Each session applies the limits to its own copies and starts its sync.
    for (AccountSession *session : std::as_const(m_sessions)) {
        session->setStorageSettings(settings);
    }
    updateSyncing();
    updateStorageUsage();
}

qint64 MailController::storageUsedBytes() const
{
    return m_storageUsedBytes;
}

bool MailController::syncing() const
{
    return m_syncing;
}

bool MailController::googleConfigured() const
{
    return m_googleConfigured;
}

bool MailController::keepInTray() const
{
    return m_app.keepInTray;
}

int MailController::checkMinutes() const
{
    return m_app.checkMinutes;
}

int MailController::sidebarWidth() const
{
    return m_app.sidebarWidth;
}

int MailController::listWidth() const
{
    return m_app.listWidth;
}

int MailController::sidebarMinWidth() const
{
    return AppSettings::minSidebarWidth;
}

int MailController::sidebarMaxWidth() const
{
    return AppSettings::maxSidebarWidth;
}

int MailController::sidebarDefaultWidth() const
{
    return AppSettings::defaultSidebarWidth;
}

int MailController::listMinWidth() const
{
    return AppSettings::minListWidth;
}

int MailController::listMaxWidth() const
{
    return AppSettings::maxListWidth;
}

int MailController::listDefaultWidth() const
{
    return AppSettings::defaultListWidth;
}

void MailController::savePanelWidths(int sidebar, int list)
{
    AppSettings settings = m_app;
    settings.sidebarWidth = std::clamp(sidebar, AppSettings::minSidebarWidth, AppSettings::maxSidebarWidth);
    settings.listWidth = std::clamp(list, AppSettings::minListWidth, AppSettings::maxListWidth);
    if (settings == m_app) {
        return;
    }
    settings.save();
    m_app = settings;
    Q_EMIT panelWidthsChanged();
}

bool MailController::notificationsConfigurable() const
{
    return !QStandardPaths::locate(QStandardPaths::GenericDataLocation, u"knotifications6/plasma-mail.notifyrc"_s).isEmpty();
}

void MailController::saveAppSettings(bool keepInTray, int checkMinutes)
{
    AppSettings settings = m_app; // keeps the panel widths
    settings.keepInTray = keepInTray;
    settings.checkMinutes = std::clamp(checkMinutes, AppSettings::minCheckMinutes, AppSettings::maxCheckMinutes);
    if (settings == m_app) {
        return;
    }
    settings.save();
    m_app = settings;
    m_checkTimer->start(std::chrono::minutes(m_app.checkMinutes));
    Q_EMIT appSettingsChanged();
}

void MailController::configureNotifications()
{
    KNotifyConfigWidget::configure(nullptr, u"plasma-mail"_s);
}

int MailController::unreadCount() const
{
    int total = 0;
    for (const auto &[account, count] : m_unreadByAccount) {
        total += count;
    }
    return total;
}

bool MailController::storageReady() const
{
    return std::all_of(m_sessions.cbegin(), m_sessions.cend(), [](const AccountSession *session) {
        return session->storageReady();
    });
}

void MailController::cleanUpNow()
{
    for (AccountSession *session : std::as_const(m_sessions)) {
        session->applyStorageLimits();
    }
}

void MailController::showStoredMail()
{
    bool any = false;
    for (AccountSession *session : std::as_const(m_sessions)) {
        any = session->showStoredFolders() || any;
    }
    if (!any) {
        return;
    }
    m_currentFolder = QLatin1StringView(allInboxesPath);
    for (const AccountSession *session : std::as_const(m_sessions)) {
        m_sharedParts.insert_or_assign(session->account().email, storedMessages(session, m_currentFolder));
    }
    mergeSharedParts();
    m_showingStored = true;
    Q_EMIT currentFolderChanged();
}

void MailController::updateStorageUsage()
{
    qint64 used = 0;
    for (const AccountSession *session : std::as_const(m_sessions)) {
        used += session->storageUsedBytes();
    }
    if (used != m_storageUsedBytes) {
        m_storageUsedBytes = used;
        Q_EMIT storageUsageChanged();
    }
}

void MailController::updateSyncing()
{
    const bool syncing = std::any_of(m_sessions.cbegin(), m_sessions.cend(), [](const AccountSession *session) {
        return session->syncing();
    });
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

void MailController::selectFolder(const QString &account, const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    AccountSession *session = isSharedView(path) ? uiSession() : sessionFor(account);
    if (!session) {
        return;
    }
    const QString newAccount = isSharedView(path) ? QString() : session->account().email;
    if (path != m_currentFolder || newAccount != m_currentAccount) {
        m_currentAccount = newAccount;
        m_currentFolder = path;
        m_sharedParts.clear();
        m_messageModel->clear();
        clearMessage(true); // the open message belongs to the folder being left
        Q_EMIT currentFolderChanged();
    }
    if (isSharedView(path)) {
        // Every account's inbox, merged; the accounts whose folders are not known yet join
        // when their folder list arrives (onSessionFolders).
        m_sharedRequested.clear();
        for (AccountSession *each : std::as_const(m_sessions)) {
            fetchSharedPart(each);
        }
        return;
    }
    if (session->storageActive() && m_messageModel->rowCount() == 0) {
        m_messageModel->setMessages(storedMessages(session, path));
    }
    beginRequest();
    session->client()->fetchHeaders(path, messageLimit, filterFor(path));
}

// Asks one account for its part of the open shared view, once per selection.
void MailController::fetchSharedPart(AccountSession *session)
{
    // Folders arrive from the server only once the account is connected.
    if (session->folders().isEmpty() || !m_sharedRequested.insert(session).second) {
        return;
    }
    const QString folder = imapFolderFor(session, m_currentFolder);
    if (folder.isEmpty()) {
        return;
    }
    const QString email = session->account().email;
    if (session->storageActive() && !m_sharedParts.contains(email)) {
        m_sharedParts.insert_or_assign(email, storedMessages(session, m_currentFolder));
        mergeSharedParts();
    }
    beginRequest();
    session->client()->fetchHeaders(folder, messageLimit, filterFor(m_currentFolder));
}

// Shows the newest messageLimit messages of all accounts' parts, newest first.
void MailController::mergeSharedParts()
{
    QList<MessageSummary> merged;
    for (const AccountSession *session : std::as_const(m_sessions)) {
        const auto it = m_sharedParts.find(session->account().email);
        if (it != m_sharedParts.end()) {
            merged.append(it->second);
        }
    }
    // Undated messages (e.g. Yahoo mail without a Date header) come last.
    std::stable_sort(merged.begin(), merged.end(), [](const MessageSummary &a, const MessageSummary &b) {
        if (a.date.isValid() != b.date.isValid()) {
            return a.date.isValid();
        }
        return a.date > b.date;
    });
    if (merged.size() > messageLimit) {
        merged.resize(messageLimit);
    }
    m_messageModel->setMessages(merged);
}

bool MailController::storageActive() const
{
    const AccountSession *session = currentSession();
    return session && session->storageActive();
}

QList<MessageSummary> MailController::storedMessages(const AccountSession *session, const QString &path) const
{
    if (!session || !session->storageActive()) {
        return {};
    }
    const QString folder = imapFolderFor(session, path);
    const ImapClient::HeaderFilter filter = filterFor(path);
    QList<MessageSummary> result;
    for (MessageSummary message : session->store()->messages(folder, filter == ImapClient::AllMessages ? messageLimit : -1)) {
        if (filter == ImapClient::AllMessages || (filter == ImapClient::Unread ? message.unread : message.flagged)) {
            message.account = session->account().email; // the store of one account doesn't know whose it is
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
    if (isSharedView(m_currentFolder)) {
        // Every account lists its folders again; each answer then asks for that account's part.
        m_sharedRequested.clear();
        for (AccountSession *session : std::as_const(m_sessions)) {
            if (!session->folders().isEmpty() && m_awaitingFolders.insert(session).second) {
                beginRequest();
                session->client()->listFolders();
            }
        }
        return;
    }
    AccountSession *session = currentSession();
    if (!session) {
        return;
    }
    if (m_awaitingFolders.insert(session).second) {
        beginRequest();
    }
    session->client()->listFolders();
    if (!m_currentFolder.isEmpty()) {
        selectFolder(m_currentAccount, m_currentFolder);
    }
}

void MailController::onHeadersFetched(AccountSession *session, const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages)
{
    endRequest();
    const bool shared = isSharedView(m_currentFolder);
    if (!isKnown(session) || (!shared && session != currentSession()) || folder != imapFolderFor(session, m_currentFolder)
        || filter != filterFor(m_currentFolder)) {
        return; // the user switched folders while this was loading
    }
    QList<MessageSummary> shown = messages;
    if (session->storageActive()) {
        // The server sends no body text; previews come from the local copies.
        const QHash<qint64, QString> previews = session->store()->previews(folder);
        for (MessageSummary &message : shown) {
            message.preview = previews.value(message.uid);
        }
    }
    if (!shared) {
        m_messageModel->setMessages(shown);
        // A click on a notification asked for a message of this list.
        if (!m_pendingOpenAccount.isEmpty() && session->account().email == m_pendingOpenAccount) {
            const qint64 uid = std::exchange(m_pendingOpenUid, 0);
            const QString account = std::exchange(m_pendingOpenAccount, QString());
            if (m_messageModel->message(account, uid).uid == uid) {
                openMessage(account, uid);
                Q_EMIT showMessageRequested();
            }
        }
        return;
    }
    // One account's answer replaces only that account's part of the merged list.
    m_sharedParts.insert_or_assign(session->account().email, shown);
    mergeSharedParts();
}

void MailController::openMessage(const QString &account, qint64 uid)
{
    AccountSession *session = sessionFor(account);
    if (uid <= 0 || m_currentFolder.isEmpty() || !session) {
        return;
    }
    m_currentUid = uid;
    m_messageAccount = session->account().email;
    m_messageFolder = imapFolderFor(session, m_currentFolder);
    clearMessage(false);
    const MessageSummary listed = m_messageModel->message(m_messageAccount, uid); // the parsed message carries no IMAP flags
    m_messageFlagged = listed.flagged;
    m_messageUnread = listed.unread;
    Q_EMIT currentMessageChanged(); // the flag shows while the message is still loading
    if (session->storageActive()) {
        if (const QByteArray raw = session->store()->rawMessage(m_messageFolder, uid); !raw.isEmpty()) {
            // Parsed like KIMAP does with fetched messages.
            KMime::Message message;
            message.setContent(KMime::CRLFtoLF(raw));
            message.parse();
            showMessage(session, m_messageFolder, uid, message);
            return;
        }
    }
    beginRequest();
    session->client()->fetchMessage(m_messageFolder, uid);
}

void MailController::setFlagged(const QString &account, qint64 uid, bool flagged)
{
    AccountSession *session = sessionFor(account);
    if (uid <= 0 || !session) {
        return;
    }
    const bool isOpenMessage = uid == m_currentUid && session->account().email == m_messageAccount;
    // The open message may belong to a folder the list no longer shows.
    const QString folder = isOpenMessage ? m_messageFolder : imapFolderFor(session, m_currentFolder);
    if (folder.isEmpty()) {
        return;
    }
    m_messageModel->setFlagged(session->account().email, uid, flagged);
    if (isOpenMessage && m_messageFlagged != flagged) {
        m_messageFlagged = flagged;
        Q_EMIT currentMessageChanged();
    }
    session->client()->setFlagged(folder, uid, flagged);
}

void MailController::setUnread(const QString &account, qint64 uid, bool unread)
{
    AccountSession *session = sessionFor(account);
    if (uid <= 0 || !session) {
        return;
    }
    const bool isOpenMessage = uid == m_currentUid && session->account().email == m_messageAccount;
    // The open message may belong to a folder the list no longer shows.
    const QString folder = isOpenMessage ? m_messageFolder : imapFolderFor(session, m_currentFolder);
    if (folder.isEmpty()) {
        return;
    }
    if (m_messageModel->message(session->account().email, uid).unread != unread) {
        adjustUnread(session, folder, unread ? 1 : -1);
    }
    m_messageModel->setUnread(session->account().email, uid, unread);
    if (session->storageActive() && session->store()->contains(folder, uid)) {
        session->store()->setUnread(folder, uid, unread);
    }
    if (isOpenMessage && m_messageUnread != unread) {
        m_messageUnread = unread;
        Q_EMIT currentMessageChanged();
    }
    session->client()->setSeen(folder, uid, !unread);
}

void MailController::checkForNewMail()
{
    for (AccountSession *session : std::as_const(m_sessions)) {
        checkInbox(session);
    }
}

void MailController::checkInbox(AccountSession *session)
{
    // Folders arrive from the server only once the account is connected; one check at a time.
    if (session->folders().isEmpty() || m_checking.contains(session)) {
        return;
    }
    const QString inbox = folderForRole(session->folders(), u"inbox"_s);
    if (inbox.isEmpty()) {
        return;
    }
    m_checking.insert(session);
    session->client()->checkInbox(inbox, 20);
}

void MailController::onInboxChecked(AccountSession *session, const QString &folder, int unreadTotal, const QList<MessageSummary> &newestUnread)
{
    m_checking.erase(session);
    if (!isKnown(session)) {
        return;
    }
    const QString email = session->account().email;
    const int before = unreadCount();
    // Not the first check of this account: something changed in the inbox since the last one.
    const auto known = m_unreadByAccount.find(email);
    const bool unreadChanged = known != m_unreadByAccount.end() && known->second != unreadTotal;
    m_unreadByAccount[email] = unreadTotal;
    if (unreadCount() != before) {
        Q_EMIT unreadCountChanged();
    }
    // UIDVALIDITY is not known here (0); a rebuilt inbox is picked up once its uids pass the old ones.
    const QList<MessageSummary> fresh = m_newMail.newMessages(email, folder, 0, newestUnread, QDateTime::currentDateTimeUtc());
    // Counts only, never message content.
    qInfo("Inbox check for %s: %d unread, %d new", qPrintable(email), unreadTotal, int(fresh.size()));
    // New mail should show up in the list that is open, not only in a notification.
    if (unreadChanged || !fresh.isEmpty()) {
        refreshInboxView(session, folder);
    }
    if (fresh.isEmpty()) {
        return;
    }
    const MessageSummary &newest = fresh.constFirst();
    Q_EMIT newMailArrived(email, int(fresh.size()), fresh.size() == 1 ? newest.from : QString(), fresh.size() == 1 ? newest.subject : QString(), newest.uid);
}

// Loads the inbox again if the open view shows it (the inbox itself, or a shared view).
void MailController::refreshInboxView(AccountSession *session, const QString &inbox)
{
    if (isSharedView(m_currentFolder)) {
        m_sharedRequested.erase(session);
        fetchSharedPart(session);
    } else if (m_currentFolder == inbox && m_currentAccount == session->account().email) {
        selectFolder(m_currentAccount, m_currentFolder);
    }
}

void MailController::adjustUnread(const AccountSession *session, const QString &folder, int delta)
{
    if (folder != folderForRole(session->folders(), u"inbox"_s)) {
        return;
    }
    int &count = m_unreadByAccount[session->account().email];
    if (count + delta >= 0) {
        count += delta;
        Q_EMIT unreadCountChanged();
    }
}

QString MailController::moveKey(const QString &account, const QString &folder, qint64 uid)
{
    return u"%1\n%2\n%3"_s.arg(account, folder).arg(uid);
}

void MailController::openNewMail(const QString &account, qint64 uid)
{
    const AccountSession *session = sessionFor(account);
    if (!session || uid <= 0) {
        return;
    }
    const QString inbox = folderForRole(session->folders(), u"inbox"_s);
    if (inbox.isEmpty()) {
        return;
    }
    m_pendingOpenAccount = session->account().email;
    m_pendingOpenUid = uid;
    selectFolder(m_pendingOpenAccount, inbox); // the message opens when the list is in
}

void MailController::archiveMessage()
{
    if (const AccountSession *session = sessionFor(m_messageAccount); session && canArchive()) {
        moveOpenMessage(archiveTarget(session->folders()));
    }
}

void MailController::deleteMessage()
{
    if (const AccountSession *session = sessionFor(m_messageAccount); session && canDelete()) {
        moveOpenMessage(folderForRole(session->folders(), u"trash"_s));
    }
}

void MailController::moveMessage(const QString &folderPath)
{
    const AccountSession *session = sessionFor(m_messageAccount);
    if (!session) {
        return;
    }
    const QList<FolderInfo> folders = session->folders();
    if (std::none_of(folders.cbegin(), folders.cend(), [&folderPath](const FolderInfo &folder) {
            return folder.path == folderPath;
        })) {
        return; // not a folder of this account
    }
    moveOpenMessage(folderPath);
}

void MailController::moveOpenMessage(const QString &target)
{
    AccountSession *session = sessionFor(m_messageAccount);
    if (!session || m_currentUid <= 0 || target.isEmpty() || target == m_messageFolder) {
        return;
    }
    if (!m_moving.insert(moveKey(m_messageAccount, m_messageFolder, m_currentUid)).second) {
        return; // already on its way
    }
    beginRequest();
    session->client()->moveMessage(m_messageFolder, m_currentUid, target);
    Q_EMIT currentMessageChanged(); // canArchive / canDelete are false while it moves
}

void MailController::onMessageMoved(AccountSession *session, const QString &from, qint64 uid, const QString &to)
{
    Q_UNUSED(to)
    const QString email = session->account().email;
    if (m_moving.erase(moveKey(email, from, uid)) == 0 || !isKnown(session)) {
        return;
    }
    endRequest();
    // The server has it now; the copy in the target folder arrives with the next sync.
    if (session->storageActive() && !session->store()->removeMessage(from, uid)) {
        qWarning("Cannot remove the local copy of a moved message: %s", qPrintable(session->store()->lastError()));
    }
    if (const auto part = m_sharedParts.find(email); part != m_sharedParts.end()) {
        part->second.removeIf([uid](const MessageSummary &message) {
            return message.uid == uid;
        });
    }
    if (m_messageModel->message(email, uid).unread) {
        adjustUnread(session, from, -1); // an unread message leaving the inbox
    }
    const bool wasOpen = uid == m_currentUid && email == m_messageAccount && from == m_messageFolder;
    const int row = m_messageModel->removeMessage(email, uid);
    if (!wasOpen) {
        return;
    }
    // The next message in the list takes its place, else the one before it.
    MessageSummary next = row >= 0 ? m_messageModel->messageAt(row) : MessageSummary();
    if (next.uid == 0 && row > 0) {
        next = m_messageModel->messageAt(row - 1);
    }
    if (next.uid > 0) {
        openMessage(next.account, next.uid);
    } else {
        clearMessage(true);
    }
}

void MailController::onMessageFetched(AccountSession *session, const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message)
{
    endRequest();
    if (!isKnown(session) || uid != m_currentUid || folder != m_messageFolder || session->account().email != m_messageAccount || !message) {
        return; // another message was opened while this one was loading
    }
    showMessage(session, folder, uid, *message);
}

void MailController::showMessage(AccountSession *session, const QString &folder, qint64 uid, const KMime::Message &msg)
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

    const QString email = session->account().email;
    if (folder == imapFolderFor(session, m_currentFolder) && m_messageModel->message(email, uid).unread) {
        session->client()->markSeen(folder, uid);
        adjustUnread(session, folder, -1);
        m_messageModel->setUnread(email, uid, false);
        if (m_messageUnread) {
            m_messageUnread = false;
            Q_EMIT currentMessageChanged();
        }
        if (session->storageActive() && session->store()->contains(folder, uid)) {
            session->store()->setUnread(folder, uid, false);
        }
    }
}

void MailController::clearMessage(bool clearUid)
{
    if (clearUid) {
        m_currentUid = 0;
        m_messageAccount.clear();
        m_messageFolder.clear();
    }
    m_messageSubject.clear();
    m_messageFrom.clear();
    m_messageTo.clear();
    m_messageDate = {};
    m_messageHtml.clear();
    m_messageAttachments.clear();
    m_messageFlagged = false;
    m_messageUnread = false;
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

    if (sessionFor(account.email)) {
        Q_EMIT errorOccurred(i18n("%1 is already added.", account.email));
        return;
    }
    if (m_pending) {
        discardSession(std::exchange(m_pending, nullptr), false);
    }
    m_pendingPassword = password;
    m_pending = createSession(account);
    m_awaitingFolders.insert(m_pending);
    beginRequest();
    m_pending->connectWithSecret(password);
}

void MailController::setupGoogleAccount(const QString &email, const QString &displayName)
{
    Account account;
    Account::applyPreset(account, u"gmail"_s);
    account.authMethod = u"oauth"_s;
    account.email = email.trimmed();
    account.displayName = displayName.trimmed();
    if (!account.isValid()) {
        Q_EMIT errorOccurred(i18n("Please enter your Gmail address."));
        return;
    }
    if (!m_googleConfigured) {
        Q_EMIT errorOccurred(i18n("No Google OAuth client is set up. Add ClientId and ClientSecret to the [GoogleOAuth] group of plasmamailrc."));
        return;
    }
    if (sessionFor(account.email)) {
        Q_EMIT errorOccurred(i18n("%1 is already added.", account.email));
        return;
    }
    if (m_pending) {
        discardSession(std::exchange(m_pending, nullptr), false);
    }
    m_pendingPassword.clear();
    m_pending = createSession(account);
    m_awaitingFolders.insert(m_pending);
    beginRequest(); // until the folders arrive: the browser sign-in is part of it
    m_pending->signInWithGoogle();
}

void MailController::cancelSetup()
{
    if (!m_pending) {
        return;
    }
    discardSession(std::exchange(m_pending, nullptr), false); // also ends its busy request
    m_pendingPassword.clear();
}

void MailController::removeAccount()
{
    removeAccount(accountEmail());
}

void MailController::removeAccount(const QString &account)
{
    if (m_pending && m_pending->account().email.compare(account, Qt::CaseInsensitive) == 0) {
        discardSession(std::exchange(m_pending, nullptr), false);
        m_pendingPassword.clear();
    }
    AccountSession *session = sessionFor(account);
    if (!session) {
        return;
    }
    const QString email = session->account().email;
    m_sessions.removeOne(session);
    m_checking.erase(session);
    m_newMail.forget(email);
    if (m_unreadByAccount.erase(email) > 0) {
        Q_EMIT unreadCountChanged();
    }
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
    m_secrets->deletePassword(email);
    saveAccounts();
    // The local copies belong to the removed account.
    discardSession(session, true);
    m_accounts->setSessions(m_sessions);
    m_sharedParts.erase(email);
    if (m_messageAccount == email) {
        clearMessage(true);
    }
    if (m_currentAccount == email || m_sessions.isEmpty()) {
        // The open folder is gone: back to All Inboxes, or to nothing without accounts.
        m_showingStored = false;
        m_currentAccount.clear();
        m_sharedParts.clear();
        m_messageModel->clear();
        clearMessage(true);
        if (!m_currentFolder.isEmpty()) {
            m_currentFolder.clear();
            Q_EMIT currentFolderChanged();
        }
        if (!m_sessions.isEmpty()) {
            selectFolder(QString(), QLatin1StringView(allInboxesPath));
        }
    } else if (isSharedView(m_currentFolder)) {
        mergeSharedParts(); // without the removed account's messages
    }
    updateStorageUsage();
    updateSyncing();
    Q_EMIT accountChanged();
}

void MailController::saveAccounts() const
{
    QList<Account> accounts;
    for (const AccountSession *session : m_sessions) {
        accounts.append(session->account());
    }
    Account::saveAll(accounts);
}

void MailController::connectSavedAccounts()
{
    for (AccountSession *session : std::as_const(m_sessions)) {
        if (session == uiSession()) {
            m_awaitingFolders.insert(session);
            beginRequest(); // ends when its folders arrive or it fails
        }
        beginRequest(); // ends with the keychain answer
        m_secrets->readPassword(session->account().email);
    }
}

void MailController::onSecretRead(const QString &key, const QString &password, const QString &error)
{
    endRequest();
    const auto it = std::find_if(m_sessions.cbegin(), m_sessions.cend(), [&key](const AccountSession *session) {
        return session->account().email == key;
    });
    if (it == m_sessions.cend()) {
        return; // the account was removed meanwhile
    }
    if (!error.isEmpty()) {
        if (m_awaitingFolders.erase(*it) > 0) {
            endRequest(); // its folders will not arrive
        }
        Q_EMIT errorOccurred(i18n("Could not read the password for %1 from the keychain: %2", key, error));
        return;
    }
    // For a Google account the keychain holds the refresh token, not a password.
    if ((*it)->account().authMethod == "oauth"_L1) {
        (*it)->connectWithRefreshToken(password);
    } else {
        (*it)->connectWithSecret(password);
    }
}

void MailController::onSessionError(AccountSession *session, const QString &message)
{
    if (!isKnown(session)) {
        return;
    }
    // A failed background check is only logged: offline, it would complain every few minutes.
    const bool quiet = m_checking.erase(session) > 0;
    if (quiet) {
        qWarning("Checking the inbox of %s failed: %s", qPrintable(session->account().email), qPrintable(message));
    }
    // A failed connection fails what was queued, including moves that were on their way.
    const QString movePrefix = session->account().email + u'\n';
    const size_t movesBefore = m_moving.size();
    for (auto it = m_moving.begin(); it != m_moving.end();) {
        if (it->startsWith(movePrefix)) {
            it = m_moving.erase(it);
            endRequest();
        } else {
            ++it;
        }
    }
    if (const bool anyDropped = m_moving.size() != movesBefore; anyDropped) {
        Q_EMIT currentMessageChanged(); // canArchive / canDelete come back
    }
    if (session == m_pending) {
        // Verification of a new account failed: forget it, keep whatever was there before.
        m_pending = nullptr;
        m_pendingPassword.clear();
        discardSession(session, false);
        resetRequests();
        Q_EMIT errorOccurred(message);
        return;
    }
    if (session == uiSession()) {
        // A failed connection or login fails every queued job, so nothing is in flight any more.
        resetRequests();
        if (!quiet) {
            Q_EMIT errorOccurred(message);
        }
        return;
    }
    if (!quiet) {
        Q_EMIT errorOccurred(u"%1: %2"_s.arg(session->account().email, message));
    }
}

void MailController::onSessionFolders(AccountSession *session)
{
    if (!isKnown(session)) {
        return;
    }
    if (session != m_pending) {
        if (m_awaitingFolders.erase(session) > 0) {
            endRequest();
        }
        if (session == currentSession()) {
            applyFolderList();
        }
        if (isSharedView(m_currentFolder)) {
            fetchSharedPart(session); // its inbox is known now (a no-op if already asked)
        }
        checkInbox(session); // the first one records what is there and counts the unread
        return;
    }

    // The login worked: now keep the password and add the account.
    if (m_awaitingFolders.erase(session) > 0) {
        endRequest();
    }
    m_pending = nullptr;
    const Account account = session->account();
    // Google accounts keep the refresh token in the keychain.
    const QString password = account.authMethod == "oauth"_L1 ? session->refreshToken() : std::exchange(m_pendingPassword, QString());
    m_pendingPassword.clear();
    connect(
        m_secrets,
        &SecretStore::passwordWritten,
        this,
        [this, session, account](const QString &key, const QString &error) {
            if (!error.isEmpty()) {
                discardSession(session, false);
                Q_EMIT errorOccurred(i18n("Could not save the password for %1 in the keychain: %2", key, error));
                return;
            }
            const bool firstAccount = m_sessions.isEmpty();
            m_sessions.append(session);
            session->setStorageSettings(m_storage); // they may have changed while it was verified
            saveAccounts();
            m_accounts->setSessions(m_sessions);
            updateStorageUsage();
            updateSyncing();
            Q_EMIT accountChanged();
            if (firstAccount) {
                m_showingStored = false;
                m_currentAccount.clear();
                m_sharedParts.clear();
                m_currentFolder.clear();
                m_messageModel->clear();
                clearMessage(true);
                applyFolderList();
            } else if (isSharedView(m_currentFolder)) {
                fetchSharedPart(session); // its inbox joins the open shared view
            }
        },
        Qt::SingleShotConnection);
    m_secrets->writePassword(account.email, password);
}

void MailController::applyFolderList()
{
    const AccountSession *session = currentSession();
    if (!session) {
        return;
    }
    const QList<FolderInfo> folders = session->folders();
    const bool wasShowingStored = std::exchange(m_showingStored, false);
    const bool currentStillExists = isSharedView(m_currentFolder) || std::any_of(folders.cbegin(), folders.cend(), [this](const FolderInfo &f) {
                                        return f.path == m_currentFolder;
                                    });
    if (!currentStillExists) {
        // First list after connecting, or the folder was deleted elsewhere: show All Inboxes.
        m_currentFolder.clear();
        m_messageModel->clear();
        clearMessage(true);
        Q_EMIT currentFolderChanged();
        selectFolder(QString(), QLatin1StringView(allInboxesPath));
    } else {
        Q_EMIT currentFolderChanged(); // the display name may have changed
        if (wasShowingStored) {
            selectFolder(m_currentAccount, m_currentFolder); // now ask the server too
        }
    }
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
