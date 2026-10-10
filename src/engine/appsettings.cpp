// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "appsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QUrl>

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

static int clampSidebar(int width)
{
    return std::clamp(width, AppSettings::minSidebarWidth, AppSettings::maxSidebarWidth);
}

static int clampList(int width)
{
    return std::clamp(width, AppSettings::minListWidth, AppSettings::maxListWidth);
}

// Subgroup of [General]: one key per account, the email percent-encoded (so no character of it can
// clash with the config file syntax), the value a list of folder paths (KConfig escapes the commas
// and backslashes in a list).
static KConfigGroup hiddenGroup()
{
    return generalGroup().group(u"HiddenFolders"_s);
}

AppSettings AppSettings::load()
{
    const KConfigGroup group = generalGroup();
    AppSettings settings;
    settings.keepInTray = group.readEntry("KeepInTray", settings.keepInTray);
    settings.checkMinutes = clampMinutes(group.readEntry("CheckMinutes", settings.checkMinutes));
    settings.pushEnabled = group.readEntry("PushEnabled", settings.pushEnabled);
    settings.sidebarWidth = clampSidebar(group.readEntry("SidebarWidth", settings.sidebarWidth));
    settings.listWidth = clampList(group.readEntry("ListWidth", settings.listWidth));
    settings.sidebarCollapsed = group.readEntry("SidebarCollapsed", settings.sidebarCollapsed);
    settings.collapsedAccounts = group.readEntry("CollapsedAccounts", settings.collapsedAccounts);
    settings.showHiddenFolders = group.readEntry("ShowHiddenFolders", settings.showHiddenFolders);
    const KConfigGroup hidden = hiddenGroup();
    const QStringList keys = hidden.keyList();
    for (const QString &key : keys) {
        const QStringList paths = hidden.readEntry(key, QStringList());
        const QString email = QUrl::fromPercentEncoding(key.toUtf8());
        if (!paths.isEmpty() && !email.isEmpty()) {
            settings.hiddenFolders.insert(email, paths);
        }
    }
    return settings;
}

void AppSettings::save() const
{
    KConfigGroup group = generalGroup();
    group.writeEntry("KeepInTray", keepInTray);
    group.writeEntry("CheckMinutes", clampMinutes(checkMinutes));
    group.writeEntry("PushEnabled", pushEnabled);
    group.writeEntry("SidebarWidth", clampSidebar(sidebarWidth));
    group.writeEntry("ListWidth", clampList(listWidth));
    group.writeEntry("SidebarCollapsed", sidebarCollapsed);
    group.writeEntry("CollapsedAccounts", collapsedAccounts);
    group.writeEntry("ShowHiddenFolders", showHiddenFolders);
    KConfigGroup hidden = hiddenGroup();
    for (const QString &key : hidden.keyList()) {
        hidden.deleteEntry(key);
    }
    for (auto it = hiddenFolders.cbegin(); it != hiddenFolders.cend(); ++it) {
        if (!it.value().isEmpty()) {
            hidden.writeEntry(QString::fromLatin1(QUrl::toPercentEncoding(it.key())), it.value());
        }
    }
    group.sync();
}
