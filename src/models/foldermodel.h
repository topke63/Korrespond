// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "folderutils.h"

#include <QAbstractListModel>
#include <QList>
#include <QString>

class FolderModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        NameRole,
        FolderRoleRole,
        DepthRole,
        IconNameRole,
    };

    explicit FolderModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setFolders(const QList<FolderInfo> &folders);
    // Translated name for display; returns `path` itself for unknown folders.
    [[nodiscard]] QString displayName(const QString &path) const;
    // Path of the inbox, or of the first folder if there is no inbox, or empty.
    [[nodiscard]] QString inboxPath() const;

private:
    QList<FolderInfo> m_folders;
};
