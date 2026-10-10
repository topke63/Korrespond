// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Local cache of contacts for recipient suggestions and sender names: Google contacts, Google
// "other contacts" and addresses seen in mail ("history"). An SQLite file (contacts.sqlite) in
// baseDir, separate from the message cache so clearing it never touches mail. Only local data;
// not thread-safe (one instance per thread).
class ContactStore
{
public:
    // The order is the ranking: contacts first.
    enum Source {
        Google = 0,
        Other = 1,
        History = 2,
    };

    struct Suggestion {
        QString name; // may be empty
        QString email;
        int source = Google;

        bool operator==(const Suggestion &) const = default;
    };

    // The app passes a folder under AppDataLocation; tests pass a temporary directory.
    explicit ContactStore(const QString &baseDir);
    ~ContactStore();

    ContactStore(const ContactStore &) = delete;
    ContactStore &operator=(const ContactStore &) = delete;

    bool open();
    [[nodiscard]] bool isOpen() const;

    // Adds or replaces one person of a source: all addresses of `resource` are replaced.
    bool
    upsertPerson(const QString &account, Source source, const QString &resource, const QString &name, const QStringList &emails, const QString &photoUrl = {});
    bool removePerson(const QString &account, Source source, const QString &resource);
    // An address seen in a message: remembered once per account; the name of the newest sighting
    // wins when it is not empty; `seenAtMs` (ms since the epoch) orders suggestions.
    bool addHistory(const QString &account, const QString &email, const QString &name, qint64 seenAtMs);
    // Removes one source of an account (its sync token too), or everything of the account when
    // `all` is true; an empty account means every account.
    bool clearSource(const QString &account, Source source);
    bool clearAccount(const QString &account);
    bool clearAll();

    // Up to `limit` addresses whose name (any word) or address starts with `prefix`, best first
    // (source, then newest, then name); one entry per address. An empty prefix gives nothing.
    [[nodiscard]] QList<Suggestion> suggest(const QString &prefix, int limit) const;
    // The best known name for an address, or empty.
    [[nodiscard]] QString nameFor(const QString &email) const;
    [[nodiscard]] int count(Source source) const;

    // Opaque sync state of the People API, per account and source ("" = none).
    [[nodiscard]] QString syncToken(const QString &account, Source source) const;
    bool setSyncToken(const QString &account, Source source, const QString &token);

    [[nodiscard]] QString lastError() const;

private:
    QString m_baseDir;
    QString m_connectionName;
    mutable QString m_lastError;
};
