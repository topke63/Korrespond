// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "imapclient.h"

#include <QHash>
#include <QList>
#include <QString>

// Groups the messages of one folder into conversations. Pure: no store, no server.
//
// 1. A message with a Gmail thread id (X-GM-THRID) joins every message with the same id; that id
//    is the server's own answer and wins over the headers.
// 2. The others are linked through Message-ID / In-Reply-To / References (a shared, even missing,
//    ancestor joins two replies).
// 3. A message that has no link at all falls back to its normalised subject: it joins the nearest
//    thread with that subject whose newest message is at most 30 days from it.
//
// The id of a thread depends only on its members: "g:<thrid>" for Gmail, else "m:<Message-ID>" or
// "u:<uid>" of the oldest message (date, then uid).
namespace ThreadBuilder
{
// uid → thread id for every message in `messages`.
QHash<qint64, QString> build(const QList<MessageSummary> &messages);

// Subject without Re:/Fwd:/AW:/SV:… prefixes (repeated ones too) and surrounding space, case-folded
// and with inner whitespace collapsed; empty for an empty subject.
QString normalisedSubject(const QString &subject);
}
