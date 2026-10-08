/*
 * SPDX-FileCopyrightText: 2026 Free Software Foundation Europe e.V. <https://fsfe.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "folderutils.h"

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
    static const QStringList order = {u"inbox"_s, u"flagged"_s, u"drafts"_s, u"sent"_s, u"archive"_s, u"all"_s, u"junk"_s, u"trash"_s};
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