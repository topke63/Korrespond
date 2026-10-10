// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "messagestore.h"
#include "storagesettings.h"

#include <QDateTime>

struct RetentionResult {
    bool ok = true; // false when the store failed; MessageStore::lastError() says why
    int removed = 0; // messages removed by this run
    qint64 remainingBytes = 0; // MessageStore::totalSize() afterwards
};

// Trims the local copies to the storage settings: first everything dated before
// settings.cutoff(now), then the oldest messages until the size limit fits. With storage
// turned off, every local copy is removed. Never touches the server.
RetentionResult applyRetention(MessageStore &store, const StorageSettings &settings, const QDateTime &now);
