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
        SubjectRole,
        FromRole,
        DateRole,
        DateTextRole,
        UnreadRole,
        FlaggedRole,
    };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QList<MessageSummary> &messages);
    void clear();
    // Updates one message's unread state; emits dataChanged for that row only.
    void setUnread(qint64 uid, bool unread);
    void setFlagged(qint64 uid, bool flagged);
    // Returns a default MessageSummary (uid 0) if `uid` is not in the list.
    [[nodiscard]] MessageSummary message(qint64 uid) const;

    // Short local-time text: time of day for today, short date otherwise.
    [[nodiscard]] static QString dateText(const QDateTime &dateTime, const QDate &today);

private:
    QList<MessageSummary> m_messages;
};
