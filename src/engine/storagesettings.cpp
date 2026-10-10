// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "storagesettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QTimeZone>

#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

// Config values by enum order; names keep the file readable and stable if enums are reordered.
static constexpr std::array keepUnitNames{"days", "weeks", "months", "years", "forever"};
static constexpr std::array sizeUnitNames{"MB", "GB", "unlimited"};

static KConfigGroup storageGroup()
{
    return KSharedConfig::openConfig(u"korrespondrc"_s)->group(u"Storage"_s);
}

template<typename Enum, std::size_t N>
static Enum readUnit(const KConfigGroup &group, const char *key, const std::array<const char *, N> &names, Enum fallback)
{
    const QString value = group.readEntry(key, QString());
    for (std::size_t i = 0; i < N; ++i) {
        if (value == QLatin1StringView(names[i])) {
            return static_cast<Enum>(i);
        }
    }
    return fallback;
}

StorageSettings StorageSettings::defaults(QDate today)
{
    StorageSettings settings;
    settings.importSince = today.addYears(-1);
    return settings;
}

QDateTime StorageSettings::cutoff(const QDateTime &now) const
{
    QDateTime keepCutoff;
    switch (keepUnit) {
    case Days:
        keepCutoff = now.addDays(-keepValue);
        break;
    case Weeks:
        keepCutoff = now.addDays(-7 * qint64(keepValue));
        break;
    case Months:
        keepCutoff = now.addMonths(-keepValue); // clamps to month end, e.g. Mar 31 → Feb 28
        break;
    case Years:
        keepCutoff = now.addYears(-keepValue);
        break;
    case Forever:
        break;
    }

    const QDateTime startCutoff = importSince.isValid() ? importSince.startOfDay(now.timeZone()) : QDateTime();
    if (!startCutoff.isValid()) {
        return keepCutoff;
    }
    if (!keepCutoff.isValid()) {
        return startCutoff;
    }
    return std::max(startCutoff, keepCutoff);
}

qint64 StorageSettings::maxBytes() const
{
    constexpr qint64 mb = 1024 * 1024;
    switch (sizeUnit) {
    case MB:
        return sizeValue * mb;
    case GB:
        return sizeValue * 1024 * mb;
    case Unlimited:
        break;
    }
    return 0;
}

StorageSettings StorageSettings::load()
{
    const KConfigGroup group = storageGroup();
    StorageSettings settings = defaults(QDate::currentDate());
    settings.enabled = group.readEntry("Enabled", settings.enabled);
    if (group.hasKey("ImportSince")) {
        // An empty value means all mail.
        settings.importSince = QDate::fromString(group.readEntry("ImportSince", QString()), Qt::ISODate);
    }
    settings.keepValue = std::max(1, group.readEntry("KeepValue", settings.keepValue));
    settings.keepUnit = readUnit(group, "KeepUnit", keepUnitNames, settings.keepUnit);
    settings.sizeValue = std::max(1, group.readEntry("SizeValue", settings.sizeValue));
    settings.sizeUnit = readUnit(group, "SizeUnit", sizeUnitNames, settings.sizeUnit);
    return settings;
}

void StorageSettings::save() const
{
    KConfigGroup group = storageGroup();
    group.writeEntry("Enabled", enabled);
    group.writeEntry("ImportSince", importSince.isValid() ? importSince.toString(Qt::ISODate) : QString());
    group.writeEntry("KeepValue", keepValue);
    group.writeEntry("KeepUnit", keepUnitNames[keepUnit]);
    group.writeEntry("SizeValue", sizeValue);
    group.writeEntry("SizeUnit", sizeUnitNames[sizeUnit]);
    group.sync();
}
