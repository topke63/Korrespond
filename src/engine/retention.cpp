// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "retention.h"

RetentionResult applyRetention(MessageStore &store, const StorageSettings &settings, const QDateTime &now)
{
    RetentionResult result;
    auto remove = [&result](int count) {
        if (count < 0) {
            result.ok = false;
        } else {
            result.removed += count;
        }
    };

    if (!settings.enabled) {
        // removeOldestUntil(0) counts the messages; clear() then also drops stray files.
        remove(store.removeOldestUntil(0));
        if (result.ok && !store.clear()) {
            result.ok = false;
        }
    } else {
        const QDateTime cutoff = settings.cutoff(now);
        if (cutoff.isValid()) {
            remove(store.removeOlderThan(cutoff));
        }
        const qint64 maxBytes = settings.maxBytes();
        if (result.ok && maxBytes > 0) {
            remove(store.removeOldestUntil(maxBytes));
        }
    }
    result.remainingBytes = store.totalSize();
    return result;
}
