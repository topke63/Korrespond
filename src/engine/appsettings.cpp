// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "appsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <algorithm>

using namespace Qt::StringLiterals;

static KConfigGroup generalGroup()
{
    return KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"General"_s);
}

static int clampMinutes(int minutes)
{
    return std::clamp(minutes, AppSettings::minCheckMinutes, AppSettings::maxCheckMinutes);
}

AppSettings AppSettings::load()
{
    const KConfigGroup group = generalGroup();
    AppSettings settings;
    settings.keepInTray = group.readEntry("KeepInTray", settings.keepInTray);
    settings.checkMinutes = clampMinutes(group.readEntry("CheckMinutes", settings.checkMinutes));
    return settings;
}

void AppSettings::save() const
{
    KConfigGroup group = generalGroup();
    group.writeEntry("KeepInTray", keepInTray);
    group.writeEntry("CheckMinutes", clampMinutes(checkMinutes));
    group.sync();
}
