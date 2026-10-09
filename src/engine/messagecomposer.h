// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>

namespace KMime
{
class Message;
}

// What a reply or a forward starts from: subjects, recipients, the quoted text and the thread
// headers, worked out from the original message. Pure functions, no I/O.
namespace MessageComposer
{
// "Re: x" / "Fwd: x"; a subject that already starts with "Re:", "RE:", "Re[2]:" (for a reply) or
// "Fwd:", "Fw:" (for a forward) is kept as it is.
QString replySubject(const QString &subject);
QString forwardSubject(const QString &subject);

struct Recipients {
    QStringList to; // "Name <address>" or "address"
    QStringList cc;
};
// Reply: the Reply-To (else From) of the original; when that is the user's own address (a reply to
// a sent message) the original To instead. Reply all: also the original To and Cc, without the
// user's own addresses and without anyone listed twice (compared by address, ignoring case).
Recipients replyRecipients(const KMime::Message &original, const QStringList &ownAddresses, bool replyAll);

// The text of the original as plain text (the text/plain part, else the HTML part turned into
// text), line ends as \n.
QString plainBody(const KMime::Message &original);

// "On <date>, <name> wrote:" followed by the original with every line quoted with "> ".
QString quotedReply(const KMime::Message &original);
// The usual "---- Forwarded message ----" block with From, Date, Subject and To, then the text.
QString forwardedBody(const KMime::Message &original);

struct ThreadIds {
    QString inReplyTo; // the original's Message-ID, without <>; empty when it has none
    QStringList references; // the original's References and then its Message-ID, without <>, no repeats
};
ThreadIds threadIds(const KMime::Message &original);
}
