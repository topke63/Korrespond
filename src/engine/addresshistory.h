// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace KMime
{
class Message;
}

// An address that appears in a message header.
struct SeenAddress {
    QString email;
    QString name; // may be empty

    bool operator==(const SeenAddress &) const = default;
};

// The addresses of From, To, Cc and Bcc, each once (by lower-cased address), in that order, with
// the display name when the header has one.
QList<SeenAddress> seenAddresses(const KMime::Message &message);
// The same for a raw message; only its header block is read.
QList<SeenAddress> seenAddressesInRaw(const QByteArray &raw);

// True for addresses that nobody writes to: no-reply, do-not-reply, mailer-daemon, bounces.
bool isAutomatedAddress(const QString &email);
