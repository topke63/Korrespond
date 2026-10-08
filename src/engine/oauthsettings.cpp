// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "oauthsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

bool GoogleOAuthSettings::isConfigured() const
{
    return !clientId.isEmpty() && !clientSecret.isEmpty();
}

// A set but empty variable doesn't hide the config value.
static QString value(const char *envName, const KConfigGroup &group, const char *key)
{
    const QString env = qEnvironmentVariable(envName).trimmed();
    return env.isEmpty() ? group.readEntry(key, QString()).trimmed() : env;
}

GoogleOAuthSettings GoogleOAuthSettings::load()
{
    const KConfigGroup group = KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"GoogleOAuth"_s);
    GoogleOAuthSettings settings;
    settings.clientId = value("PLASMAMAIL_GOOGLE_CLIENT_ID", group, "ClientId");
    settings.clientSecret = value("PLASMAMAIL_GOOGLE_CLIENT_SECRET", group, "ClientSecret");
    return settings;
}
