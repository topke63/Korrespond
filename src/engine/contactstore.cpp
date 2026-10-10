// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "contactstore.h"

#include <QDir>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace
{
// A null QString binds as NULL, which the NOT NULL columns refuse.
QString nonNull(const QString &text)
{
    return text.isNull() ? QStringLiteral("") : text;
}

QString escapeLike(const QString &text)
{
    QString result;
    for (const QChar ch : text) {
        if (ch == u'%' || ch == u'_' || ch == u'\\') {
            result += u'\\';
        }
        result += ch;
    }
    return result;
}
}

ContactStore::ContactStore(const QString &baseDir)
    : m_baseDir(baseDir)
    , m_connectionName(u"plasmamail-contacts-"_s + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

ContactStore::~ContactStore()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool ContactStore::open()
{
    if (isOpen()) {
        return true;
    }
    if (!QDir().mkpath(m_baseDir)) {
        m_lastError = u"Cannot create %1"_s.arg(m_baseDir);
        return false;
    }
    QSqlDatabase db =
        QSqlDatabase::contains(m_connectionName) ? QSqlDatabase::database(m_connectionName, false) : QSqlDatabase::addDatabase(u"QSQLITE"_s, m_connectionName);
    db.setDatabaseName(m_baseDir + u"/contacts.sqlite"_s);
    if (!db.open()) {
        m_lastError = db.lastError().text();
        return false;
    }
    QSqlQuery query(db);
    // email_key is the lower-cased address; (account, source, resource, email_key) is one row.
    const bool ok = query.exec(
                        u"CREATE TABLE IF NOT EXISTS contacts ("
                        "account TEXT NOT NULL, source INTEGER NOT NULL, resource TEXT NOT NULL, email TEXT NOT NULL, email_key TEXT NOT NULL, "
                        "name TEXT NOT NULL DEFAULT '', photo TEXT NOT NULL DEFAULT '', last_used INTEGER NOT NULL DEFAULT 0, "
                        "pos INTEGER NOT NULL DEFAULT 0, "
                        "PRIMARY KEY(account, source, resource, email_key))"_s)
        && query.exec(u"CREATE INDEX IF NOT EXISTS contacts_email ON contacts(email_key)"_s)
        && query.exec(
            u"CREATE TABLE IF NOT EXISTS sync_state (account TEXT NOT NULL, source INTEGER NOT NULL, token TEXT NOT NULL, "
            "PRIMARY KEY(account, source))"_s)
        && query.exec(u"PRAGMA user_version = 1"_s);
    if (!ok) {
        m_lastError = query.lastError().text();
        db.close();
        return false;
    }
    return true;
}

bool ContactStore::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName) && QSqlDatabase::database(m_connectionName, false).isOpen();
}

QString ContactStore::lastError() const
{
    return m_lastError;
}

bool ContactStore::upsertPerson(const QString &account,
                                Source source,
                                const QString &resource,
                                const QString &name,
                                const QStringList &emails,
                                const QString &photoUrl)
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    if (!db.transaction()) {
        m_lastError = db.lastError().text();
        return false;
    }
    QSqlQuery query(db);
    query.prepare(u"DELETE FROM contacts WHERE account = ? AND source = ? AND resource = ?"_s);
    query.addBindValue(account);
    query.addBindValue(int(source));
    query.addBindValue(resource);
    bool ok = query.exec();
    int position = 0;
    for (const QString &email : emails) {
        if (!ok) {
            break;
        }
        const QString address = email.trimmed();
        if (address.isEmpty()) {
            continue;
        }
        query.prepare(u"INSERT OR REPLACE INTO contacts (account, source, resource, email, email_key, name, photo, pos) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"_s);
        query.addBindValue(account);
        query.addBindValue(int(source));
        query.addBindValue(resource);
        query.addBindValue(address);
        query.addBindValue(address.toLower());
        query.addBindValue(nonNull(name.trimmed()));
        query.addBindValue(nonNull(photoUrl));
        query.addBindValue(position++);
        ok = query.exec();
    }
    if (!ok) {
        m_lastError = query.lastError().text();
        db.rollback();
        return false;
    }
    if (!db.commit()) {
        m_lastError = db.lastError().text();
        return false;
    }
    return true;
}

bool ContactStore::removePerson(const QString &account, Source source, const QString &resource)
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"DELETE FROM contacts WHERE account = ? AND source = ? AND resource = ?"_s);
    query.addBindValue(account);
    query.addBindValue(int(source));
    query.addBindValue(resource);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ContactStore::addHistory(const QString &account, const QString &email, const QString &name, qint64 seenAtMs)
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    const QString address = email.trimmed();
    if (address.isEmpty() || !address.contains(u'@')) {
        return true; // nothing to remember
    }
    const QString key = address.toLower();
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    // A newer sighting updates the time, and the name when it has one; an older one changes neither
    // (the list is stored newest first, so most sightings are older than the first).
    query.prepare(
        u"INSERT INTO contacts (account, source, resource, email, email_key, name, last_used) VALUES (?, 2, ?, ?, ?, ?, ?) "
        "ON CONFLICT(account, source, resource, email_key) DO UPDATE SET "
        "name = CASE WHEN excluded.last_used >= last_used AND excluded.name <> '' THEN excluded.name ELSE name END, "
        "last_used = MAX(last_used, excluded.last_used)"_s);
    query.addBindValue(account);
    query.addBindValue(key); // the resource of a history entry is its address
    query.addBindValue(address);
    query.addBindValue(key);
    query.addBindValue(nonNull(name.trimmed()));
    query.addBindValue(seenAtMs);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

bool ContactStore::clearSource(const QString &account, Source source)
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
    QSqlQuery query(db);
    query.prepare(u"DELETE FROM contacts WHERE account = ? AND source = ?"_s);
    query.addBindValue(account);
    query.addBindValue(int(source));
    bool ok = query.exec();
    if (ok) {
        query.prepare(u"DELETE FROM sync_state WHERE account = ? AND source = ?"_s);
        query.addBindValue(account);
        query.addBindValue(int(source));
        ok = query.exec();
    }
    if (!ok) {
        m_lastError = query.lastError().text();
    }
    return ok;
}

bool ContactStore::clearAccount(const QString &account)
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"DELETE FROM contacts WHERE account = ?"_s);
    query.addBindValue(account);
    bool ok = query.exec();
    if (ok) {
        query.prepare(u"DELETE FROM sync_state WHERE account = ?"_s);
        query.addBindValue(account);
        ok = query.exec();
    }
    if (!ok) {
        m_lastError = query.lastError().text();
    }
    return ok;
}

bool ContactStore::clearAll()
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (!query.exec(u"DELETE FROM contacts"_s) || !query.exec(u"DELETE FROM sync_state"_s)) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}

QList<ContactStore::Suggestion> ContactStore::suggest(const QString &prefix, int limit) const
{
    QList<Suggestion> result;
    const QString needle = prefix.trimmed().toLower();
    if (!isOpen() || needle.isEmpty() || limit <= 0) {
        return result;
    }
    const QString like = escapeLike(needle);
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    // A name word or the address starts with the prefix. LIKE is case-insensitive for ASCII; the
    // names are compared lower-cased in C++ too, so other scripts match as typed.
    query.prepare(
        u"SELECT email, email_key, name, source, last_used FROM contacts "
        "WHERE email_key LIKE ? ESCAPE '\\' OR name LIKE ? ESCAPE '\\' OR name LIKE ? ESCAPE '\\' "
        "ORDER BY source, last_used DESC, pos, name COLLATE NOCASE, email_key LIMIT 400"_s);
    query.addBindValue(QString(like + u'%'));
    query.addBindValue(QString(like + u'%'));
    query.addBindValue(QString(u"% "_s + like + u'%')); // a later word of the name
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return result;
    }
    QSet<QString> seen;
    while (query.next() && result.size() < limit) {
        const QString key = query.value(1).toString();
        if (seen.contains(key)) {
            continue; // the same address from a worse source or an older sighting
        }
        seen.insert(key);
        Suggestion suggestion;
        suggestion.email = query.value(0).toString();
        suggestion.name = query.value(2).toString();
        suggestion.source = query.value(3).toInt();
        result.append(suggestion);
    }
    // A better source may have the address without a name: borrow the name of any row.
    for (Suggestion &suggestion : result) {
        if (suggestion.name.isEmpty()) {
            suggestion.name = nameFor(suggestion.email);
        }
    }
    return result;
}

QString ContactStore::nameFor(const QString &email) const
{
    if (!isOpen()) {
        return {};
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT name FROM contacts WHERE email_key = ? AND name <> '' ORDER BY source, last_used DESC LIMIT 1"_s);
    query.addBindValue(email.trimmed().toLower());
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return {};
}

int ContactStore::count(Source source) const
{
    if (!isOpen()) {
        return 0;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT COUNT(DISTINCT email_key) FROM contacts WHERE source = ?"_s);
    query.addBindValue(int(source));
    return query.exec() && query.next() ? query.value(0).toInt() : 0;
}

QString ContactStore::syncToken(const QString &account, Source source) const
{
    if (!isOpen()) {
        return {};
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    query.prepare(u"SELECT token FROM sync_state WHERE account = ? AND source = ?"_s);
    query.addBindValue(account);
    query.addBindValue(int(source));
    return query.exec() && query.next() ? query.value(0).toString() : QString();
}

bool ContactStore::setSyncToken(const QString &account, Source source, const QString &token)
{
    if (!isOpen()) {
        m_lastError = u"Contact store is not open"_s;
        return false;
    }
    QSqlQuery query(QSqlDatabase::database(m_connectionName, false));
    if (token.isEmpty()) {
        query.prepare(u"DELETE FROM sync_state WHERE account = ? AND source = ?"_s);
        query.addBindValue(account);
        query.addBindValue(int(source));
    } else {
        query.prepare(u"INSERT OR REPLACE INTO sync_state (account, source, token) VALUES (?, ?, ?)"_s);
        query.addBindValue(account);
        query.addBindValue(int(source));
        query.addBindValue(token);
    }
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    return true;
}
