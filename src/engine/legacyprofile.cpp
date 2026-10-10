// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "legacyprofile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

using namespace Qt::StringLiterals;

LegacyProfile::Result LegacyProfile::migrate(const QString &configDir, const QString &dataDir)
{
    Result result;

    const QString oldConfig = QDir(configDir).filePath(u"plasmamailrc"_s);
    const QString newConfig = QDir(configDir).filePath(u"korrespondrc"_s);
    if (QFileInfo::exists(oldConfig) && !QFileInfo::exists(newConfig)) {
        result.configCopied = QFile::copy(oldConfig, newConfig);
    }

    const QString oldData = QDir(dataDir).filePath(u"plasma-mail"_s);
    const QString newData = QDir(dataDir).filePath(u"korrespond"_s);
    if (QFileInfo(oldData).isDir() && !QFileInfo(oldData).isSymLink() && !QFileInfo::exists(newData)) {
        result.dataMoved = QDir().rename(oldData, newData);
    }
    return result;
}
