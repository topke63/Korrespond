// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "imapclient.h"

#include <QByteArray>
#include <QList>
#include <QString>

// Local copies of messages: an SQLite index (mail.sqlite) plus one raw .eml file per message
// under messages/, both inside baseDir. Only ever touches local data, never the server.
// Not thread-safe; use one instance per thread.
class MessageStore
{
public:
    // The app passes QStandardPaths::AppDataLocation; tests pass a temporary directory.
    explicit MessageStore(const QString &baseDir);
    ~MessageStore();

    MessageStore(const MessageStore &) = delete;
    MessageStore &operator=(const MessageStore &) = delete;

    // Creates baseDir, the messages directory and the schema if needed. Safe to call again.
    bool open();
    [[nodiscard]] bool isOpen() const;

    // Writes raw (the whole message, attachments included) to its .eml file, then adds or
    // replaces the index row for (folder, uid). On failure nothing is changed.
    bool addMessage(const QString &folder, qint64 uidValidity, const MessageSummary &summary, const QByteArray &raw);
    [[nodiscard]] bool contains(const QString &folder, qint64 uid) const;

    // Up to limit stored messages of folder, newest date first; messages without a date come
    // last. limit < 0 means all of them.
    [[nodiscard]] QList<MessageSummary> messages(const QString &folder, int limit) const;
    // The stored .eml bytes; empty when the message isn't stored or its file can't be read.
    [[nodiscard]] QByteArray rawMessage(const QString &folder, qint64 uid) const;
    // Updates the local read state, e.g. after the message was marked seen on the server.
    // Returns false when the message isn't stored.
    bool setUnread(const QString &folder, qint64 uid, bool unread);

    // Human-readable reason for the last failed call (for logs, not shown in the UI).
    [[nodiscard]] QString lastError() const;

private:
    // Path of a message file relative to baseDir. Folder names are hashed, so any IMAP
    // folder name (slashes, non-ASCII) gives a valid file name.
    [[nodiscard]] static QString relativePath(const QString &folder, qint64 uidValidity, qint64 uid);
    [[nodiscard]] QString storedPath(const QString &folder, qint64 uid) const;

    QString m_baseDir;
    QString m_connectionName;
    mutable QString m_lastError;
};
