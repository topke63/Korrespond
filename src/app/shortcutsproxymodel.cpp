// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "shortcutsproxymodel.h"

#include <QAbstractListModel>
#include <QMetaObject>

ShortcutsProxyModel::ShortcutsProxyModel(QAbstractListModel *source, QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_nameRole = source->roleNames().key("actionName", -1);
    setSourceModel(source);
}

void ShortcutsProxyModel::setHiddenNames(const QSet<QString> &names)
{
    if (m_hidden == names) {
        return;
    }
    m_hidden = names;
    invalidateFilter();
}

bool ShortcutsProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_hidden.isEmpty() || m_nameRole < 0) {
        return true;
    }
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return !m_hidden.contains(index.data(m_nameRole).toString());
}

QList<QKeySequence> ShortcutsProxyModel::updateShortcut(int row, int alternate, const QKeySequence &sequence)
{
    QList<QKeySequence> result;
    const QModelIndex source = mapToSource(index(row, 0));
    if (source.isValid()) {
        QMetaObject::invokeMethod(sourceModel(),
                                  "updateShortcut",
                                  Q_RETURN_ARG(QList<QKeySequence>, result),
                                  Q_ARG(int, source.row()),
                                  Q_ARG(int, alternate),
                                  Q_ARG(QKeySequence, sequence));
    }
    return result;
}

QList<QKeySequence> ShortcutsProxyModel::reset(int row)
{
    QList<QKeySequence> result;
    const QModelIndex source = mapToSource(index(row, 0));
    if (source.isValid()) {
        QMetaObject::invokeMethod(sourceModel(), "reset", Q_RETURN_ARG(QList<QKeySequence>, result), Q_ARG(int, source.row()));
    }
    return result;
}

void ShortcutsProxyModel::resetAll()
{
    QMetaObject::invokeMethod(sourceModel(), "resetAll");
}

void ShortcutsProxyModel::save()
{
    QMetaObject::invokeMethod(sourceModel(), "save");
}

QKeySequence ShortcutsProxyModel::emptyKeySequence()
{
    QKeySequence result;
    QMetaObject::invokeMethod(sourceModel(), "emptyKeySequence", Q_RETURN_ARG(QKeySequence, result));
    return result;
}
