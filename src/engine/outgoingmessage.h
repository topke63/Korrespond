// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>

// A plain-text message the user wrote, ready to be turned into the bytes an SMTP server takes.
struct OutgoingMessage {
    QString fromName; // may be empty
    QString fromAddress;
    // Recipients as typed: "a@b.org" or "Name <a@b.org>"; each field may hold several, separated
    // by commas.
    QStringList to;
    QStringList cc;
    QStringList bcc;
    QString subject;
    QString body;
    // For a reply or forward: the Message-ID of the original (with or without <>) and the
    // Message-IDs of the thread so far, oldest first.
    QString inReplyTo;
    QStringList references;

    struct Built {
        QByteArray data; // the whole message, CRLF line ends; no Bcc header
        // The bare addresses for the SMTP envelope (RCPT TO), Bcc included.
        QStringList recipients;
    };

    // Builds an RFC 5322 message: UTF-8 text (quoted-printable), encoded headers, Date from `now`,
    // a generated Message-ID on the sender's domain, In-Reply-To and References when given.
    [[nodiscard]] Built build(const QDateTime &now) const;
};
