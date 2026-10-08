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
    KIMAP::Session *session();
    void queueSelect(const QString &folder);
    bool checkJob(KJob *job);

    Account m_account;
    QString m_password;
    QPointer<KIMAP::Session> m_session;
    QString m_selectedFolder; // folder of the last queued SELECT
};
