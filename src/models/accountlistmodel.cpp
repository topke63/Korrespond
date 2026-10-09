// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "accountlistmodel.h"

#include "accountsession.h"
#include "foldermodel.h"

#include <KLocalizedString>

#include <algorithm>

using namespace Qt::StringLiterals;

AccountListModel::AccountListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AccountListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_sessions.size());
}

QVariant AccountListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const AccountSession *session = m_sessions.at(index.row());
    if (!session) {
        return {};
    }
    const Account &account = session->account();
    switch (role) {
    case EmailRole:
        return account.email;
    case DisplayNameRole:
        return account.displayName;
    case ProviderRole:
        return providerName(account.provider);
    case ServerRole:
        return u"%1:%2"_s.arg(account.imapHost).arg(account.imapPort);
    case AuthMethodRole:
        return account.authMethod;
    case ConnectionStateRole:
        return AccountSession::stateName(session->connectionState());
    case StatusTextRole:
        return session->lastError();
    case SyncTextRole:
        return session->syncText();
    case FolderModelRole:
        return QVariant::fromValue<QObject *>(session->folderModel());
    }
    return {};
}

QHash<int, QByteArray> AccountListModel::roleNames() const
{
    return {
        {EmailRole, "email"},
        {DisplayNameRole, "displayName"},
        {ProviderRole, "provider"},
        {ServerRole, "server"},
        {AuthMethodRole, "authMethod"},
        {ConnectionStateRole, "connectionState"},
        {StatusTextRole, "statusText"},
        {SyncTextRole, "syncText"},
        {FolderModelRole, "folderModel"},
    };
}

void AccountListModel::setSessions(const QList<AccountSession *> &sessions)
{
    beginResetModel();
    for (const auto &old : std::as_const(m_sessions)) {
        if (old) {
            disconnect(old, nullptr, this, nullptr);
        }
    }
    m_sessions.clear();
    for (AccountSession *session : sessions) {
        m_sessions.append(session);
        // The status roles follow the session; the row is looked up again as rows can move.
        const auto changed = [this, session](const QList<int> &roles) {
            const auto it = std::find(m_sessions.cbegin(), m_sessions.cend(), session);
            if (it != m_sessions.cend()) {
                const QModelIndex idx = index(int(it - m_sessions.cbegin()));
                Q_EMIT dataChanged(idx, idx, roles);
            }
        };
        connect(session, &AccountSession::connectionStateChanged, this, [changed] {
            changed({ConnectionStateRole, StatusTextRole});
        });
        connect(session, &AccountSession::syncTextChanged, this, [changed] {
            changed({SyncTextRole});
        });
        // By the time destroyed() arrives the QPointer is already null.
        connect(session, &QObject::destroyed, this, [this] {
            for (int row = int(m_sessions.size()) - 1; row >= 0; --row) {
                if (m_sessions.at(row).isNull()) {
                    beginRemoveRows({}, row, row);
                    m_sessions.removeAt(row);
                    endRemoveRows();
                }
            }
        });
    }
    endResetModel();
}

AccountSession *AccountListModel::session(int row) const
{
    return row >= 0 && row < m_sessions.size() ? m_sessions.at(row).data() : nullptr;
}

bool AccountListModel::moveRow(int from, int to)
{
    const int count = int(m_sessions.size());
    if (from < 0 || from >= count || to < 0 || to >= count || from == to) {
        return false;
    }
    // beginMoveRows wants the row the block goes in front of, so one further when moving down.
    if (!beginMoveRows({}, from, from, {}, to > from ? to + 1 : to)) {
        return false;
    }
    m_sessions.move(from, to);
    endMoveRows();
    return true;
}

QString AccountListModel::providerName(const QString &provider)
{
    if (provider == "yahoo"_L1) {
        return i18nc("@info mail provider", "Yahoo");
    }
    if (provider == "gmail"_L1) {
        return i18nc("@info mail provider", "Gmail");
    }
    if (provider == "outlook"_L1) {
        return i18nc("@info mail provider", "Outlook");
    }
    return i18nc("@info mail provider", "Custom IMAP");
}
