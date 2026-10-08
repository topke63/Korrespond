// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imapclient.h"

#include <KIMAP/FetchJob>
#include <KIMAP/ListJob>
#include <KIMAP/LoginJob>
#include <KIMAP/LogoutJob>
#include <KIMAP/SelectJob>
#include <KIMAP/Session>
#include <KIMAP/StoreJob>
#include <KMime/Message>

#include <algorithm>

using namespace Qt::StringLiterals;

ImapClient::ImapClient(QObject *parent)
    : QObject(parent)
{
}

ImapClient::~ImapClient()
{
    disconnectFromServer();
}

void ImapClient::setAccount(const Account &account, const QString &password)
{
    disconnectFromServer();
    m_account = account;
    m_password = password;
}

void ImapClient::disconnectFromServer()
{
    if (m_session) {
        m_session->close();
        m_session->deleteLater();
    }
    m_session = nullptr;
    m_selectedFolder.clear();
}

KIMAP::Session *ImapClient::session()
{
    if (m_session) {
        return m_session;
    }

    m_session = new KIMAP::Session(m_account.imapHost, m_account.imapPort, this);
    m_session->setTimeout(60);
    m_selectedFolder.clear();

    auto dropSession = [this] {
        if (m_session) {
            m_session->deleteLater();
        }
        m_session = nullptr;
        m_selectedFolder.clear();
    };
    connect(m_session, &KIMAP::Session::connectionLost, this, dropSession);
    connect(m_session, &KIMAP::Session::connectionFailed, this, [this, dropSession] {
        Q_EMIT errorOccurred(u"Could not connect to %1."_s.arg(m_account.imapHost));
        dropSession();
    });

    // Queued first; every later job waits for it.
    auto login = new KIMAP::LoginJob(m_session);
    login->setUserName(m_account.email);
    login->setPassword(m_password);
    login->setEncryptionMode(m_account.imapPort == 143 ? KIMAP::LoginJob::STARTTLS : KIMAP::LoginJob::SSLorTLS);
    login->setAuthenticationMode(KIMAP::LoginJob::ClearText);
    connect(login, &KJob::result, this, [this](KJob *job) {
        if (job->error()) {
            Q_EMIT errorOccurred(u"Login failed: %1"_s.arg(job->errorString()));
        }
    });
    login->start();

    return m_session;
}

bool ImapClient::checkJob(KJob *job)
{
    if (job->error()) {
        Q_EMIT errorOccurred(job->errorString());
        return false;
    }
    return true;
}

void ImapClient::queueSelect(const QString &folder)
{
    if (m_session && m_selectedFolder == folder) {
        return;
    }
    auto select = new KIMAP::SelectJob(session());
    select->setMailBox(folder);
    m_selectedFolder = folder;
    connect(select, &KJob::result, this, [this](KJob *job) {
        if (job->error()) {
            m_selectedFolder.clear();
        }
    });
    select->start();
}

void ImapClient::listFolders()
{
    auto job = new KIMAP::ListJob(session());
    job->setOption(KIMAP::ListJob::IncludeFolderRoleFlags);

    auto folders = std::make_shared<QList<FolderInfo>>();
    connect(job, &KIMAP::ListJob::mailBoxesReceived, this,
            [folders](const QList<KIMAP::MailBoxDescriptor> &descriptors, const QList<QList<QByteArray>> &flags) {
                for (qsizetype i = 0; i < descriptors.size(); ++i) {
                    const auto &descriptor = descriptors[i];
                    const QList<QByteArray> folderFlags = flags.value(i);
                    const bool selectable = std::none_of(folderFlags.begin(), folderFlags.end(), [](const QByteArray &f) {
                        return f.compare("\\noselect", Qt::CaseInsensitive) == 0 || f.compare("\\nonexistent", Qt::CaseInsensitive) == 0;
                    });
                    if (!selectable) {
                        continue;
                    }
                    FolderInfo info;
                    info.path = descriptor.name;
                    const QStringList parts = descriptor.separator.isNull() ? QStringList{descriptor.name} : descriptor.name.split(descriptor.separator);
                    info.name = parts.last();
                    info.depth = int(parts.size()) - 1;
                    info.role = folderRoleFromFlags(descriptor.name, folderFlags);
                    folders->append(info);
                }
            });

    connect(job, &KJob::result, this, [this, folders](KJob *job) {
        if (!checkJob(job)) {
            return;
        }
        // Special folders first, the rest alphabetically by path.
        sortFolders(*folders);
        Q_EMIT foldersListed(*folders);
    });
    job->start();
}

void ImapClient::fetchHeaders(const QString &folder, int limit)
{
    // SELECT is issued directly here because we need its message count.
    auto select = new KIMAP::SelectJob(session());
    select->setMailBox(folder);
    m_selectedFolder = folder;

    connect(select, &KJob::result, this, [this, folder, limit](KJob *job) {
        if (!checkJob(job)) {
            m_selectedFolder.clear();
            return;
        }
        const int count = static_cast<KIMAP::SelectJob *>(job)->messageCount();
        if (count <= 0 || !m_session) {
            Q_EMIT headersFetched(folder, {});
            return;
        }

        auto fetch = new KIMAP::FetchJob(m_session);
        fetch->setSequenceSet(KIMAP::ImapSet(std::max(1, count - limit + 1), count));
        KIMAP::FetchJob::FetchScope scope;
        scope.mode = KIMAP::FetchJob::FetchScope::Headers;
        fetch->setScope(scope);

        auto messages = std::make_shared<QList<MessageSummary>>();
        connect(fetch, &KIMAP::FetchJob::messagesAvailable, this, [messages](const QMap<qint64, KIMAP::Message> &batch) {
            for (const KIMAP::Message &msg : batch) {
                MessageSummary summary;
                summary.uid = msg.uid;
                summary.unread = !msg.flags.contains("\\Seen");
                if (const auto &mime = msg.message) {
                    if (auto subject = mime->subject()) {
                        summary.subject = subject->asUnicodeString();
                    }
                    if (auto from = mime->from()) {
                        summary.from = from->displayString();
                    }
                    if (auto date = mime->date()) {
                        summary.date = date->dateTime();
                    }
                }
                messages->append(summary);
            }
        });
        connect(fetch, &KJob::result, this, [this, folder, messages](KJob *job) {
            if (!checkJob(job)) {
                return;
            }
            std::sort(messages->begin(), messages->end(), [](const MessageSummary &a, const MessageSummary &b) {
                return a.uid > b.uid; // newest first
            });
            Q_EMIT headersFetched(folder, *messages);
        });
        fetch->start();
    });
    select->start();
}

void ImapClient::fetchMessage(const QString &folder, qint64 uid)
{
    queueSelect(folder);

    auto fetch = new KIMAP::FetchJob(session());
    fetch->setUidBased(true);
    fetch->setSequenceSet(KIMAP::ImapSet(uid));
    KIMAP::FetchJob::FetchScope scope;
    scope.mode = KIMAP::FetchJob::FetchScope::Full;
    fetch->setScope(scope);

    auto result = std::make_shared<std::shared_ptr<KMime::Message>>();
    connect(fetch, &KIMAP::FetchJob::messagesAvailable, this, [result](const QMap<qint64, KIMAP::Message> &batch) {
        if (!batch.isEmpty()) {
            *result = batch.first().message;
        }
    });
    connect(fetch, &KJob::result, this, [this, folder, uid, result](KJob *job) {
        if (!checkJob(job)) {
            return;
        }
        if (!*result) {
            Q_EMIT errorOccurred(u"The message no longer exists on the server."_s);
            return;
        }
        Q_EMIT messageFetched(folder, uid, *result);
    });
    fetch->start();
}

void ImapClient::markSeen(const QString &folder, qint64 uid)
{
    queueSelect(folder);

    auto store = new KIMAP::StoreJob(session());
    store->setUidBased(true);
    store->setSequenceSet(KIMAP::ImapSet(uid));
    store->setFlags({"\\Seen"});
    store->setMode(KIMAP::StoreJob::AppendFlags);
    connect(store, &KJob::result, this, &ImapClient::checkJob);
    store->start();
}
