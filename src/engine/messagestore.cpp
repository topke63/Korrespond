// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagestore.h"

#include "messagerenderer.h"

#include <KMime/Message>

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimeZone>
#include <QUuid>

using namespace Qt::StringLiterals;

// Bump when the schema changes; open() can then migrate older databases.
// 2: messages.preview. 3: previews without image placeholders. Every bump recomputes the
// previews, so a better messagePreview() reaches messages stored before it.
static constexpr int schemaVersion = 3;

MessageStore::MessageStore(const QString &baseDir)
    : m_baseDir(baseDir)
    // Every instance gets its own connection, so two stores (e.g. in tests) never share one.
    , m_connectionName(u"plasmamail-store-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

MessageStore::~MessageStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        // The database handle must be gone before the connection is removed.
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool MessageStore::open()
{
    if (isOpen()) {
        return true;
    }

    if (!QDir().mkpath(m_baseDir + u"/messages"_s)) {
        m_lastError = u"Cannot create %1"_s.arg(m_baseDir + u"/messages"_s);
        return false;
    }

    QSqlDatabase db = QSqlDatabase::contains(m_connectionName) ? QSqlDatabase::database(m_connectionName, false)
                                                               : QSqlDatabase::addDatabase(u"QSQLITE"_s, m_connectionName);
    db.setDatabaseName(m_baseDir + u"/mail.sqlite"_s);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    // date is milliseconds since the epoch (UTC), NULL when unknown, so it sorts correctly.
    // path is relative to baseDir, so the data directory can move.
    const bool ok = query.exec(
        u"CREATE TABLE IF NOT EXISTS messages ("
        "folder TEXT NOT NULL, uidvalidity INTEGER NOT NULL, uid INTEGER NOT NULL, "
        "subject TEXT, sender TEXT, date INTEGER, unread INTEGER NOT NULL, flagged INTEGER NOT NULL DEFAULT 0, "
        "size INTEGER NOT NULL, path TEXT NOT NULL, preview TEXT, "
        "PRIMARY KEY(folder, uid))"_s)
        && query.exec(u"CREATE INDEX IF NOT EXISTS messages_date ON messages(folder, date)"_s)
        // position keeps the server list's order (special folders first).
        && query.exec(
            u"CREATE TABLE IF NOT EXISTS folders ("
            "position INTEGER PRIMARY KEY, path TEXT NOT NULL, name TEXT NOT NULL, role TEXT NOT NULL, depth INTEGER NOT NULL)"_s)
        && updatePreviews()
        && query.exec(u"PRAGMA user_version = %1"_s.arg(schemaVersion));
    if (!ok) {
        if (query.lastError().isValid()) { // else updatePreviews() set m_lastError
            m_lastError = query.lastError().text();
        }
        db.close();
        return false;
    }
    return true;
}

bool MessageStore::updatePreviews()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    if (!query.exec(u"PRAGMA user_version"_s) || !query.next()) {
        m_lastError = query.lastError().text();
        return false;
    }
    const bool outdated = query.value(0).toInt() < schemaVersion;
    if (!query.exec(u"PRAGMA table_info(messages)"_s)) {
        m_lastError = query.lastError().text();
        return false;
    }
    bool hasPreview = false;
    while (query.next()) {
        hasPreview = hasPreview || query.value(1).toString() == u"preview"_s;
    }
    if (!hasPreview && !query.exec(u"ALTER TABLE messages ADD COLUMN preview TEXT"_s)) {
        m_lastError = query.lastError().text();
        return false;
    }

    // NULL = not computed yet ("" means computed, no text).
    if (outdated && !query.exec(u"UPDATE messages SET preview = NULL"_s)) {
        m_lastError = query.lastError().text();
        return false;
    }
    if (!query.exec(u"SELECT folder, uid, path FROM messages WHERE preview IS NULL"_s)) {
        m_lastError = query.lastError().text();
        return false;
    }
    QList<std::tuple<QString, qint64, QString>> previews;
    while (query.next()) {
        QFile file(m_baseDir + u'/' + query.value(2).toString());
        const QByteArray raw = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        previews.append({query.value(0).toString(), query.value(1).toLongLong(), previewOf(raw)});
    }
    if (previews.isEmpty()) {
        return true;
    }
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    query.prepare(u"UPDATE messages SET preview = ? WHERE folder = ? AND uid = ?"_s);
    for (const auto &[folder, uid, preview] : std::as_const(previews)) {
        query.addBindValue(preview);
        query.addBindValue(folder);
        query.addBindValue(uid);
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            db.rollback();
            return false;
        }
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

QString MessageStore::previewOf(const QByteArray &raw)
{
    if (raw.isEmpty()) {
        return u""_s; // not NULL: computed, nothing to show
    }
    // Parsed like KIMAP does with fetched messages.
    KMime::Message message;
    message.setContent(KMime::CRLFtoLF(raw));
    message.parse();
    const QString preview = messagePreview(message);
    return preview.isNull() ? u""_s : preview;
}

bool MessageStore::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) && QSqlDatabase::database(m_connectionName, false).isOpen();
}

bool MessageStore::addMessage(const QString &folder, qint64 uidValidity, const MessageSummary &summary, const QByteArray &raw)
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return false;
    }

    const QString relPath = relativePath(folder, uidValidity, summary.uid);
    const QString absPath = m_baseDir + u'/' + relPath;
    const QString oldRelPath = storedPath(folder, summary.uid);

    if (!QDir().mkpath(QFileInfo(absPath).absolutePath())) {
        m_lastError = u"Cannot create the folder for %1"_s.arg(absPath);
        return false;
    }
    // QSaveFile writes to a temporary file and renames it, so a crash never leaves half a
    // message behind under the real name.
    QSaveFile file(absPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(raw) != raw.size() || !file.commit()) {
        m_lastError = file.errorString();
        return false;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(
        u"INSERT OR REPLACE INTO messages (folder, uidvalidity, uid, subject, sender, date, unread, flagged, size, path, preview) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
    query.addBindValue(folder);
    query.addBindValue(uidValidity);
    query.addBindValue(summary.uid);
    query.addBindValue(summary.subject);
    query.addBindValue(summary.from);
    query.addBindValue(summary.date.isValid() ? QVariant(summary.date.toMSecsSinceEpoch()) : QVariant(QMetaType::fromType<qint64>()));
    query.addBindValue(summary.unread);
    query.addBindValue(summary.flagged);
    query.addBindValue(qint64(raw.size()));
    query.addBindValue(relPath);
    query.addBindValue(previewOf(raw));
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        // The file only belongs to us if no row pointed at it before.
        if (oldRelPath != relPath) {
            QFile::remove(absPath);
        }
        return false;
    }

    // Same uid under an older UIDVALIDITY: the previous file is no longer referenced.
    if (!oldRelPath.isEmpty() && oldRelPath != relPath) {
        QFile::remove(m_baseDir + u'/' + oldRelPath);
    }
    return true;
}

bool MessageStore::contains(const QString &folder, qint64 uid) const
{
    return !storedPath(folder, uid).isEmpty();
}

QList<MessageSummary> MessageStore::messages(const QString &folder, int limit) const
{
    QList<MessageSummary> result;
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return result;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    // SQLite treats LIMIT -1 as "no limit". uid breaks ties, so equal dates keep a stable order.
    query.prepare(
        u"SELECT uid, subject, sender, date, unread, flagged, preview FROM messages WHERE folder = ? "
        "ORDER BY date IS NULL, date DESC, uid DESC LIMIT ?"_s);
    query.addBindValue(folder);
    query.addBindValue(limit < 0 ? -1 : limit);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        MessageSummary summary;
        summary.uid = query.value(0).toLongLong();
        summary.subject = query.value(1).toString();
        summary.from = query.value(2).toString();
        if (!query.value(3).isNull()) {
            summary.date = QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong(), QTimeZone::UTC);
        }
        summary.unread = query.value(4).toBool();
        summary.flagged = query.value(5).toBool();
        summary.preview = query.value(6).toString();
        result.append(summary);
    }
    return result;
}

QHash<qint64, QString> MessageStore::previews(const QString &folder) const
{
    QHash<qint64, QString> result;
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return result;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT uid, preview FROM messages WHERE folder = ? AND preview <> ''"_s);
    query.addBindValue(folder);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        result.insert(query.value(0).toLongLong(), query.value(1).toString());
    }
    return result;
}

QByteArray MessageStore::rawMessage(const QString &folder, qint64 uid) const
{
    const QString relPath = storedPath(folder, uid);
    if (relPath.isEmpty()) {
        return {};
    }
    QFile file(m_baseDir + u'/' + relPath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = file.errorString();
        return {};
    }
    return file.readAll();
}

bool MessageStore::setUnread(const QString &folder, qint64 uid, bool unread)
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"UPDATE messages SET unread = ? WHERE folder = ? AND uid = ?"_s);
    query.addBindValue(unread);
    query.addBindValue(folder);
    query.addBindValue(uid);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() != 1) {
        m_lastError = u"Message %1 in %2 is not stored"_s.arg(uid).arg(folder);
        return false;
    }
    return true;
}

qint64 MessageStore::totalSize() const
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return 0;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(u"SELECT COALESCE(SUM(size), 0) FROM messages"_s) || !query.next()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.value(0).toLongLong();
}

int MessageStore::removeOlderThan(const QDateTime &cutoff)
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return -1;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT folder, uid, path FROM messages WHERE date < ?"_s);
    query.addBindValue(cutoff.toMSecsSinceEpoch());
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return -1;
    }
    QList<StoredFile> files;
    while (query.next()) {
        files.append({query.value(0).toString(), query.value(1).toLongLong(), query.value(2).toString()});
    }
    return removeStored(files);
}

int MessageStore::removeOldestUntil(qint64 maxBytes)
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return -1;
    }
    qint64 total = totalSize();
    if (total <= maxBytes) {
        return 0;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(u"SELECT folder, uid, path, size FROM messages ORDER BY date IS NULL, date, uid"_s)) {
        m_lastError = query.lastError().text();
        return -1;
    }
    QList<StoredFile> files;
    while (total > maxBytes && query.next()) {
        files.append({query.value(0).toString(), query.value(1).toLongLong(), query.value(2).toString()});
        total -= query.value(3).toLongLong();
    }
    return removeStored(files);
}

int MessageStore::removeFolder(const QString &folder)
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return -1;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT uid, path FROM messages WHERE folder = ?"_s);
    query.addBindValue(folder);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return -1;
    }
    QList<StoredFile> files;
    while (query.next()) {
        files.append({folder, query.value(0).toLongLong(), query.value(1).toString()});
    }
    return removeStored(files);
}

qint64 MessageStore::storedUidValidity(const QString &folder) const
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return 0;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    // addMessage() replaces by uid only, so a folder could briefly mix values; the newest wins.
    query.prepare(u"SELECT COALESCE(MAX(uidvalidity), 0) FROM messages WHERE folder = ?"_s);
    query.addBindValue(folder);
    if (!query.exec() || !query.next()) {
        m_lastError = query.lastError().text();
        return 0;
    }
    return query.value(0).toLongLong();
}

bool MessageStore::clear()
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(u"DELETE FROM messages"_s) || !query.exec(u"DELETE FROM folders"_s)) {
        m_lastError = query.lastError().text();
        return false;
    }
    const QString messagesDir = m_baseDir + u"/messages"_s;
    if (!QDir(messagesDir).removeRecursively() || !QDir().mkpath(messagesDir)) {
        m_lastError = u"Cannot empty %1"_s.arg(messagesDir);
        return false;
    }
    return true;
}

bool MessageStore::setFolders(const QList<FolderInfo> &folders)
{
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    QSqlQuery query(db);
    bool ok = query.exec(u"DELETE FROM folders"_s);
    query.prepare(u"INSERT INTO folders (position, path, name, role, depth) VALUES (?, ?, ?, ?, ?)"_s);
    for (qsizetype i = 0; ok && i < folders.size(); ++i) {
        const FolderInfo &folder = folders.at(i);
        query.addBindValue(qint64(i));
        query.addBindValue(folder.path);
        // A null QString binds as NULL, which the NOT NULL columns refuse.
        query.addBindValue(folder.name.isNull() ? u""_s : folder.name);
        query.addBindValue(folder.role.isNull() ? u""_s : folder.role);
        query.addBindValue(folder.depth);
        ok = query.exec();
    }
    if (!ok || !db.commit()) {
        m_lastError = ok ? db.lastError().text() : query.lastError().text();
        db.rollback();
        return false;
    }
    return true;
}

QList<FolderInfo> MessageStore::folders() const
{
    QList<FolderInfo> result;
    if (!isOpen()) {
        m_lastError = u"Message store is not open"_s;
        return result;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(u"SELECT path, name, role, depth FROM folders ORDER BY position"_s)) {
        m_lastError = query.lastError().text();
        return result;
    }
    while (query.next()) {
        FolderInfo folder;
        folder.path = query.value(0).toString();
        folder.name = query.value(1).toString();
        folder.role = query.value(2).toString();
        folder.depth = query.value(3).toInt();
        result.append(folder);
    }
    return result;
}

int MessageStore::removeStored(const QList<StoredFile> &files)
{
    if (files.isEmpty()) {
        return 0;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return -1;
    }
    QSqlQuery query(db);
    query.prepare(u"DELETE FROM messages WHERE folder = ? AND uid = ?"_s);
    for (const StoredFile &file : files) {
        query.addBindValue(file.folder);
        query.addBindValue(file.uid);
        if (!query.exec()) {
            m_lastError = query.lastError().text();
            db.rollback();
            return -1;
        }
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        db.rollback();
        return -1;
    }
    // Rows go first: a file left behind by a failed remove is only wasted space, while a row
    // without its file would show a message that can't be opened.
    for (const StoredFile &file : files) {
        const QString absPath = m_baseDir + u'/' + file.path;
        QFile::remove(absPath);
        QDir().rmdir(QFileInfo(absPath).absolutePath()); // only succeeds once the folder is empty
    }
    return int(files.size());
}

QString MessageStore::lastError() const
{
    return m_lastError;
}

QString MessageStore::relativePath(const QString &folder, qint64 uidValidity, qint64 uid)
{
    const QByteArray folderHash = QCryptographicHash::hash(folder.toUtf8(), QCryptographicHash::Sha1).toHex();
    return u"messages/%1/%2-%3.eml"_s.arg(QString::fromLatin1(folderHash)).arg(uidValidity).arg(uid);
}

QString MessageStore::storedPath(const QString &folder, qint64 uid) const
{
    if (!isOpen()) {
        return {};
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT path FROM messages WHERE folder = ? AND uid = ?"_s);
    query.addBindValue(folder);
    query.addBindValue(uid);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return {};
    }
    return query.next() ? query.value(0).toString() : QString();
}
