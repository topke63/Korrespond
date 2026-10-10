// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "threadbuilder.h"

#include <QRegularExpression>

#include <algorithm>
#include <numeric>

using namespace Qt::StringLiterals;

namespace
{
// Union-find over indexes.
class Groups
{
public:
    explicit Groups(int size)
        : m_parent(size)
    {
        std::iota(m_parent.begin(), m_parent.end(), 0);
    }

    int find(int node)
    {
        while (m_parent[node] != node) {
            m_parent[node] = m_parent[m_parent[node]];
            node = m_parent[node];
        }
        return node;
    }

    void join(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a != b) {
            m_parent[std::max(a, b)] = std::min(a, b);
        }
    }

    int add()
    {
        m_parent.append(int(m_parent.size()));
        return int(m_parent.size()) - 1;
    }

private:
    QList<int> m_parent;
};

constexpr qint64 subjectWindowDays = 30;
}

QString ThreadBuilder::normalisedSubject(const QString &subject)
{
    // Re, Fw, Fwd, AW (de), SV (sv/no), VS, WG, Antw, Odp (pl), optional "[2]" counter, then a colon.
    static const QRegularExpression prefix(u"^\\s*(?:re|fwd?|aw|sv|vs|wg|antw|odp|ref)(?:\\[\\d+\\])?\\s*:\\s*"_s, QRegularExpression::CaseInsensitiveOption);
    QString result = subject;
    for (;;) {
        const QRegularExpressionMatch match = prefix.match(result);
        if (!match.hasMatch()) {
            break;
        }
        result.remove(0, match.capturedLength());
    }
    return result.simplified().toCaseFolded();
}

QHash<qint64, QString> ThreadBuilder::build(const QList<MessageSummary> &messages)
{
    const int count = int(messages.size());
    QHash<qint64, QString> result;
    if (count == 0) {
        return result;
    }

    Groups groups(count);
    // One extra node per Message-ID that no message of the folder has (a missing ancestor).
    QHash<QString, int> nodeOfId;
    for (int i = 0; i < count; ++i) {
        const QString &id = messages.at(i).messageId;
        if (!id.isEmpty() && !nodeOfId.contains(id)) {
            nodeOfId.insert(id, i);
        }
    }
    auto nodeFor = [&](const QString &id) {
        auto it = nodeOfId.constFind(id);
        if (it != nodeOfId.constEnd()) {
            return it.value();
        }
        const int node = groups.add();
        nodeOfId.insert(id, node);
        return node;
    };

    // 1. Gmail's own thread ids.
    QHash<qint64, int> firstOfGmail;
    QList<bool> linked(count, false); // has a header link or a Gmail id
    for (int i = 0; i < count; ++i) {
        const qint64 gm = messages.at(i).gmThreadId;
        if (gm <= 0) {
            continue;
        }
        linked[i] = true;
        auto it = firstOfGmail.constFind(gm);
        if (it == firstOfGmail.constEnd()) {
            firstOfGmail.insert(gm, i);
        } else {
            groups.join(it.value(), i);
        }
    }

    // 2. Headers, for the messages that have no Gmail id (a Gmail message is placed by its id only,
    // so a thread Gmail split stays split).
    QList<bool> referenced(count, false);
    for (int i = 0; i < count; ++i) {
        const MessageSummary &message = messages.at(i);
        if (message.gmThreadId > 0) {
            continue;
        }
        QStringList ids = message.references;
        if (!message.inReplyTo.isEmpty() && !ids.contains(message.inReplyTo)) {
            ids.append(message.inReplyTo);
        }
        for (const QString &id : std::as_const(ids)) {
            if (id == message.messageId) {
                continue; // a message that references itself
            }
            const int other = nodeFor(id);
            groups.join(i, other);
            linked[i] = true;
            if (other < count) {
                referenced[other] = true;
            }
        }
    }
    for (int i = 0; i < count; ++i) {
        linked[i] = linked[i] || referenced[i];
    }

    // 3. Subject fallback for the messages without any link, in date order so "nearest" is the
    // newest earlier message of the same subject.
    QList<int> order(count);
    std::iota(order.begin(), order.end(), 0);
    auto dateOf = [&](int i) {
        const QDateTime &date = messages.at(i).date;
        return date.isValid() ? date.toMSecsSinceEpoch() : std::numeric_limits<qint64>::min();
    };
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        return dateOf(a) != dateOf(b) ? dateOf(a) < dateOf(b) : messages.at(a).uid < messages.at(b).uid;
    });
    QHash<QString, int> newestOfSubject; // normalised subject → index of the newest message so far
    const qint64 window = subjectWindowDays * 24 * 3600 * 1000;
    for (const int i : std::as_const(order)) {
        const QString subject = normalisedSubject(messages.at(i).subject);
        if (subject.isEmpty()) {
            continue;
        }
        auto it = newestOfSubject.find(subject);
        if (it != newestOfSubject.end() && !linked[i] && messages.at(i).date.isValid() && messages.at(it.value()).date.isValid()
            && dateOf(i) - dateOf(it.value()) <= window) {
            groups.join(i, it.value());
        }
        // Linked messages join the subject index too, so an unlinked reply finds their thread.
        if (it == newestOfSubject.end()) {
            newestOfSubject.insert(subject, i);
        } else {
            it.value() = i;
        }
    }

    // Ids: the oldest member names the thread.
    QHash<int, int> oldestOfGroup;
    for (const int i : std::as_const(order)) {
        const int root = groups.find(i);
        if (!oldestOfGroup.contains(root)) {
            oldestOfGroup.insert(root, i);
        }
    }
    QHash<int, QString> idOfGroup;
    for (auto it = oldestOfGroup.cbegin(); it != oldestOfGroup.cend(); ++it) {
        const MessageSummary &oldest = messages.at(it.value());
        QString id;
        // A Gmail thread is named by its Gmail id, whichever member is oldest.
        for (int i = 0; i < count && id.isEmpty(); ++i) {
            if (groups.find(i) == it.key() && messages.at(i).gmThreadId > 0) {
                id = u"g:"_s + QString::number(messages.at(i).gmThreadId);
            }
        }
        if (id.isEmpty()) {
            if (!oldest.messageId.isEmpty()) {
                id = u"m:"_s + oldest.messageId;
            } else {
                id = u"u:"_s + QString::number(oldest.uid);
            }
        }
        idOfGroup.insert(it.key(), id);
    }
    for (int i = 0; i < count; ++i) {
        result.insert(messages.at(i).uid, idOfGroup.value(groups.find(i)));
    }
    return result;
}
