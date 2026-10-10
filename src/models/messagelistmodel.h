// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "imapclient.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>

#include <functional>

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
        // Conversation rows (T221). In the flat view each message is its own conversation of one.
        ThreadIdRole, // "<account>\n<thread id>"
        ThreadCountRole,
        ParticipantsRole, // "Alice, Bob, me": the senders, oldest first, own address as "me"
        ThreadUnreadRole, // any message of the conversation is unread
        ThreadFlaggedRole, // any message is flagged
        ThreadUidsRole, // the conversation's uids (same account as the row), in list order
    };

    explicit MessageListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(const QList<MessageSummary> &messages);
    void clear();
    // One row per conversation (its newest message stands for it) instead of one per message.
    // Lookups by account and uid (message(), setUnread() …) always see every message.
    void setConversationView(bool on);
    [[nodiscard]] bool conversationView() const;
    // Every message the model holds, in list order (also those hidden behind a conversation row).
    [[nodiscard]] QList<MessageSummary> allMessages() const;
    // The messages of the conversation that row `row` shows, newest first; the one message of the
    // row in the flat view, empty for a row that doesn't exist.
    [[nodiscard]] QList<MessageSummary> conversationAt(int row) const;
    // The conversation that holds this message, newest first (just the message in the flat view);
    // empty when the message is not in the list.
    [[nodiscard]] QList<MessageSummary> conversationOf(const QString &account, qint64 uid) const;

    // "Alice Example <alice@example.org>" → "Alice"; an address alone → its local part; "Doe, John"
    // → "John". `own` (the account's address) → empty; the caller shows "me".
    [[nodiscard]] static QString participantName(const QString &from);
    [[nodiscard]] static QString participantAddress(const QString &from);
    // A uid is only unique within one account, so every lookup names the account.
    // Updates one message's unread state; emits dataChanged for that row only.
    void setUnread(const QString &account, qint64 uid, bool unread);
    void setFlagged(const QString &account, qint64 uid, bool flagged);
    // Removes one message from the list. Returns the row it had, or -1 if it was not there.
    int removeMessage(const QString &account, qint64 uid);
    // The message in `row`; a default MessageSummary (uid 0) for a row that doesn't exist.
    [[nodiscard]] MessageSummary messageAt(int row) const;
    // Returns a default MessageSummary (uid 0) if the message is not in the list.
    [[nodiscard]] MessageSummary message(const QString &account, qint64 uid) const;

    // Looks up a name for an address (contacts); used for senders whose header has no display
    // name. The answers are cached until namesChanged().
    void setNameResolver(const std::function<QString(const QString &address)> &resolver);
    // The resolver would answer differently now (the contacts changed): rows are refreshed.
    void namesChanged();
    // The sender as shown: the header's own text, or "Name <address>" when it had no name and the
    // resolver knows one.
    [[nodiscard]] QString displayFrom(const MessageSummary &message) const;

    // Short local-time text: time of day for today, short date otherwise.
    [[nodiscard]] static QString dateText(const QDateTime &dateTime, const QDate &today);

private:
    struct Row {
        QString key; // account + thread id
        QString account;
        qint64 uid = 0; // the representative: the first (newest) message of the conversation
        int count = 0;
        bool unread = false;
        bool flagged = false;
        QList<qint64> uids;
        QString participants;

        bool operator==(const Row &) const = default;
    };

    [[nodiscard]] QList<Row> buildRows() const;
    // Makes `rows` the visible rows, telling views the smallest change (a row changed, one row
    // removed, else a reset).
    void applyRows(const QList<Row> &rows);
    [[nodiscard]] const MessageSummary &representative(const Row &row) const;

    QList<MessageSummary> m_messages;
    bool m_conversationView = false;
    QList<Row> m_rows; // only in the conversation view
    std::function<QString(const QString &)> m_resolver;
    mutable QHash<QString, QString> m_names; // address -> resolved name ("" = none)
};
