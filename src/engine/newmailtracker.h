// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "imapclient.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

// Decides which of the messages a background check fetched are new mail worth announcing. Keeps
// the highest uid seen per account and folder, in memory only: the first check after the app
// starts just records what is there, so old mail is never announced.
class NewMailTracker
{
public:
    // Messages in `summaries` (the newest of `folder`) that are new since the last call for this
    // account and folder: uid above the highest seen, unread, and not older than a day (a message
    // without a date counts as recent). Newest uid first. The first call, and the first call
    // after `uidValidity` changed (every old uid is meaningless then), only record and return
    // nothing; pass 0 when the value is unknown. Read and old messages are recorded too, so they
    // are not announced later.
    QList<MessageSummary>
    newMessages(const QString &account, const QString &folder, qint64 uidValidity, const QList<MessageSummary> &summaries, const QDateTime &now);

    // Forgets an account, e.g. after it was removed.
    void forget(const QString &account);

private:
    struct State {
        qint64 uidValidity = 0;
        qint64 highestUid = 0;
    };
    QHash<QString, State> m_states; // key: account + '\n' + folder
};
