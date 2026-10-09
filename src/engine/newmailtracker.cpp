// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "newmailtracker.h"

#include <algorithm>

using namespace Qt::StringLiterals;

// Older mail that only now shows up (e.g. moved in, or the app was not running) is not news.
static constexpr qint64 maxAgeSeconds = 24 * 60 * 60;

QList<MessageSummary>
NewMailTracker::newMessages(const QString &account, const QString &folder, qint64 uidValidity, const QList<MessageSummary> &summaries, const QDateTime &now)
{
    qint64 highest = 0;
    for (const MessageSummary &message : summaries) {
        highest = std::max(highest, message.uid);
    }

    const QString key = account + u'\n' + folder;
    const auto it = m_states.find(key);
    const bool firstTime = it == m_states.end();
    const bool validityChanged = !firstTime && uidValidity != 0 && it->uidValidity != 0 && it->uidValidity != uidValidity;
    if (firstTime || validityChanged) {
        m_states.insert(key, State{uidValidity, highest});
        return {};
    }

    QList<MessageSummary> result;
    for (const MessageSummary &message : summaries) {
        const bool recent = !message.date.isValid() || message.date.secsTo(now) <= maxAgeSeconds;
        if (message.uid > it->highestUid && message.unread && recent) {
            result.append(message);
        }
    }
    std::sort(result.begin(), result.end(), [](const MessageSummary &a, const MessageSummary &b) {
        return a.uid > b.uid;
    });
    it->highestUid = std::max(it->highestUid, highest);
    if (uidValidity != 0) {
        it->uidValidity = uidValidity;
    }
    return result;
}

void NewMailTracker::forget(const QString &account)
{
    const QString prefix = account + u'\n';
    for (auto it = m_states.begin(); it != m_states.end();) {
        it = it.key().startsWith(prefix) ? m_states.erase(it) : std::next(it);
    }
}
