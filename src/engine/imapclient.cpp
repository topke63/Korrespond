// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "imapclient.h"

#include <KIMAP/AppendJob>
#include <KIMAP/CapabilitiesJob>
#include <KIMAP/CopyJob>
#include <KIMAP/ExpungeJob>
#include <KIMAP/FetchJob>
#include <KIMAP/ListJob>
#include <KIMAP/LoginJob>
#include <KIMAP/LogoutJob>
#include <KIMAP/MoveJob>
#include <KIMAP/SearchJob>
#include <KIMAP/SelectJob>
#include <KIMAP/Session>
#include <KIMAP/StoreJob>
#include <KLocalizedString>
#include <KMime/Message>

#include <algorithm>
#include <functional>

using namespace Qt::StringLiterals;

// Most matches ImapClient::search() fetches: the newest this many by UID.
constexpr int SearchLimit = 200;

QDateTime messageDate(const KMime::Message &message)
{
    if (const auto *date = message.date(); date && date->dateTime().isValid()) {
        return date->dateTime();
    }
    // Each server prepends its Received header, so the first one is the newest. Its date
    // follows the last ';' (RFC 5322 section 3.6.7).
    for (const KMime::Headers::Base *received : message.headersByType("Received")) {
        const QByteArray value = received->as7BitString();
        const qsizetype semicolon = value.lastIndexOf(';');
        if (semicolon < 0) {
            continue;
        }
        KMime::Headers::Date date;
        date.from7BitString(value.mid(semicolon + 1).trimmed());
        if (date.dateTime().isValid()) {
            return date.dateTime();
        }
    }
    return {};
}

static MessageSummary summaryOf(const KIMAP::Message &msg, const QString &account)
{
    MessageSummary summary;
    summary.account = account;
    summary.uid = msg.uid;
    summary.unread = !msg.flags.contains("\\Seen");
    summary.flagged = msg.flags.contains("\\Flagged");
    if (const auto &mime = msg.message) {
        if (auto subject = mime->subject()) {
            summary.subject = subject->asUnicodeString();
        }
        if (auto from = mime->from()) {
            summary.from = from->displayString();
        }
        summary.date = messageDate(*mime);
    }
    return summary;
}

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

void ImapClient::setAccessToken(const QString &accessToken)
{
    m_password = accessToken;
}

void ImapClient::disconnectFromServer()
{
    ++m_generation; // whatever the old session still reports is no longer relevant
    dropSession();
}

QString ImapClient::secret() const
{
    return m_password;
}

bool ImapClient::hasSession() const
{
    return m_session != nullptr;
}

void ImapClient::dropSession()
{
    if (m_session) {
        m_session->close();
        m_session->deleteLater();
    }
    m_session = nullptr;
    m_selectedFolder.clear();
    m_capabilities.clear();
}

bool ImapClient::isCurrent(quint64 generation) const
{
    return generation == m_generation && generation != m_failedGeneration;
}

void ImapClient::fail(quint64 generation, const QString &message)
{
    if (!isCurrent(generation)) {
        return;
    }
    m_failedGeneration = generation;
    dropSession();
    Q_EMIT errorOccurred(message);
}

// KIMAP's text looks like "Login failed, server replied: A000002 NO LOGIN Invalid credentials
// [ AUTHENTICATIONFAILED ]". A NO reply means the server refused the credentials.
QString ImapClient::loginErrorMessage(const QString &jobError) const
{
    const bool rejected = jobError.contains("AUTHENTICATIONFAILED"_L1) || jobError.contains(" NO "_L1);
    if (!rejected) {
        return i18n("Could not log in: %1", jobError);
    }
    if (m_account.authMethod == "oauth"_L1) {
        return i18n("The server did not accept the Google sign-in. Please sign in again.");
    }
    if (m_account.provider == "yahoo"_L1) {
        return i18n(
            "Yahoo did not accept the email address or password. Yahoo needs an app password here, "
            "not your normal password.");
    }
    return i18n("The server did not accept the email address or password.");
}

KIMAP::Session *ImapClient::session()
{
    if (m_session) {
        return m_session;
    }

    m_session = new KIMAP::Session(m_account.imapHost, m_account.imapPort, this);
    m_session->setTimeout(60);
    m_selectedFolder.clear();
    const quint64 generation = ++m_generation;

    connect(m_session, &KIMAP::Session::connectionLost, this, [this, generation] {
        fail(generation, u"Connection to the server was lost."_s);
    });
    connect(m_session, &KIMAP::Session::connectionFailed, this, [this, generation] {
        fail(generation, u"Could not connect to %1."_s.arg(m_account.imapHost));
    });

    // Queued first; every later job waits for it.
    auto login = new KIMAP::LoginJob(m_session);
    login->setUserName(m_account.email);
    login->setPassword(m_password);
    login->setEncryptionMode(m_account.imapPort == 143 ? KIMAP::LoginJob::STARTTLS : KIMAP::LoginJob::SSLorTLS);
    login->setAuthenticationMode(m_account.authMethod == "oauth"_L1 ? KIMAP::LoginJob::XOAuth2 : KIMAP::LoginJob::ClearText);
    connect(login, &KJob::result, this, [this, generation](KJob *job) {
        if (job->error()) {
            fail(generation, loginErrorMessage(job->errorString()));
        }
    });
    login->start();

    return m_session;
}

void ImapClient::appendMessage(const QString &folder, const QByteArray &message, const QList<QByteArray> &flags)
{
    session();
    const quint64 generation = m_generation;
    auto append = new KIMAP::AppendJob(session());
    append->setMailBox(folder);
    append->setContent(message);
    append->setFlags(flags);
    append->setInternalDate(QDateTime::currentDateTime());
    connect(append, &KJob::result, this, [this, folder, generation](KJob *job) {
        if (checkJob(job, generation)) {
            Q_EMIT messageAppended(folder);
        }
    });
    append->start();
}

void ImapClient::moveMessage(const QString &from, qint64 uid, const QString &to)
{
    session();
    const quint64 generation = m_generation;
    queueSelect(from, generation);

    const auto proceed = [this, from, uid, to, generation] {
        if (!isCurrent(generation)) {
            return;
        }
        if (!m_capabilities.contains(u"MOVE"_s)) {
            moveByCopy(from, uid, to, generation);
            return;
        }
        auto move = new KIMAP::MoveJob(session());
        move->setUidBased(true);
        move->setSequenceSet(KIMAP::ImapSet(uid));
        move->setMailBox(to);
        connect(move, &KJob::result, this, [this, from, uid, to, generation](KJob *job) {
            if (checkJob(job, generation)) {
                Q_EMIT messageMoved(from, uid, to);
            }
        });
        move->start();
    };
    if (!m_capabilities.isEmpty()) {
        proceed();
        return;
    }
    // Asked once per session, after the login that is already queued.
    auto capabilities = new KIMAP::CapabilitiesJob(session());
    connect(capabilities, &KJob::result, this, [this, generation, proceed](KJob *job) {
        if (!checkJob(job, generation)) {
            return;
        }
        m_capabilities = static_cast<KIMAP::CapabilitiesJob *>(job)->capabilities();
        proceed();
    });
    capabilities->start();
}

// For servers without MOVE: COPY, flag the original \Deleted, and expunge only when that is
// safe. The original folder is still the selected one.
void ImapClient::moveByCopy(const QString &from, qint64 uid, const QString &to, quint64 generation)
{
    auto copy = new KIMAP::CopyJob(session());
    copy->setUidBased(true);
    copy->setSequenceSet(KIMAP::ImapSet(uid));
    copy->setMailBox(to);
    connect(copy, &KJob::result, this, [this, from, uid, to, generation](KJob *copyJob) {
        if (!checkJob(copyJob, generation)) {
            return;
        }
        const auto setDeleted = [this, uid, generation](bool deleted, const std::function<void()> &then) {
            auto store = new KIMAP::StoreJob(session());
            store->setUidBased(true);
            store->setSequenceSet(KIMAP::ImapSet(uid));
            store->setFlags({"\\Deleted"});
            store->setMode(deleted ? KIMAP::StoreJob::AppendFlags : KIMAP::StoreJob::RemoveFlags);
            connect(store, &KJob::result, this, [this, generation, then](KJob *job) {
                if (checkJob(job, generation) && then) {
                    then();
                }
            });
            store->start();
        };
        setDeleted(true, [this, from, uid, to, generation, setDeleted] {
            // Who else is flagged \Deleted in this folder? EXPUNGE would remove them as well.
            auto search = new KIMAP::SearchJob(session());
            search->setUidBased(true);
            search->setTerm(KIMAP::Term(KIMAP::Term::Deleted));
            connect(search, &KJob::result, this, [this, from, uid, to, generation, setDeleted](KJob *job) {
                if (!checkJob(job, generation)) {
                    return;
                }
                const QList<qint64> deleted = static_cast<KIMAP::SearchJob *>(job)->results();
                if (deleted.size() != 1 || deleted.constFirst() != uid) {
                    // Leave the original as it was; the copy stays in `to`.
                    setDeleted(false, {});
                    Q_EMIT errorOccurred(i18n(
                        "The message was copied, but the original could not be removed safely because other messages in this folder are marked as deleted."));
                    return;
                }
                auto expunge = new KIMAP::ExpungeJob(session());
                connect(expunge, &KJob::result, this, [this, from, uid, to, generation](KJob *expungeJob) {
                    if (checkJob(expungeJob, generation)) {
                        Q_EMIT messageMoved(from, uid, to);
                    }
                });
                expunge->start();
            });
            search->start();
        });
    });
    copy->start();
}

bool ImapClient::checkJob(KJob *job, quint64 generation)
{
    if (!isCurrent(generation)) {
        return false;
    }
    if (!job->error()) {
        return true;
    }
    // A job failing because the connection is gone ends the session; any other failure
    // (e.g. NO for a missing folder) is reported but keeps the session.
    if (!m_session || m_session->state() == KIMAP::Session::Disconnected) {
        fail(generation, job->errorString());
    } else {
        Q_EMIT errorOccurred(job->errorString());
    }
    return false;
}

void ImapClient::queueSelect(const QString &folder, quint64 generation)
{
    if (m_session && m_selectedFolder == folder) {
        return;
    }
    auto select = new KIMAP::SelectJob(session());
    select->setMailBox(folder);
    m_selectedFolder = folder;
    connect(select, &KJob::result, this, [this, generation](KJob *job) {
        if (job->error() && generation == m_generation) {
            m_selectedFolder.clear();
        }
    });
    select->start();
}

void ImapClient::listFolders()
{
    auto job = new KIMAP::ListJob(session());
    const quint64 generation = m_generation;
    // Plain LIST: servers with SPECIAL-USE (RFC 6154) put \Sent, \Trash etc. in its answer.
    // IncludeFolderRoleFlags would send XLIST, which only Gmail knows (Yahoo rejects it).
    job->setOption(KIMAP::ListJob::IncludeUnsubscribed);

    auto folders = std::make_shared<QList<FolderInfo>>();
    connect(job,
            &KIMAP::ListJob::mailBoxesReceived,
            this,
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

    connect(job, &KJob::result, this, [this, folders, generation](KJob *job) {
        if (!checkJob(job, generation)) {
            return;
        }
        // Special folders first, the rest alphabetically by path.
        sortFolders(*folders);
        Q_EMIT foldersListed(*folders);
    });
    job->start();
}

void ImapClient::checkInbox(const QString &folder, int limit)
{
    session();
    const quint64 generation = m_generation;
    // A folder that stays selected may keep showing the mail it had when it was selected, so the
    // check always selects it again (some servers only report new mail that way).
    m_selectedFolder.clear();
    queueSelect(folder, generation);
    auto search = new KIMAP::SearchJob(session());
    search->setUidBased(true);
    search->setTerm(KIMAP::Term(KIMAP::Term::Seen).setNegated(true));
    connect(search, &KJob::result, this, [this, folder, limit, generation](KJob *job) {
        if (!checkJob(job, generation)) {
            return;
        }
        QList<qint64> uids = static_cast<KIMAP::SearchJob *>(job)->results();
        const int total = int(uids.size());
        if (uids.isEmpty() || !m_session) {
            Q_EMIT inboxChecked(folder, 0, {});
            return;
        }
        std::sort(uids.begin(), uids.end());
        KIMAP::ImapSet set;
        set.add(QList<qint64>(uids.cend() - std::min<qsizetype>(limit, uids.size()), uids.cend()));
        fetchSummaries(set, true, folder, Unread, generation, [this, folder, total](const QList<MessageSummary> &messages) {
            Q_EMIT inboxChecked(folder, total, messages);
        });
    });
    search->start();
}

void ImapClient::fetchHeaders(const QString &folder, int limit, HeaderFilter filter)
{
    if (filter != AllMessages) {
        // UID SEARCH in the folder, then the headers of the newest `limit` matches.
        session();
        const quint64 generation = m_generation;
        queueSelect(folder, generation);
        auto search = new KIMAP::SearchJob(session());
        search->setUidBased(true);
        search->setTerm(filter == Unread ? KIMAP::Term(KIMAP::Term::Seen).setNegated(true) : KIMAP::Term(KIMAP::Term::Flagged));
        connect(search, &KJob::result, this, [this, folder, limit, filter, generation](KJob *job) {
            if (!checkJob(job, generation)) {
                return;
            }
            QList<qint64> uids = static_cast<KIMAP::SearchJob *>(job)->results();
            if (uids.isEmpty() || !m_session) {
                Q_EMIT headersFetched(folder, filter, {});
                return;
            }
            std::sort(uids.begin(), uids.end());
            KIMAP::ImapSet set;
            set.add(QList<qint64>(uids.cend() - std::min<qsizetype>(limit, uids.size()), uids.cend()));
            fetchSummaries(set, true, folder, filter, generation);
        });
        search->start();
        return;
    }

    // SELECT is issued directly here because we need its message count.
    auto select = new KIMAP::SelectJob(session());
    const quint64 generation = m_generation;
    select->setMailBox(folder);
    m_selectedFolder = folder;

    connect(select, &KJob::result, this, [this, folder, limit, filter, generation](KJob *job) {
        if (!checkJob(job, generation)) {
            if (generation == m_generation) {
                m_selectedFolder.clear();
            }
            return;
        }
        const int count = static_cast<KIMAP::SelectJob *>(job)->messageCount();
        if (count <= 0 || !m_session) {
            Q_EMIT headersFetched(folder, filter, {});
            return;
        }
        fetchSummaries(KIMAP::ImapSet(std::max(1, count - limit + 1), count), false, folder, filter, generation);
    });
    select->start();
}

void ImapClient::search(const QString &folder, const QString &text)
{
    // A blank term would match everything; treat it as "no results" without asking the server.
    if (text.trimmed().isEmpty()) {
        Q_EMIT headersFetched(folder, AllMessages, {});
        return;
    }
    session();
    const quint64 generation = m_generation;
    queueSelect(folder, generation);
    const QString needle = text.trimmed();
    auto search = new KIMAP::SearchJob(session());
    search->setUidBased(true);
    search->setTerm(KIMAP::Term(KIMAP::Term::Or,
                                {KIMAP::Term(KIMAP::Term::Subject, needle), KIMAP::Term(KIMAP::Term::From, needle), KIMAP::Term(KIMAP::Term::Body, needle)}));
    connect(search, &KJob::result, this, [this, folder, generation](KJob *job) {
        if (!checkJob(job, generation)) {
            return;
        }
        QList<qint64> uids = static_cast<KIMAP::SearchJob *>(job)->results();
        if (uids.isEmpty() || !m_session) {
            Q_EMIT headersFetched(folder, AllMessages, {});
            return;
        }
        std::sort(uids.begin(), uids.end());
        KIMAP::ImapSet set;
        set.add(QList<qint64>(uids.cend() - std::min<qsizetype>(SearchLimit, uids.size()), uids.cend()));
        fetchSummaries(set, true, folder, AllMessages, generation);
    });
    search->start();
}

void ImapClient::searchSince(const QString &folder, const QDate &since)
{
    // SELECT is issued directly here because we need its UIDVALIDITY.
    auto select = new KIMAP::SelectJob(session());
    const quint64 generation = m_generation;
    select->setMailBox(folder);
    m_selectedFolder = folder;

    connect(select, &KJob::result, this, [this, folder, since, generation](KJob *job) {
        if (!checkJob(job, generation)) {
            if (generation == m_generation) {
                m_selectedFolder.clear();
            }
            return;
        }
        const auto selectJob = static_cast<KIMAP::SelectJob *>(job);
        const qint64 uidValidity = selectJob->uidValidity();
        // Some servers reject SEARCH 1:* in an empty folder, so don't ask.
        if (selectJob->messageCount() <= 0 || !m_session) {
            Q_EMIT searchFinished(folder, uidValidity, {});
            return;
        }
        auto search = new KIMAP::SearchJob(m_session);
        search->setUidBased(true);
        // IMAP has no plain "ALL" term in KIMAP; 1:* (every sequence number) means the same.
        search->setTerm(since.isValid() ? KIMAP::Term(KIMAP::Term::Since, since) : KIMAP::Term(KIMAP::Term::SequenceNumber, KIMAP::ImapSet(1, 0)));
        connect(search, &KJob::result, this, [this, folder, uidValidity, generation](KJob *job) {
            if (!checkJob(job, generation)) {
                return;
            }
            QList<qint64> uids = static_cast<KIMAP::SearchJob *>(job)->results();
            std::sort(uids.begin(), uids.end());
            Q_EMIT searchFinished(folder, uidValidity, uids);
        });
        search->start();
    });
    select->start();
}

void ImapClient::fetchRawMessages(const QString &folder, const QList<qint64> &uids)
{
    if (uids.isEmpty()) {
        Q_EMIT rawFetchFinished(folder);
        return;
    }
    session();
    const quint64 generation = m_generation;
    queueSelect(folder, generation);

    auto fetch = new KIMAP::FetchJob(session());
    fetch->setUidBased(true);
    KIMAP::ImapSet set;
    set.add(uids);
    fetch->setSequenceSet(set);
    KIMAP::FetchJob::FetchScope scope;
    scope.mode = KIMAP::FetchJob::FetchScope::Full; // BODY.PEEK[]: doesn't set \Seen
    fetch->setScope(scope);

    connect(fetch, &KIMAP::FetchJob::messagesAvailable, this, [this, folder, generation](const QMap<qint64, KIMAP::Message> &batch) {
        if (!isCurrent(generation)) {
            return;
        }
        for (const KIMAP::Message &msg : batch) {
            if (msg.message) {
                Q_EMIT rawMessageFetched(folder, summaryOf(msg, m_account.email), msg.message->encodedContent(KMime::NewlineType::CRLF));
            }
        }
    });
    connect(fetch, &KJob::result, this, [this, folder, generation](KJob *job) {
        if (!checkJob(job, generation)) {
            return;
        }
        Q_EMIT rawFetchFinished(folder);
    });
    fetch->start();
}

void ImapClient::fetchSummaries(const KIMAP::ImapSet &set,
                                bool uidBased,
                                const QString &folder,
                                HeaderFilter filter,
                                quint64 generation,
                                const std::function<void(const QList<MessageSummary> &)> &done)
{
    auto fetch = new KIMAP::FetchJob(m_session);
    fetch->setUidBased(uidBased);
    fetch->setSequenceSet(set);
    KIMAP::FetchJob::FetchScope scope;
    scope.mode = KIMAP::FetchJob::FetchScope::Headers;
    fetch->setScope(scope);

    auto messages = std::make_shared<QList<MessageSummary>>();
    connect(fetch, &KIMAP::FetchJob::messagesAvailable, this, [messages, email = m_account.email](const QMap<qint64, KIMAP::Message> &batch) {
        for (const KIMAP::Message &msg : batch) {
            messages->append(summaryOf(msg, email));
        }
    });
    connect(fetch, &KJob::result, this, [this, folder, filter, messages, generation, done](KJob *job) {
        if (!checkJob(job, generation)) {
            return;
        }
        std::sort(messages->begin(), messages->end(), [](const MessageSummary &a, const MessageSummary &b) {
            return a.uid > b.uid; // newest first
        });
        if (done) {
            done(*messages);
        } else {
            Q_EMIT headersFetched(folder, filter, *messages);
        }
    });
    fetch->start();
}

void ImapClient::fetchMessage(const QString &folder, qint64 uid)
{
    session();
    const quint64 generation = m_generation;
    queueSelect(folder, generation);

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
    connect(fetch, &KJob::result, this, [this, folder, uid, result, generation](KJob *job) {
        if (!checkJob(job, generation)) {
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
    setSeen(folder, uid, true);
}

void ImapClient::setSeen(const QString &folder, qint64 uid, bool seen)
{
    session();
    const quint64 generation = m_generation;
    queueSelect(folder, generation);

    auto store = new KIMAP::StoreJob(session());
    store->setUidBased(true);
    store->setSequenceSet(KIMAP::ImapSet(uid));
    store->setFlags({"\\Seen"});
    store->setMode(seen ? KIMAP::StoreJob::AppendFlags : KIMAP::StoreJob::RemoveFlags);
    connect(store, &KJob::result, this, [this, generation](KJob *job) {
        checkJob(job, generation);
    });
    store->start();
}

void ImapClient::setFlagged(const QString &folder, qint64 uid, bool flagged)
{
    session();
    const quint64 generation = m_generation;
    queueSelect(folder, generation);

    auto store = new KIMAP::StoreJob(session());
    store->setUidBased(true);
    store->setSequenceSet(KIMAP::ImapSet(uid));
    store->setFlags({"\\Flagged"});
    store->setMode(flagged ? KIMAP::StoreJob::AppendFlags : KIMAP::StoreJob::RemoveFlags);
    connect(store, &KJob::result, this, [this, generation](KJob *job) {
        checkJob(job, generation);
    });
    store->start();
}
