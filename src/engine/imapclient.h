// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "folderutils.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>

#include <memory>

namespace KIMAP
{
class ImapSet;
class Session;
}
namespace KMime
{
class Message;
}
class KJob;

struct MessageSummary {
    qint64 uid = 0;
    QString subject;
    QString from;
    QDateTime date;
    bool unread = false;
    bool flagged = false;
    QString preview; // body excerpt; only messages in local storage have one
};

// When the message was sent: its Date header, else the date of the newest Received header
// (the last mail server's arrival time), else null. Yahoo webmail sends mail without Date.
QDateTime messageDate(const KMime::Message &message);

// Thin asynchronous wrapper around one KIMAP session.
// KIMAP queues jobs per session, so callers can issue requests back to back.
class ImapClient : public QObject
{
    Q_OBJECT
public:
    explicit ImapClient(QObject *parent = nullptr);
    ~ImapClient() override;

    void setAccount(const Account &account, const QString &password);
    void disconnectFromServer();

    // Which messages fetchHeaders() returns.
    enum HeaderFilter {
        AllMessages,
        Unread,
        Flagged
    };

    void listFolders();
    // Fetches headers of the newest `limit` messages in `folder` that match `filter`.
    void fetchHeaders(const QString &folder, int limit, HeaderFilter filter = AllMessages);
    void fetchMessage(const QString &folder, qint64 uid);
    void markSeen(const QString &folder, qint64 uid);
    // Sets or clears \Flagged (shown as a star or flag by mail clients).
    void setFlagged(const QString &folder, qint64 uid, bool flagged);
    // UIDs of the messages in folder that arrived on or after since (the server's arrival date,
    // not the Date header); a null date means all messages. Emits searchFinished().
    void searchSince(const QString &folder, const QDate &since);
    // Downloads whole messages (attachments included) without marking them read. Emits
    // rawMessageFetched() for each one, in no particular order, then rawFetchFinished(). uids
    // gone from the server are skipped. Callers pass batches; this sends one FETCH.
    void fetchRawMessages(const QString &folder, const QList<qint64> &uids);

Q_SIGNALS:
    void foldersListed(const QList<FolderInfo> &folders);
    // `filter` is the one passed to fetchHeaders(), so results for the same folder can be told apart.
    void headersFetched(const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages);
    // uids are in ascending order. uidValidity is the folder's current UIDVALIDITY: when it
    // differs from the stored one, every stored uid of the folder is stale.
    void searchFinished(const QString &folder, qint64 uidValidity, const QList<qint64> &uids);
    // raw has CRLF line endings, as on the wire.
    void rawMessageFetched(const QString &folder, const MessageSummary &summary, const QByteArray &raw);
    // Not emitted when the fetch failed; errorOccurred() is emitted instead.
    void rawFetchFinished(const QString &folder);
    void messageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    void errorOccurred(const QString &message);

private:
    // Returns the session, creating it (and queueing the login) if needed. Each new session
    // gets a new generation number; callbacks carry the number they were created for.
    KIMAP::Session *session();
    // One readable sentence for a failed LoginJob, without IMAP protocol text.
    [[nodiscard]] QString loginErrorMessage(const QString &jobError) const;
    // True while callbacks of `generation` should still be acted on: it is the current
    // session and no failure has been reported for it yet.
    [[nodiscard]] bool isCurrent(quint64 generation) const;
    // Reports a fatal error once per session and drops the session; later errors of the
    // same (or an older) session are ignored. The next request opens a new session.
    void fail(quint64 generation, const QString &message);
    void dropSession();
    void queueSelect(const QString &folder, quint64 generation);
    // Fetches headers for `set` (UIDs if uidBased, else sequence numbers) of the selected folder
    // and emits headersFetched() newest first.
    void fetchSummaries(const KIMAP::ImapSet &set, bool uidBased, const QString &folder, HeaderFilter filter, quint64 generation);
    // false if the job failed or belongs to an outdated session; reports the error if relevant.
    bool checkJob(KJob *job, quint64 generation);

    Account m_account;
    QString m_password;
    QPointer<KIMAP::Session> m_session;
    QString m_selectedFolder; // folder of the last queued SELECT
    quint64 m_generation = 0; // number of the current (or last) session
    quint64 m_failedGeneration = 0; // session whose failure was already reported
};
