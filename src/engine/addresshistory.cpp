// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "addresshistory.h"

#include <KMime/Message>

#include <QSet>

using namespace Qt::StringLiterals;

static void collect(const QList<KMime::Types::Mailbox> &mailboxes, QList<SeenAddress> &result, QSet<QString> &seen)
{
    for (const KMime::Types::Mailbox &mailbox : mailboxes) {
        const QString address = QString::fromLatin1(mailbox.address()).trimmed();
        if (address.isEmpty() || !address.contains(u'@')) {
            continue;
        }
        const QString key = address.toLower();
        if (seen.contains(key)) {
            continue;
        }
        seen.insert(key);
        result.append({address, mailbox.name().trimmed()});
    }
}

QList<SeenAddress> seenAddresses(const KMime::Message &message)
{
    QList<SeenAddress> result;
    QSet<QString> seen;
    // The const getters return nullptr for a missing header.
    if (const auto *from = message.from()) {
        collect(from->mailboxes(), result, seen);
    }
    if (const auto *to = message.to()) {
        collect(to->mailboxes(), result, seen);
    }
    if (const auto *cc = message.cc()) {
        collect(cc->mailboxes(), result, seen);
    }
    if (const auto *bcc = message.bcc()) {
        collect(bcc->mailboxes(), result, seen);
    }
    return result;
}

QList<SeenAddress> seenAddressesInRaw(const QByteArray &raw)
{
    // Up to the first empty line: attachments are not parsed.
    qsizetype end = raw.size();
    for (const qsizetype candidate : {raw.indexOf("\r\n\r\n"), raw.indexOf("\n\n")}) {
        if (candidate >= 0 && candidate < end) {
            end = candidate;
        }
    }
    KMime::Message message;
    message.setContent(raw.left(end) + "\n\n");
    message.parse();
    return seenAddresses(message);
}

bool isAutomatedAddress(const QString &email)
{
    const QString local = email.toLower().section(u'@', 0, 0);
    static const QStringList markers =
        {u"noreply"_s, u"no-reply"_s, u"no_reply"_s, u"donotreply"_s, u"do-not-reply"_s, u"do_not_reply"_s, u"mailer-daemon"_s, u"postmaster"_s, u"bounce"_s};
    for (const QString &marker : markers) {
        if (local.contains(marker)) {
            return true;
        }
    }
    return false;
}
