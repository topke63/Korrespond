/*
 * SPDX-FileCopyrightText: 2026 Free Software Foundation Europe e.V. <https://fsfe.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <QList>
#include <QString>

struct FolderInfo {
    QString path; // full IMAP mailbox name
    QString name; // last path component, for display
    QString role; // "inbox", "sent", "drafts", "trash", "junk", "archive", "all" or empty
    int depth = 0;
};

// Returns the role of a folder based on its flags
QString folderRoleFromFlags(const QString &path, const QList<QByteArray> &flags);

// Sorts folders according to the standard order:
// inbox, flagged, drafts, sent, archive, all, junk, trash, then others by path case-insensitively
void sortFolders(QList<FolderInfo> &folders);