// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QList>
#include <QString>

struct FolderInfo {
    QString path; // full IMAP mailbox name
    QString name; // last path component, for display
    QString role; // "inbox", "sent", "drafts", "trash", "junk", "archive", "all", "flagged",
                  // "important" (Gmail) or empty
    int depth = 0;
};

// Returns the role of a folder based on its flags
QString folderRoleFromFlags(const QString &path, const QList<QByteArray> &flags);

// Sorts folders according to the standard order:
// inbox, flagged, drafts, sent, archive, all, important, junk, trash, then others by path
// case-insensitively
void sortFolders(QList<FolderInfo> &folders);

// The path of the first folder with this role, or an empty string when there is none.
QString folderForRole(const QList<FolderInfo> &folders, const QString &role);

// Where Archive moves a message to: the "archive" folder, else (Gmail) "all", whose All Mail
// holds every message that is not in the inbox any more. Empty when there is neither.
QString archiveTarget(const QList<FolderInfo> &folders);

// Whether the user may hide this folder from the sidebar: never the inbox (path "INBOX" in any
// case, or the folder whose role is "inbox"), and never an empty path. Nothing may be hidden
// while the folder list is still unknown (empty), as the inbox could not be recognised.
bool canHideFolder(const QList<FolderInfo> &folders, const QString &path);
