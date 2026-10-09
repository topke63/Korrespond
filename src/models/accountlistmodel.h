// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QPointer>

class AccountSession;

// One row per account, in the order of the sessions. The sidebar shows each account's folders
// through the folderModel role; the settings list shows the other roles. Rows follow the
// sessions: a session that is deleted drops out of the list.
class AccountListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        EmailRole = Qt::UserRole + 1,
        DisplayNameRole,
        ProviderRole, // readable, e.g. "Yahoo"
        ServerRole, // "host:port" of the IMAP server
        AuthMethodRole, // "password" or "oauth"
        ConnectionStateRole, // "idle", "connecting", "connected" or "failed"
        StatusTextRole, // the last error while failed, else empty
        SyncTextRole, // e.g. "Syncing Inbox 120 / 340", empty when no sync runs
        PushTextRole, // "Push: active", "Push: connecting", ...
        FolderModelRole, // the account's FolderModel, as QObject*
    };

    explicit AccountListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // The sessions stay owned by the caller.
    void setSessions(const QList<AccountSession *> &sessions);
    [[nodiscard]] AccountSession *session(int row) const;
    // The push setting; the push text of every row depends on it.
    void setPushEnabled(bool enabled);
    // Moves the row `from` to the place `to` (the row ends up at index `to`) without a reset.
    // False if either index is out of range or the move is refused.
    bool moveRow(int from, int to);

    // The readable name of an Account::provider value ("yahoo" → "Yahoo", unknown → "Custom IMAP").
    [[nodiscard]] static QString providerName(const QString &provider);

private:
    QList<QPointer<AccountSession>> m_sessions;
    bool m_pushEnabled = true;
};
