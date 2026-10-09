// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "imapclient.h"

#include <QAbstractListModel>
#include <QList>

class MessageListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UidRole = Qt::UserRole + 1,
        AccountRole, // email address of the message's account
        SubjectRole,
        FromRole,
        DateRole,
        DateTextRole,
        UnreadRole,
        FlaggedRole,
        PreviewRole,
    };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QList<MessageSummary> &messages);
    void clear();
    // A uid is only unique within one account, so every lookup names the account.
    // Updates one message's unread state; emits dataChanged for that row only.
    void setUnread(const QString &account, qint64 uid, bool unread);
    void setFlagged(const QString &account, qint64 uid, bool flagged);
    // Returns a default MessageSummary (uid 0) if the message is not in the list.
    [[nodiscard]] MessageSummary message(const QString &account, qint64 uid) const;

    // Short local-time text: time of day for today, short date otherwise.
    [[nodiscard]] static QString dateText(const QDateTime &dateTime, const QDate &today);

private:
    QList<MessageSummary> m_messages;
};
