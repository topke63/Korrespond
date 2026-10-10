// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QKeySequence>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>

// The Addons' shortcuts model without some actions: the "Go to <folder>" and "Move to <folder>"
// entries of the command bar come and go with the folders and are not for the shortcuts editor.
// The editor calls the model's own methods, so they are forwarded here with the row mapped back.
class ShortcutsProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ShortcutsProxyModel(QAbstractListModel *source, QObject *parent = nullptr);

    // The action texts to leave out (the actionName role); an empty set shows everything.
    void setHiddenNames(const QSet<QString> &names);

    Q_INVOKABLE QList<QKeySequence> updateShortcut(int row, int alternate, const QKeySequence &sequence);
    Q_INVOKABLE QList<QKeySequence> reset(int row);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE void save();
    Q_INVOKABLE QKeySequence emptyKeySequence();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QSet<QString> m_hidden;
    int m_nameRole = -1;
};
