// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagelistmodel.h"

#include <KLocalizedString>

#include <QLocale>

MessageListModel::MessageListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

QString MessageListModel::dateText(const QDateTime &dateTime, const QDate &today)
{
    if (!dateTime.isValid()) {
        return {};
    }
    // Mail dates carry the sender's time zone; compare and show them in ours.
    const QDateTime local = dateTime.toLocalTime();
    if (local.date() == today) {
        return QLocale().toString(local.time(), QLocale::ShortFormat);
    }
    return QLocale().toString(local.date(), QLocale::ShortFormat);
}

int MessageListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_messages.size());
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const MessageSummary &message = m_messages.at(index.row());
    switch (role) {
    case UidRole:
        return message.uid;
    case Qt::DisplayRole:
    case SubjectRole:
        return message.subject.isEmpty() ? i18nc("@info placeholder for an empty mail subject", "(No subject)") : message.subject;
    case FromRole:
        return message.from;
    case DateRole:
        return message.date;
    case DateTextRole:
        return dateText(message.date, QDate::currentDate());
    case UnreadRole:
        return message.unread;
    case FlaggedRole:
        return message.flagged;
    case PreviewRole:
        return message.preview;
    }
    return {};
}

QHash<int, QByteArray> MessageListModel::roleNames() const
{
    return {
        {UidRole, "uid"},
        {SubjectRole, "subject"},
        {FromRole, "from"},
        {DateRole, "date"},
        {DateTextRole, "dateText"},
        {UnreadRole, "unread"},
        {FlaggedRole, "flagged"},
        {PreviewRole, "preview"},
    };
}

void MessageListModel::setMessages(const QList<MessageSummary> &messages)
{
    beginResetModel();
    m_messages = messages;
    endResetModel();
}

void MessageListModel::clear()
{
    setMessages({});
}

void MessageListModel::setUnread(qint64 uid, bool unread)
{
    for (qsizetype row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].uid != uid) {
            continue;
        }
        if (m_messages[row].unread != unread) {
            m_messages[row].unread = unread;
            const QModelIndex idx = index(int(row));
            Q_EMIT dataChanged(idx, idx, {UnreadRole});
        }
        return;
    }
}

void MessageListModel::setFlagged(qint64 uid, bool flagged)
{
    for (qsizetype row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].uid != uid) {
            continue;
        }
        if (m_messages[row].flagged != flagged) {
            m_messages[row].flagged = flagged;
            const QModelIndex idx = index(int(row));
            Q_EMIT dataChanged(idx, idx, {FlaggedRole});
        }
        return;
    }
}

MessageSummary MessageListModel::message(qint64 uid) const
{
    for (const MessageSummary &msg : m_messages) {
        if (msg.uid == uid) {
            return msg;
        }
    }
    return {};
}
