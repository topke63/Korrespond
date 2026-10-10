// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagelistmodel.h"

#include "threadbuilder.h"

#include <KLocalizedString>

#include <QHash>
#include <QLocale>
#include <QSet>

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
    if (parent.isValid()) {
        return 0;
    }
    return int(m_conversationView ? m_rows.size() : m_messages.size());
}

QVariant MessageListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const MessageSummary &message = m_conversationView ? representative(m_rows.at(index.row())) : m_messages.at(index.row());
    if (role >= ThreadIdRole) {
        if (m_conversationView) {
            const Row &row = m_rows.at(index.row());
            switch (role) {
            case ThreadIdRole:
                return row.key;
            case ThreadCountRole:
                return row.count;
            case ParticipantsRole:
                return row.participants;
            case ThreadUnreadRole:
                return row.unread;
            case ThreadFlaggedRole:
                return row.flagged;
            case ThreadUidsRole: {
                QVariantList uids;
                for (const qint64 uid : row.uids) {
                    uids.append(uid);
                }
                return uids;
            }
            }
            return {};
        }
        // Flat view: a conversation of one.
        switch (role) {
        case ThreadIdRole:
            return QString(message.account + u'\n' + QString::number(message.uid));
        case ThreadCountRole:
            return 1;
        case ParticipantsRole: {
            const QString name = participantName(message.from);
            return participantAddress(message.from) == message.account.toLower() ? i18nc("@item the user themselves in a list of senders", "me") : name;
        }
        case ThreadUnreadRole:
            return message.unread;
        case ThreadFlaggedRole:
            return message.flagged;
        case ThreadUidsRole:
            return QVariantList{message.uid};
        }
        return {};
    }
    switch (role) {
    case UidRole:
        return message.uid;
    case AccountRole:
        return message.account;
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
        {AccountRole, "account"},
        {SubjectRole, "subject"},
        {FromRole, "from"},
        {DateRole, "date"},
        {DateTextRole, "dateText"},
        {UnreadRole, "unread"},
        {FlaggedRole, "flagged"},
        {PreviewRole, "preview"},
        {ThreadIdRole, "threadId"},
        {ThreadCountRole, "threadCount"},
        {ParticipantsRole, "participants"},
        {ThreadUnreadRole, "threadUnread"},
        {ThreadFlaggedRole, "threadFlagged"},
        {ThreadUidsRole, "threadUids"},
    };
}

QString MessageListModel::participantAddress(const QString &from)
{
    const qsizetype open = from.lastIndexOf(u'<');
    const qsizetype close = from.lastIndexOf(u'>');
    if (open >= 0 && close > open) {
        return from.mid(open + 1, close - open - 1).trimmed().toLower();
    }
    return from.trimmed().toLower();
}

QString MessageListModel::participantName(const QString &from)
{
    const qsizetype open = from.lastIndexOf(u'<');
    QString name = open > 0 ? from.left(open) : QString();
    name = name.trimmed();
    if (name.size() >= 2 && name.startsWith(u'"') && name.endsWith(u'"')) {
        name = name.mid(1, name.size() - 2).trimmed();
    }
    if (name.isEmpty() || name.contains(u'@')) {
        // No usable name: the part of the address before the @.
        const QString address = participantAddress(from);
        const qsizetype at = address.indexOf(u'@');
        return at > 0 ? address.left(at) : address;
    }
    const qsizetype comma = name.indexOf(u',');
    if (comma > 0 && comma + 1 < name.size()) {
        return name.mid(comma + 1).trimmed().section(u' ', 0, 0); // "Doe, John"
    }
    return name.section(u' ', 0, 0);
}

QList<MessageListModel::Row> MessageListModel::buildRows() const
{
    // The grouping is per account (a uid is only unique within one).
    QHash<QString, QList<MessageSummary>> perAccount;
    for (const MessageSummary &message : m_messages) {
        perAccount[message.account].append(message);
    }
    QHash<QString, QHash<qint64, QString>> threadOf;
    for (auto it = perAccount.cbegin(); it != perAccount.cend(); ++it) {
        threadOf.insert(it.key(), ThreadBuilder::build(it.value()));
    }

    QList<Row> rows;
    QHash<QString, int> rowOfKey;
    QHash<QString, QList<const MessageSummary *>> members; // key → messages in list order
    for (const MessageSummary &message : m_messages) {
        const QString key = message.account + u'\n' + threadOf.value(message.account).value(message.uid);
        auto it = rowOfKey.find(key);
        if (it == rowOfKey.end()) {
            Row row;
            row.key = key;
            row.account = message.account;
            row.uid = message.uid;
            rowOfKey.insert(key, int(rows.size()));
            rows.append(row);
        }
        members[key].append(&message);
    }
    for (Row &row : rows) {
        const QList<const MessageSummary *> &list = members.value(row.key);
        row.count = int(list.size());
        QStringList names;
        QSet<QString> seen;
        // Senders oldest first: the list is newest first, so walk it backwards.
        for (auto it = list.crbegin(); it != list.crend(); ++it) {
            const MessageSummary &message = **it;
            row.unread = row.unread || message.unread;
            row.flagged = row.flagged || message.flagged;
            const QString address = participantAddress(message.from);
            if (seen.contains(address)) {
                continue;
            }
            seen.insert(address);
            names.append(address == message.account.toLower() ? i18nc("@item the user themselves in a list of senders", "me") : participantName(message.from));
        }
        for (const MessageSummary *message : list) {
            row.uids.append(message->uid);
        }
        row.participants = names.join(i18nc("@item separator between senders", ", "));
    }
    return rows;
}

const MessageSummary &MessageListModel::representative(const Row &row) const
{
    for (const MessageSummary &message : m_messages) {
        if (message.uid == row.uid && message.account == row.account) {
            return message;
        }
    }
    static const MessageSummary none;
    return none;
}

void MessageListModel::applyRows(const QList<Row> &rows)
{
    if (rows.size() == m_rows.size()) {
        bool sameKeys = true;
        for (qsizetype i = 0; i < rows.size() && sameKeys; ++i) {
            sameKeys = rows.at(i).key == m_rows.at(i).key;
        }
        if (sameKeys) {
            const QList<Row> old = m_rows;
            m_rows = rows;
            for (qsizetype i = 0; i < rows.size(); ++i) {
                if (!(rows.at(i) == old.at(i))) {
                    const QModelIndex idx = index(int(i));
                    Q_EMIT dataChanged(idx, idx);
                }
            }
            return;
        }
    } else if (rows.size() + 1 == m_rows.size()) {
        qsizetype gone = m_rows.size() - 1;
        for (qsizetype i = 0; i < rows.size(); ++i) {
            if (rows.at(i).key != m_rows.at(i).key) {
                gone = i;
                break;
            }
        }
        QList<Row> without = m_rows;
        without.removeAt(gone);
        bool same = without.size() == rows.size();
        for (qsizetype i = 0; i < rows.size() && same; ++i) {
            same = without.at(i).key == rows.at(i).key;
        }
        if (same) {
            beginRemoveRows({}, int(gone), int(gone));
            m_rows = rows;
            endRemoveRows();
            // The survivors may have changed too (a conversation lost its newest message).
            for (qsizetype i = 0; i < rows.size(); ++i) {
                if (!(rows.at(i) == without.at(i))) {
                    const QModelIndex idx = index(int(i));
                    Q_EMIT dataChanged(idx, idx);
                }
            }
            return;
        }
    }
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

void MessageListModel::setConversationView(bool on)
{
    if (m_conversationView == on) {
        return;
    }
    beginResetModel();
    m_conversationView = on;
    m_rows = on ? buildRows() : QList<Row>();
    endResetModel();
}

bool MessageListModel::conversationView() const
{
    return m_conversationView;
}

QList<MessageSummary> MessageListModel::allMessages() const
{
    return m_messages;
}

QList<MessageSummary> MessageListModel::conversationOf(const QString &account, qint64 uid) const
{
    if (!m_conversationView) {
        const MessageSummary found = message(account, uid);
        return found.uid == uid ? QList<MessageSummary>{found} : QList<MessageSummary>();
    }
    for (qsizetype row = 0; row < m_rows.size(); ++row) {
        if (m_rows.at(row).account == account && m_rows.at(row).uids.contains(uid)) {
            return conversationAt(int(row));
        }
    }
    return {};
}

QList<MessageSummary> MessageListModel::conversationAt(int row) const
{
    if (!m_conversationView) {
        return row >= 0 && row < m_messages.size() ? QList<MessageSummary>{m_messages.at(row)} : QList<MessageSummary>();
    }
    if (row < 0 || row >= m_rows.size()) {
        return {};
    }
    QList<MessageSummary> result;
    const Row &conversation = m_rows.at(row);
    for (const MessageSummary &message : m_messages) {
        if (message.account == conversation.account && conversation.uids.contains(message.uid)) {
            result.append(message);
        }
    }
    return result;
}

void MessageListModel::setMessages(const QList<MessageSummary> &messages)
{
    beginResetModel();
    m_messages = messages;
    if (m_conversationView) {
        m_rows = buildRows();
    }
    endResetModel();
}

void MessageListModel::clear()
{
    setMessages({});
}

void MessageListModel::setUnread(const QString &account, qint64 uid, bool unread)
{
    for (qsizetype row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].uid != uid || m_messages[row].account != account) {
            continue;
        }
        if (m_messages[row].unread != unread) {
            m_messages[row].unread = unread;
            if (m_conversationView) {
                applyRows(buildRows());
                return;
            }
            const QModelIndex idx = index(int(row));
            Q_EMIT dataChanged(idx, idx, {UnreadRole});
        }
        return;
    }
}

void MessageListModel::setFlagged(const QString &account, qint64 uid, bool flagged)
{
    for (qsizetype row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].uid != uid || m_messages[row].account != account) {
            continue;
        }
        if (m_messages[row].flagged != flagged) {
            m_messages[row].flagged = flagged;
            if (m_conversationView) {
                applyRows(buildRows());
                return;
            }
            const QModelIndex idx = index(int(row));
            Q_EMIT dataChanged(idx, idx, {FlaggedRole});
        }
        return;
    }
}

int MessageListModel::removeMessage(const QString &account, qint64 uid)
{
    for (qsizetype row = 0; row < m_messages.size(); ++row) {
        if (m_messages[row].uid == uid && m_messages[row].account == account) {
            if (m_conversationView) {
                // The row of the conversation (it goes only when this was its last message).
                int shown = -1;
                for (qsizetype i = 0; i < m_rows.size(); ++i) {
                    if (m_rows.at(i).account == account && m_rows.at(i).uids.contains(uid)) {
                        shown = int(i);
                        break;
                    }
                }
                m_messages.removeAt(row);
                applyRows(buildRows());
                return shown;
            }
            beginRemoveRows({}, int(row), int(row));
            m_messages.removeAt(row);
            endRemoveRows();
            return int(row);
        }
    }
    return -1;
}

MessageSummary MessageListModel::messageAt(int row) const
{
    if (m_conversationView) {
        return row >= 0 && row < m_rows.size() ? representative(m_rows.at(row)) : MessageSummary();
    }
    return row >= 0 && row < m_messages.size() ? m_messages.at(row) : MessageSummary();
}

MessageSummary MessageListModel::message(const QString &account, qint64 uid) const
{
    for (const MessageSummary &msg : m_messages) {
        if (msg.uid == uid && msg.account == account) {
            return msg;
        }
    }
    return {};
}
