// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldermodel.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

// Icon names that exist in the Breeze icon theme.
static QString iconForRole(const QString &role)
{
    if (role == "inbox"_L1) {
        return u"mail-folder-inbox"_s;
    }
    if (role == "sent"_L1) {
        return u"mail-folder-sent"_s;
    }
    if (role == "drafts"_L1) {
        return u"document-edit"_s;
    }
    if (role == "trash"_L1) {
        return u"user-trash"_s;
    }
    if (role == "junk"_L1) {
        return u"mail-mark-junk"_s;
    }
    if (role == "archive"_L1 || role == "all"_L1) {
        return u"mail-folder-outbox"_s;
    }
    if (role == "flagged"_L1) {
        return u"mail-mark-important"_s;
    }
    return u"folder-mail"_s;
}

static QString nameForFolder(const FolderInfo &folder)
{
    if (folder.role == "inbox"_L1) {
        return i18nc("@item:inlistbox mail folder", "Inbox");
    }
    return folder.name;
}

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_folders.size());
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }

    const FolderInfo &folder = m_folders.at(index.row());
    switch (role) {
    case PathRole:
        return folder.path;
    case Qt::DisplayRole:
    case NameRole:
        return nameForFolder(folder);
    case FolderRoleRole:
        return folder.role;
    case DepthRole:
        return folder.depth;
    case Qt::DecorationRole:
    case IconNameRole:
        return iconForRole(folder.role);
    }
    return {};
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {FolderRoleRole, "folderRole"},
        {DepthRole, "depth"},
        {IconNameRole, "iconName"},
    };
}

void FolderModel::setFolders(const QList<FolderInfo> &folders)
{
    beginResetModel();
    m_folders = folders;
    endResetModel();
}

QString FolderModel::displayName(const QString &path) const
{
    for (const FolderInfo &folder : m_folders) {
        if (folder.path == path) {
            return nameForFolder(folder);
        }
    }
    return path;
}

QString FolderModel::inboxPath() const
{
    for (const FolderInfo &folder : m_folders) {
        if (folder.role == "inbox"_L1) {
            return folder.path;
        }
    }
    return m_folders.isEmpty() ? QString() : m_folders.first().path;
}
