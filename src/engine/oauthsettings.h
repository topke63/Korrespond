// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

// The Google OAuth client the app signs in with ("Desktop app" type, created by the user in the
// Google Cloud console). For a desktop app the secret isn't really secret, but it is still
// kept out of the source: it lives in korrespondrc, group [GoogleOAuth], keys ClientId and
// ClientSecret, or in the environment variables KORRESPOND_GOOGLE_CLIENT_ID and
// KORRESPOND_GOOGLE_CLIENT_SECRET, which win over the config file when not empty.
struct GoogleOAuthSettings {
    QString clientId;
    QString clientSecret;

    // Both values are set; Google's token endpoint needs the secret too.
    [[nodiscard]] bool isConfigured() const;

    static GoogleOAuthSettings load();
};
