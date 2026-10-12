// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QStringList>

// A small built-in list of common emoji for the message window's picker, with names for searching.
// Not the whole Unicode set: about 250 that people actually use in mail.
class EmojiModel : public QAbstractListModel
{
    Q_OBJECT
    // Case-insensitive text the name must contain; empty = everything.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    // One of categories(), or empty for all.
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY categoryChanged)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)

public:
    enum Roles {
        EmojiRole = Qt::UserRole + 1,
        NameRole,
        CategoryRole,
    };

    explicit EmojiModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const;
    void setFilter(const QString &filter);
    QString category() const;
    void setCategory(const QString &category);
    // The category ids in display order: smileys, people, animals, food, travel, activities,
    // objects, symbols.
    QStringList categories() const;

    // The emoji in a row of the current (filtered) list; empty when out of range.
    Q_INVOKABLE QString emojiAt(int row) const;

    // The emoji of the first entry of a category, for its tab.
    Q_INVOKABLE QString categoryEmoji(const QString &category) const;

Q_SIGNALS:
    void filterChanged();
    void categoryChanged();

private:
    void refresh();

    QString m_filter;
    QString m_category;
    QList<int> m_rows; // indices into the static table
};
