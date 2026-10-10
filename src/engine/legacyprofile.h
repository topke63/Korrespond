// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

// Brings the files of the version that was called "Plasma Mail" over to the current names, once:
//  - <configDir>/plasmamailrc is copied to <configDir>/korrespondrc (the old file stays),
//  - <dataDir>/plasma-mail is moved to <dataDir>/korrespond (the account folders and the mail cache).
// Nothing is touched when the new name already exists, so a profile is never overwritten or merged.
// configDir and dataDir are the XDG config and data base folders (tests pass temporary ones).
namespace LegacyProfile
{
struct Result {
    bool configCopied = false;
    bool dataMoved = false;
};

Result migrate(const QString &configDir, const QString &dataDir);
}
