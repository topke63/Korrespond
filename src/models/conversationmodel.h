// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "imapclient.h"

#include <QAbstractListModel>
#include <QList>
#include <QStringList>

#include <memory>

namespace KMime
{
class Message;
}

// The messages of the open conversation, oldest first, as the reading pane stacks them. Only holds
// data: the controller loads and renders the bodies and tells the model.
class ConversationModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UidRole = Qt::UserRole + 1,
        AccountRole,
        FromRole, // "Name <address>"
        FromNameRole, // the name (or the address) only
        DateRole,
        DateTextRole, // full local date and time
        PreviewRole,
        UnreadRole,
        FlaggedRole,
        ExpandedRole,
        LoadedRole, // the body has been rendered (or failed: html is then empty)
        HtmlRole,
        HasRemoteImagesRole,
        ImagesLoadedRole,
        AttachmentsRole,
    };

    explicit ConversationModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // `messages` in any order; sorted oldest first (date, then uid). A message is expanded when it
    // is unread or is the newest. Replaces everything.
    void setMessages(const QList<MessageSummary> &messages);
    void clear();

    [[nodiscard]] int indexOf(const QString &account, qint64 uid) const;
    [[nodiscard]] MessageSummary summary(int row) const;
    [[nodiscard]] bool isExpanded(int row) const;
    [[nodiscard]] bool isLoaded(int row) const;
    [[nodiscard]] std::shared_ptr<KMime::Message> parsed(int row) const;

    void setExpanded(int row, bool expanded);
    // The body arrived: keeps the parsed message for re-rendering.
    void setBody(int row, const std::shared_ptr<KMime::Message> &message, const QString &html, bool hasRemoteImages, const QStringList &attachments);
    void setHtml(int row, const QString &html, bool imagesLoaded);
    // The body can't be shown (gone from the server): loaded with no html.
    void setFailed(int row);
    void setUnread(const QString &account, qint64 uid, bool unread);
    void setFlagged(const QString &account, qint64 uid, bool flagged);
    // Drops one message (moved or deleted elsewhere); returns its row or -1.
    int removeMessage(const QString &account, qint64 uid);

private:
    struct Item {
        MessageSummary summary;
        bool expanded = false;
        bool loaded = false;
        QString html;
        bool hasRemoteImages = false;
        bool imagesLoaded = false;
        QStringList attachments;
        std::shared_ptr<KMime::Message> message;
    };

    void changed(int row, const QList<int> &roles);

    QList<Item> m_items;
};
