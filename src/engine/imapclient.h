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
};

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

    void listFolders();
    // Fetches headers of the newest `limit` messages in `folder`.
    void fetchHeaders(const QString &folder, int limit);
    void fetchMessage(const QString &folder, qint64 uid);
    void markSeen(const QString &folder, qint64 uid);

Q_SIGNALS:
    void foldersListed(const QList<FolderInfo> &folders);
    void headersFetched(const QString &folder, const QList<MessageSummary> &messages);
    void messageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    void errorOccurred(const QString &message);

private:
    // Returns the session, creating it (and queueing the login) if needed. Each new session
    // gets a new generation number; callbacks carry the number they were created for.
    KIMAP::Session *session();
    // True while callbacks of `generation` should still be acted on: it is the current
    // session and no failure has been reported for it yet.
    [[nodiscard]] bool isCurrent(quint64 generation) const;
    // Reports a fatal error once per session and drops the session; later errors of the
    // same (or an older) session are ignored. The next request opens a new session.
    void fail(quint64 generation, const QString &message);
    void dropSession();
    void queueSelect(const QString &folder, quint64 generation);
    // false if the job failed or belongs to an outdated session; reports the error if relevant.
    bool checkJob(KJob *job, quint64 generation);

    Account m_account;
    QString m_password;
    QPointer<KIMAP::Session> m_session;
    QString m_selectedFolder; // folder of the last queued SELECT
    quint64 m_generation = 0; // number of the current (or last) session
    quint64 m_failedGeneration = 0; // session whose failure was already reported
};
