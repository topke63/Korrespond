// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "conversationmodel.h"

#include <QLocale>

#include <algorithm>

ConversationModel::ConversationModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConversationModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QVariant ConversationModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Item &item = m_items.at(index.row());
    switch (role) {
    case UidRole:
        return item.summary.uid;
    case AccountRole:
        return item.summary.account;
    case FromRole:
        return item.summary.from;
    case FromNameRole:
        return item.summary.from.contains(u'<') ? item.summary.from.left(item.summary.from.lastIndexOf(u'<')).trimmed().remove(u'"') : item.summary.from;
    case DateRole:
        return item.summary.date;
    case DateTextRole:
        return item.summary.date.isValid() ? QLocale().toString(item.summary.date.toLocalTime(), QLocale::ShortFormat) : QString();
    case PreviewRole:
        return item.summary.preview;
    case UnreadRole:
        return item.summary.unread;
    case FlaggedRole:
        return item.summary.flagged;
    case ExpandedRole:
        return item.expanded;
    case LoadedRole:
        return item.loaded;
    case HtmlRole:
        return item.html;
    case HasRemoteImagesRole:
        return item.hasRemoteImages;
    case ImagesLoadedRole:
        return item.imagesLoaded;
    case AttachmentsRole:
        return item.attachments;
    }
    return {};
}

QHash<int, QByteArray> ConversationModel::roleNames() const
{
    return {
        {UidRole, "uid"},
        {AccountRole, "account"},
        {FromRole, "from"},
        {FromNameRole, "fromName"},
        {DateRole, "date"},
        {DateTextRole, "dateText"},
        {PreviewRole, "preview"},
        {UnreadRole, "unread"},
        {FlaggedRole, "flagged"},
        {ExpandedRole, "expanded"},
        {LoadedRole, "loaded"},
        {HtmlRole, "html"},
        {HasRemoteImagesRole, "hasRemoteImages"},
        {ImagesLoadedRole, "imagesLoaded"},
        {AttachmentsRole, "attachments"},
    };
}

void ConversationModel::setMessages(const QList<MessageSummary> &messages)
{
    QList<MessageSummary> sorted = messages;
    std::sort(sorted.begin(), sorted.end(), [](const MessageSummary &a, const MessageSummary &b) {
        if (a.date.isValid() != b.date.isValid()) {
            return !a.date.isValid(); // undated first, like the oldest
        }
        return a.date != b.date ? a.date < b.date : a.uid < b.uid;
    });
    beginResetModel();
    m_items.clear();
    for (qsizetype i = 0; i < sorted.size(); ++i) {
        Item item;
        item.summary = sorted.at(i);
        item.expanded = item.summary.unread || i == sorted.size() - 1;
        m_items.append(item);
    }
    endResetModel();
}

void ConversationModel::clear()
{
    if (m_items.isEmpty()) {
        return;
    }
    beginResetModel();
    m_items.clear();
    endResetModel();
}

int ConversationModel::indexOf(const QString &account, qint64 uid) const
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).summary.uid == uid && m_items.at(i).summary.account == account) {
            return int(i);
        }
    }
    return -1;
}

MessageSummary ConversationModel::summary(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).summary : MessageSummary();
}

bool ConversationModel::isExpanded(int row) const
{
    return row >= 0 && row < m_items.size() && m_items.at(row).expanded;
}

bool ConversationModel::isLoaded(int row) const
{
    return row >= 0 && row < m_items.size() && m_items.at(row).loaded;
}

std::shared_ptr<KMime::Message> ConversationModel::parsed(int row) const
{
    return row >= 0 && row < m_items.size() ? m_items.at(row).message : nullptr;
}

void ConversationModel::changed(int row, const QList<int> &roles)
{
    const QModelIndex idx = index(row);
    Q_EMIT dataChanged(idx, idx, roles);
}

void ConversationModel::setExpanded(int row, bool expanded)
{
    if (row < 0 || row >= m_items.size() || m_items.at(row).expanded == expanded) {
        return;
    }
    m_items[row].expanded = expanded;
    changed(row, {ExpandedRole});
}

void ConversationModel::setBody(int row,
                                const std::shared_ptr<KMime::Message> &message,
                                const QString &html,
                                bool hasRemoteImages,
                                const QStringList &attachments)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    Item &item = m_items[row];
    item.message = message;
    item.html = html;
    item.hasRemoteImages = hasRemoteImages;
    item.imagesLoaded = false;
    item.attachments = attachments;
    item.loaded = true;
    changed(row, {LoadedRole, HtmlRole, HasRemoteImagesRole, ImagesLoadedRole, AttachmentsRole});
}

void ConversationModel::setHtml(int row, const QString &html, bool imagesLoaded)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    m_items[row].html = html;
    m_items[row].imagesLoaded = imagesLoaded;
    changed(row, {HtmlRole, ImagesLoadedRole});
}

void ConversationModel::setFailed(int row)
{
    if (row < 0 || row >= m_items.size()) {
        return;
    }
    m_items[row].loaded = true;
    m_items[row].html.clear();
    changed(row, {LoadedRole, HtmlRole});
}

void ConversationModel::setUnread(const QString &account, qint64 uid, bool unread)
{
    const int row = indexOf(account, uid);
    if (row >= 0 && m_items.at(row).summary.unread != unread) {
        m_items[row].summary.unread = unread;
        changed(row, {UnreadRole});
    }
}

void ConversationModel::setFlagged(const QString &account, qint64 uid, bool flagged)
{
    const int row = indexOf(account, uid);
    if (row >= 0 && m_items.at(row).summary.flagged != flagged) {
        m_items[row].summary.flagged = flagged;
        changed(row, {FlaggedRole});
    }
}

int ConversationModel::removeMessage(const QString &account, qint64 uid)
{
    const int row = indexOf(account, uid);
    if (row < 0) {
        return -1;
    }
    beginRemoveRows({}, row, row);
    m_items.removeAt(row);
    endRemoveRows();
    return row;
}
