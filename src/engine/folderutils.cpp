// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "folderutils.h"

#include <algorithm>

using namespace Qt::StringLiterals;

static QString roleFromFlags(const QString &path, const QList<QByteArray> &flags)
{
    if (path.compare("INBOX"_L1, Qt::CaseInsensitive) == 0) {
        return u"inbox"_s;
    }
    // RFC 6154 special-use flags
    static const QList<QPair<QByteArray, QString>> roles = {
        {"\\sent", u"sent"_s},
        {"\\drafts", u"drafts"_s},
        {"\\trash", u"trash"_s},
        {"\\junk", u"junk"_s},
        {"\\archive", u"archive"_s},
        {"\\all", u"all"_s},
        {"\\flagged", u"flagged"_s},
        {"\\important", u"important"_s}, // Gmail
    };
    for (const QByteArray &flag : flags) {
        const QByteArray lower = flag.toLower();
        for (const auto &[special, role] : roles) {
            if (lower == special) {
                return role;
            }
        }
    }
    return {};
}

static int roleOrder(const QString &role)
{
    static const QStringList order = {u"inbox"_s, u"flagged"_s, u"drafts"_s, u"sent"_s, u"archive"_s, u"all"_s, u"important"_s, u"junk"_s, u"trash"_s};
    const qsizetype index = order.indexOf(role);
    return index < 0 ? int(order.size()) : int(index);
}

QString folderRoleFromFlags(const QString &path, const QList<QByteArray> &flags)
{
    return roleFromFlags(path, flags);
}

void sortFolders(QList<FolderInfo> &folders)
{
    // Special folders first, the rest alphabetically by path.
    std::stable_sort(folders.begin(), folders.end(), [](const FolderInfo &a, const FolderInfo &b) {
        const int ra = roleOrder(a.role);
        const int rb = roleOrder(b.role);
        if (ra != rb) {
            return ra < rb;
        }
        return a.path.compare(b.path, Qt::CaseInsensitive) < 0;
    });
}

QString folderForRole(const QList<FolderInfo> &folders, const QString &role)
{
    const auto it = std::find_if(folders.cbegin(), folders.cend(), [&role](const FolderInfo &folder) {
        return folder.role == role;
    });
    return it == folders.cend() ? QString() : it->path;
}

QString archiveTarget(const QList<FolderInfo> &folders)
{
    const QString archive = folderForRole(folders, QStringLiteral("archive"));
    return archive.isEmpty() ? folderForRole(folders, QStringLiteral("all")) : archive;
}

bool canHideFolder(const QList<FolderInfo> &folders, const QString &path)
{
    if (path.isEmpty() || path.compare(QStringLiteral("INBOX"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    return path != folderForRole(folders, QStringLiteral("inbox"));
}
