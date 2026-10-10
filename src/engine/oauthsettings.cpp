// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "oauthsettings.h"

#include <KConfigGroup>
#include <KSharedConfig>

using namespace Qt::StringLiterals;

bool GoogleOAuthSettings::isConfigured() const
{
    return !clientId.isEmpty() && !clientSecret.isEmpty();
}

// A set but empty variable doesn't hide the config value.
// legacyEnvName: the variable of the version that was called "Plasma Mail".
static QString value(const char *envName, const char *legacyEnvName, const KConfigGroup &group, const char *key)
{
    QString env = qEnvironmentVariable(envName).trimmed();
    if (env.isEmpty()) {
        env = qEnvironmentVariable(legacyEnvName).trimmed();
    }
    return env.isEmpty() ? group.readEntry(key, QString()).trimmed() : env;
}

GoogleOAuthSettings GoogleOAuthSettings::load()
{
    const KConfigGroup group = KSharedConfig::openConfig(u"korrespondrc"_s)->group(u"GoogleOAuth"_s);
    GoogleOAuthSettings settings;
    settings.clientId = value("KORRESPOND_GOOGLE_CLIENT_ID", "PLASMAMAIL_GOOGLE_CLIENT_ID", group, "ClientId");
    settings.clientSecret = value("KORRESPOND_GOOGLE_CLIENT_SECRET", "PLASMAMAIL_GOOGLE_CLIENT_SECRET", group, "ClientSecret");
    return settings;
}
