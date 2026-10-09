// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "messagecomposer.h"

#include "imapclient.h" // messageDate()

#include <KLocalizedString>
#include <KMime/Message>

#include <QLocale>
#include <QRegularExpression>
#include <QTextDocumentFragment>

using namespace Qt::StringLiterals;

namespace MessageComposer
{
QString replySubject(const QString &subject)
{
    static const QRegularExpression already(u"^\\s*re(\\[\\d+\\])?\\s*:"_s, QRegularExpression::CaseInsensitiveOption);
    const QString trimmed = subject.trimmed();
    return already.match(trimmed).hasMatch() ? trimmed : u"Re: "_s + trimmed;
}

QString forwardSubject(const QString &subject)
{
    static const QRegularExpression already(u"^\\s*fwd?\\s*:"_s, QRegularExpression::CaseInsensitiveOption);
    const QString trimmed = subject.trimmed();
    return already.match(trimmed).hasMatch() ? trimmed : u"Fwd: "_s + trimmed;
}

static QStringList lowered(const QStringList &list)
{
    QStringList result;
    for (const QString &item : list) {
        result.append(item.trimmed().toLower());
    }
    return result;
}

// The mailboxes of an address header as display strings, with their bare addresses next to them.
struct Entry {
    QString display;
    QString address; // lower case
};

static QList<Entry> entries(const QList<KMime::Types::Mailbox> &mailboxes)
{
    QList<Entry> result;
    for (const KMime::Types::Mailbox &mailbox : mailboxes) {
        const QString address = QString::fromLatin1(mailbox.address()).toLower();
        if (!address.isEmpty()) {
            result.append({mailbox.prettyAddress(KMime::Types::Mailbox::QuoteWhenNecessary), address});
        }
    }
    return result;
}

Recipients replyRecipients(const KMime::Message &original, const QStringList &ownAddresses, bool replyAll)
{
    const QStringList own = lowered(ownAddresses);
    const auto isOwn = [&own](const Entry &entry) {
        return own.contains(entry.address);
    };

    QList<Entry> sender;
    if (const auto *replyTo = original.replyTo(); replyTo && !replyTo->mailboxes().isEmpty()) {
        sender = entries(replyTo->mailboxes());
    } else if (const auto *from = original.from()) {
        sender = entries(from->mailboxes());
    }
    const QList<Entry> originalTo = original.to() ? entries(original.to()->mailboxes()) : QList<Entry>();
    const QList<Entry> originalCc = original.cc() ? entries(original.cc()->mailboxes()) : QList<Entry>();

    Recipients result;
    QStringList seen; // addresses already in to or cc
    const auto addTo = [&](const Entry &entry) {
        if (!seen.contains(entry.address)) {
            seen.append(entry.address);
            result.to.append(entry.display);
        }
    };
    const auto addCc = [&](const Entry &entry) {
        if (!seen.contains(entry.address)) {
            seen.append(entry.address);
            result.cc.append(entry.display);
        }
    };

    // Replying to a message the user sent: answer the people it went to.
    const bool fromMe = !sender.isEmpty() && std::all_of(sender.cbegin(), sender.cend(), isOwn);
    for (const Entry &entry : fromMe ? originalTo : sender) {
        addTo(entry);
    }
    if (replyAll) {
        for (const Entry &entry : originalTo) {
            if (!isOwn(entry)) {
                addTo(entry);
            }
        }
        for (const Entry &entry : originalCc) {
            if (!isOwn(entry)) {
                addCc(entry);
            }
        }
    }
    return result;
}

QString plainBody(const KMime::Message &original)
{
    QString text;
    if (const KMime::Content *plain = original.mainBodyPart("text/plain")) {
        text = plain->decodedText();
    } else if (const KMime::Content *html = original.mainBodyPart("text/html"); html && html->contentType() && html->contentType()->isHTMLText()) {
        text = QTextDocumentFragment::fromHtml(html->decodedText()).toPlainText();
    } else {
        text = original.decodedText();
    }
    text.replace(u"\r\n"_s, u"\n"_s).replace(u'\r', u'\n');
    return text;
}

static QString fromText(const KMime::Message &original)
{
    const auto *from = original.from();
    if (!from || from->mailboxes().isEmpty()) {
        return {};
    }
    const KMime::Types::Mailbox mailbox = from->mailboxes().constFirst();
    return mailbox.hasName() ? mailbox.name() : QString::fromLatin1(mailbox.address());
}

QString quotedReply(const KMime::Message &original)
{
    const QDateTime date = messageDate(original);
    const QString header = date.isValid()
        ? i18nc("@info reply header: date, sender", "On %1, %2 wrote:", QLocale().toString(date.toLocalTime(), QLocale::LongFormat), fromText(original))
        : i18nc("@info reply header: sender", "%1 wrote:", fromText(original));
    QString body = plainBody(original);
    while (body.endsWith(u'\n')) {
        body.chop(1);
    }
    QStringList quoted;
    for (const QString &line : body.split(u'\n')) {
        quoted.append(line.isEmpty() ? u">"_s : u"> "_s + line);
    }
    return header + u"\n"_s + quoted.join(u'\n') + u"\n"_s;
}

QString forwardedBody(const KMime::Message &original)
{
    const auto header = [](const QString &label, const QString &value) {
        return value.isEmpty() ? QString() : label + u": "_s + value + u"\n"_s;
    };
    const QDateTime date = messageDate(original);
    QString block = u"---------- "_s + i18nc("@info", "Forwarded message") + u" ----------\n"_s;
    block += header(i18nc("@label forwarded message header", "From"), original.from() ? original.from()->asUnicodeString() : QString());
    block += header(i18nc("@label forwarded message header", "Date"), date.isValid() ? QLocale().toString(date.toLocalTime(), QLocale::LongFormat) : QString());
    block += header(i18nc("@label forwarded message header", "Subject"), original.subject() ? original.subject()->asUnicodeString() : QString());
    block += header(i18nc("@label forwarded message header", "To"), original.to() ? original.to()->asUnicodeString() : QString());
    return block + u"\n"_s + plainBody(original);
}

static QString bareId(const QByteArray &id)
{
    QString text = QString::fromLatin1(id).trimmed();
    if (text.startsWith(u'<')) {
        text.remove(0, 1);
    }
    if (text.endsWith(u'>')) {
        text.chop(1);
    }
    return text;
}

ThreadIds threadIds(const KMime::Message &original)
{
    ThreadIds ids;
    if (const auto *references = original.references()) {
        for (const QByteArray &id : references->identifiers()) {
            if (const QString bare = bareId(id); !bare.isEmpty() && !ids.references.contains(bare)) {
                ids.references.append(bare);
            }
        }
    }
    if (const auto *messageId = original.messageID(); messageId && !messageId->identifier().isEmpty()) {
        ids.inReplyTo = bareId(messageId->identifier());
        if (!ids.references.contains(ids.inReplyTo)) {
            ids.references.append(ids.inReplyTo);
        }
    }
    return ids;
}
}
