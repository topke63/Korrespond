// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagestore.h"

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
static constexpr int schemaVersion = 1;

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
        "size INTEGER NOT NULL, path TEXT NOT NULL, "
        "PRIMARY KEY(folder, uid))"_s)
        && query.exec(u"CREATE INDEX IF NOT EXISTS messages_date ON messages(folder, date)"_s)
        && query.exec(u"PRAGMA user_version = %1"_s.arg(schemaVersion));
    if (!ok) {
        m_lastError = query.lastError().text();
        db.close();
        return false;
    }
    return true;
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
        u"INSERT OR REPLACE INTO messages (folder, uidvalidity, uid, subject, sender, date, unread, flagged, size, path) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"_s);
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
        u"SELECT uid, subject, sender, date, unread, flagged FROM messages WHERE folder = ? "
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
        result.append(summary);
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
