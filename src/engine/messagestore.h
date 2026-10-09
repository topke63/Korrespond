// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "folderutils.h"
#include "imapclient.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
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
    // uid → preview text (see messagePreview()) of the stored messages of folder.
    [[nodiscard]] QHash<qint64, QString> previews(const QString &folder) const;
    // Updates the local read state, e.g. after the message was marked seen on the server.
    // Returns false when the message isn't stored.
    bool setUnread(const QString &folder, qint64 uid, bool unread);

    // Sum of the stored .eml sizes in bytes, over all folders.
    [[nodiscard]] qint64 totalSize() const;
    // The remove functions delete index rows and their .eml files together and return how many
    // messages were removed, or -1 on error (see lastError()).
    // Removes messages dated before cutoff. Messages without a date are kept.
    int removeOlderThan(const QDateTime &cutoff);
    // Removes the oldest messages until totalSize() <= maxBytes. Undated ones go last: they are
    // usually sent mail without a Date header (see messageDate()), not old mail.
    int removeOldestUntil(qint64 maxBytes);
    // Removes every message of folder, e.g. after the server changed its UIDVALIDITY.
    int removeFolder(const QString &folder);
    // The UIDVALIDITY folder's messages were stored under; 0 when none are stored (servers
    // never use 0).
    [[nodiscard]] qint64 storedUidValidity(const QString &folder) const;
    // Removes every message, including stray .eml files no row points at, and the folder list.
    bool clear();

    // The server's folder list, kept so the stored mail can be shown before connecting
    // (e.g. offline). setFolders() replaces the previous list; folders() keeps its order.
    bool setFolders(const QList<FolderInfo> &folders);
    [[nodiscard]] QList<FolderInfo> folders() const;

    // Each account has its own store: <appDataDir>/accounts/<sha1 hex of the lower-cased email>.
    [[nodiscard]] static QString accountDir(const QString &appDataDir, const QString &email);
    // Moves the single store of milestone 1 (mail.sqlite and messages/ directly in appDataDir)
    // into accountDir. Nothing is moved when there is no old store or accountDir already has
    // one. Returns true when there was nothing to do or the move worked, false when a move
    // failed (then the old files are put back where possible).
    static bool moveLegacyStore(const QString &appDataDir, const QString &accountDir);

    // Human-readable reason for the last failed call (for logs, not shown in the UI).
    [[nodiscard]] QString lastError() const;

private:
    // Path of a message file relative to baseDir. Folder names are hashed, so any IMAP
    // folder name (slashes, non-ASCII) gives a valid file name.
    [[nodiscard]] static QString relativePath(const QString &folder, qint64 uidValidity, qint64 uid);
    [[nodiscard]] QString storedPath(const QString &folder, qint64 uid) const;

    struct StoredFile {
        QString folder;
        qint64 uid = 0;
        QString path;
    };
    // Deletes the rows in one transaction, then their files. Returns the count or -1.
    int removeStored(const QList<StoredFile> &files);
    // Adds the preview column to a version 1 database and computes missing previews from the
    // .eml files; all of them when the database is older than this code's schema version.
    bool updatePreviews();
    [[nodiscard]] static QString previewOf(const QByteArray &raw);

    QString m_baseDir;
    QString m_connectionName;
    mutable QString m_lastError;
};
