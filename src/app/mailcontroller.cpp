// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mailcontroller.h"

#include "accountlistmodel.h"
#include "accountsession.h"
#include "foldermodel.h"
#include "imapclient.h"
#include "messagelistmodel.h"
#include "messagerenderer.h"
#include "messagestore.h"
#include "secretstore.h"

#include <KLocalizedString>
#include <KMime/Message>

#include <QDir>
#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>

#include <algorithm>

using namespace Qt::StringLiterals;

MailController::MailController(QObject *parent)
    : QObject(parent)
    , m_dataDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , m_accounts(new AccountListModel(this))
    , m_noFolders(new FolderModel(this))
    , m_secrets(new SecretStore(QString::fromLatin1(AccountSecrets::service), this))
    , m_messageModel(new MessageListModel(this))
{
    connect(m_secrets, &SecretStore::passwordRead, this, &MailController::onSecretRead);

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

ImapClient *MailController::uiClient() const
{
    const AccountSession *session = uiSession();
    return session ? session->client() : nullptr;
}

MessageStore *MailController::uiStore() const
{
    const AccountSession *session = uiSession();
    return session && session->storageActive() ? session->store() : nullptr;
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
                if (session == uiSession()) {
                    onHeadersFetched(folder, filter, messages);
                }
            });
    connect(session, &AccountSession::messageFetched, this, [this, session](const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message) {
        if (session == uiSession()) {
            onMessageFetched(folder, uid, message);
        }
    });
    connect(session, &AccountSession::errorOccurred, this, [this, session](const QString &message) {
        onSessionError(session, message);
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
    return m_currentFolder.isEmpty() ? QString() : qobject_cast<FolderModel *>(folderModel())->displayName(m_currentFolder);
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
    return isSharedView(path) ? qobject_cast<FolderModel *>(folderModel())->inboxPath() : path;
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
    AccountSession *session = uiSession();
    if (!session || !session->showStoredFolders()) {
        return;
    }
    m_currentFolder = QLatin1StringView(allInboxesPath);
    m_messageModel->setMessages(storedMessages(m_currentFolder));
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

void MailController::selectFolder(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    ImapClient *client = uiClient();
    if (!client) {
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
    client->fetchHeaders(folder, messageLimit, filterFor(path));
}

bool MailController::storageActive() const
{
    return uiStore() != nullptr;
}

QList<MessageSummary> MailController::storedMessages(const QString &path) const
{
    const MessageStore *store = uiStore();
    if (!store) {
        return {};
    }
    const QString folder = imapFolderFor(path);
    const ImapClient::HeaderFilter filter = filterFor(path);
    if (filter == ImapClient::AllMessages) {
        return store->messages(folder, messageLimit);
    }
    QList<MessageSummary> result;
    for (const MessageSummary &message : store->messages(folder, -1)) {
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
    ImapClient *client = uiClient();
    if (!client) {
        return;
    }
    beginRequest();
    client->listFolders();
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
    if (!storageActive()) {
        m_messageModel->setMessages(messages);
        return;
    }
    // The server sends no body text; previews come from the local copies.
    const QHash<qint64, QString> previews = uiStore()->previews(folder);
    QList<MessageSummary> withPreviews = messages;
    for (MessageSummary &message : withPreviews) {
        message.preview = previews.value(message.uid);
    }
    m_messageModel->setMessages(withPreviews);
}

void MailController::openMessage(qint64 uid)
{
    ImapClient *client = uiClient();
    if (uid <= 0 || m_currentFolder.isEmpty() || !client) {
        return;
    }
    m_currentUid = uid;
    m_messageFolder = imapFolderFor(m_currentFolder);
    clearMessage(false);
    m_messageFlagged = m_messageModel->message(uid).flagged; // the parsed message carries no IMAP flags
    Q_EMIT currentMessageChanged(); // the flag shows while the message is still loading
    if (storageActive()) {
        if (const QByteArray raw = uiStore()->rawMessage(m_messageFolder, uid); !raw.isEmpty()) {
            // Parsed like KIMAP does with fetched messages.
            KMime::Message message;
            message.setContent(KMime::CRLFtoLF(raw));
            message.parse();
            showMessage(m_messageFolder, uid, message);
            return;
        }
    }
    beginRequest();
    client->fetchMessage(m_messageFolder, uid);
}

void MailController::setFlagged(qint64 uid, bool flagged)
{
    ImapClient *client = uiClient();
    if (uid <= 0 || !client) {
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
    client->setFlagged(folder, uid, flagged);
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
        uiClient()->markSeen(folder, uid);
        m_messageModel->setUnread(uid, false);
        if (MessageStore *store = uiStore(); store && store->contains(folder, uid)) {
            store->setUnread(folder, uid, false);
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

    if (m_pending) {
        discardSession(std::exchange(m_pending, nullptr), false);
    }
    m_pendingPassword = password;
    m_pending = createSession(account);
    beginRequest();
    m_pending->connectWithSecret(password);
}

void MailController::removeAccount()
{
    resetRequests();
    if (m_pending) {
        discardSession(std::exchange(m_pending, nullptr), false);
        m_pendingPassword.clear();
    }
    if (m_sessions.isEmpty()) {
        return;
    }
    // Only the first account for now (T90 lets the UI choose).
    AccountSession *session = m_sessions.takeFirst();
    const QString email = session->account().email;
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
    QList<Account> remaining;
    for (const AccountSession *other : std::as_const(m_sessions)) {
        remaining.append(other->account());
    }
    Account::saveAll(remaining);
    // The local copies belong to the removed account.
    discardSession(session, true);
    m_accounts->setSessions(m_sessions);
    m_showingStored = false;
    m_messageModel->clear();
    clearMessage(true);
    if (!m_currentFolder.isEmpty()) {
        m_currentFolder.clear();
        Q_EMIT currentFolderChanged();
    }
    updateStorageUsage();
    updateSyncing();
    Q_EMIT accountChanged();
    if (!m_sessions.isEmpty()) {
        selectFolder(QLatin1StringView(allInboxesPath));
    }
}

void MailController::connectSavedAccounts()
{
    for (const AccountSession *session : std::as_const(m_sessions)) {
        if (session->account().authMethod == "oauth"_L1) {
            continue; // Google accounts connect through the OAuth client (T92)
        }
        if (session == uiSession()) {
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
        if (*it == uiSession()) {
            endRequest(); // its folders will not arrive
        }
        Q_EMIT errorOccurred(i18n("Could not read the password for %1 from the keychain: %2", key, error));
        return;
    }
    (*it)->connectWithSecret(password);
}

void MailController::onSessionError(AccountSession *session, const QString &message)
{
    if (!isKnown(session)) {
        return;
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
        Q_EMIT errorOccurred(message);
        return;
    }
    Q_EMIT errorOccurred(u"%1: %2"_s.arg(session->account().email, message));
}

void MailController::onSessionFolders(AccountSession *session)
{
    if (!isKnown(session)) {
        return;
    }
    if (session != m_pending) {
        if (session == uiSession()) {
            endRequest();
            applyFolderList();
        }
        return;
    }

    // The login worked: now store the new account. It replaces the saved one (T90 will add
    // accounts instead); the replaced account's password is removed from the keychain.
    endRequest();
    m_pending = nullptr;
    const Account account = session->account();
    const QString password = std::exchange(m_pendingPassword, QString());
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
            const QList<AccountSession *> previous = std::exchange(m_sessions, {session});
            for (AccountSession *old : previous) {
                const bool sameAccount = old->account().email.compare(account.email, Qt::CaseInsensitive) == 0;
                if (!sameAccount) {
                    m_secrets->deletePassword(old->account().email);
                }
                discardSession(old, !sameAccount); // the same account's copies stay for the new session
            }
            Account::saveAll({account});
            m_accounts->setSessions(m_sessions);
            m_showingStored = false;
            m_currentFolder.clear();
            m_messageModel->clear();
            clearMessage(true);
            updateStorageUsage();
            updateSyncing();
            Q_EMIT accountChanged();
            applyFolderList();
        },
        Qt::SingleShotConnection);
    m_secrets->writePassword(account.email, password);
}

void MailController::applyFolderList()
{
    const AccountSession *session = uiSession();
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
        selectFolder(QLatin1StringView(allInboxesPath));
    } else {
        Q_EMIT currentFolderChanged(); // the display name may have changed
        if (wasShowingStored) {
            selectFolder(m_currentFolder); // now ask the server too
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
