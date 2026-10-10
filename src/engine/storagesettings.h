// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDate>
#include <QDateTime>

// How much mail is kept on this computer. Limits only ever remove local copies; nothing is
// deleted on the server. Stored in plasmamailrc, group "Storage".
struct StorageSettings {
    enum KeepUnit {
        Days,
        Weeks,
        Months,
        Years,
        Forever
    };
    enum SizeUnit {
        MB,
        GB,
        Unlimited
    };

    bool enabled = false;
    QDate importSince; // null = all mail
    int keepValue = 1;
    KeepUnit keepUnit = Years;
    int sizeValue = 1;
    SizeUnit sizeUnit = GB;

    // Storage off; the values used when it is first turned on: start 1 year back, keep 1 year,
    // limit 1 GB.
    static StorageSettings defaults(QDate today);

    // Messages dated before this are not stored: the later of importSince (start of that day)
    // and now − keep-for. Null when there is no cutoff (all mail, kept forever).
    [[nodiscard]] QDateTime cutoff(const QDateTime &now) const;
    // Size limit in bytes (1 MB = 1024×1024 bytes); 0 = unlimited.
    [[nodiscard]] qint64 maxBytes() const;

    // Missing settings fall back to defaults(today).
    static StorageSettings load();
    void save() const;

    bool operator==(const StorageSettings &) const = default;
};
