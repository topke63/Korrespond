// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "contactsync.h"

using namespace Qt::StringLiterals;

namespace
{
constexpr PeopleClient::Source sources[] = {PeopleClient::Connections, PeopleClient::OtherContacts};
}

ContactSync::ContactSync(const QString &account, ContactStore *store, QObject *parent)
    : QObject(parent)
    , m_account(account)
    , m_store(store)
    , m_client(new PeopleClient(this))
    , m_periodic(new QTimer(this))
    , m_retry(new QTimer(this))
{
    m_retry->setSingleShot(true);
    connect(m_retry, &QTimer::timeout, this, [this] {
        fetch(m_retrySource, m_retryPage);
    });
    connect(m_periodic, &QTimer::timeout, this, &ContactSync::start);
    connect(m_client, &PeopleClient::pageFetched, this, &ContactSync::onPage);
    connect(m_client, &PeopleClient::failed, this, &ContactSync::onFailed);
}

ContactSync::~ContactSync() = default;

PeopleClient *ContactSync::client() const
{
    return m_client;
}

void ContactSync::setAccessToken(const QString &accessToken)
{
    m_client->setAccessToken(accessToken);
}

ContactSync::State ContactSync::state() const
{
    return m_state;
}

QString ContactSync::errorText() const
{
    return m_error;
}

QDateTime ContactSync::lastSync() const
{
    return m_lastSync;
}

void ContactSync::setRetryInterval(int milliseconds)
{
    m_retryInterval = milliseconds;
}

ContactStore::Source ContactSync::storeSource(PeopleClient::Source source)
{
    return source == PeopleClient::Connections ? ContactStore::Google : ContactStore::Other;
}

void ContactSync::startPeriodic(int hours)
{
    m_periodic->start(hours * 3600 * 1000);
    start();
}

void ContactSync::stop()
{
    m_periodic->stop();
    m_retry->stop();
    if (m_state == Syncing) {
        m_state = Idle;
        Q_EMIT stateChanged();
    }
}

void ContactSync::start()
{
    if (m_state == Syncing || !m_store || !m_store->isOpen()) {
        return;
    }
    m_sourceIndex = 0;
    m_changed = false;
    m_retried = false;
    m_expiredOnce = false;
    m_state = Syncing;
    m_error.clear();
    Q_EMIT stateChanged();
    m_syncToken = m_store->syncToken(m_account, storeSource(sources[0]));
    fetch(sources[0], {});
}

void ContactSync::fetch(PeopleClient::Source source, const QString &pageToken)
{
    m_retrySource = source;
    m_retryPage = pageToken;
    m_client->fetchPage(source, m_syncToken, pageToken);
}

void ContactSync::onPage(PeopleClient::Source source, const PeoplePage &page)
{
    if (m_state != Syncing) {
        return; // stopped meanwhile
    }
    const ContactStore::Source target = storeSource(source);
    for (const PersonEntry &entry : page.entries) {
        if (entry.deleted || entry.emails.isEmpty()) {
            // Gone, or no address left: nothing to suggest.
            m_store->removePerson(m_account, target, entry.resource);
        } else {
            m_store->upsertPerson(m_account, target, entry.resource, entry.name, entry.emails, entry.photoUrl);
        }
        m_changed = true;
    }
    if (!page.nextPageToken.isEmpty()) {
        fetch(source, page.nextPageToken);
        return;
    }
    // The last page: only now is the new sync state worth keeping.
    if (!page.nextSyncToken.isEmpty()) {
        m_store->setSyncToken(m_account, target, page.nextSyncToken);
    }
    m_expiredOnce = false;
    nextSource();
}

void ContactSync::nextSource()
{
    ++m_sourceIndex;
    if (m_sourceIndex >= int(std::size(sources))) {
        end(Idle, {});
        return;
    }
    m_syncToken = m_store->syncToken(m_account, storeSource(sources[m_sourceIndex]));
    fetch(sources[m_sourceIndex], {});
}

void ContactSync::onFailed(PeopleClient::Source source, PeopleError error, const QString &message)
{
    if (m_state != Syncing) {
        return;
    }
    switch (error) {
    case PeopleError::SyncTokenExpired:
        if (!m_expiredOnce) {
            // Start this source over: what we have of it may be stale in ways the token can't fix.
            m_expiredOnce = true;
            m_store->clearSource(m_account, storeSource(source));
            m_syncToken.clear();
            m_changed = true;
            fetch(source, {});
            return;
        }
        end(Failed, message);
        return;
    case PeopleError::Temporary:
        if (!m_retried) {
            m_retried = true;
            m_retry->start(m_retryInterval);
            return; // still Syncing; the timer repeats the same request
        }
        end(Failed, message);
        return;
    case PeopleError::Unauthorized:
        end(Failed, message);
        Q_EMIT tokenRejected();
        return;
    case PeopleError::MissingScope:
        end(MissingScope, message);
        return;
    case PeopleError::ApiDisabled:
        end(ApiDisabled, message);
        return;
    default:
        end(Failed, message);
        return;
    }
}

void ContactSync::end(State state, const QString &error)
{
    m_state = state;
    m_error = error;
    if (state == Idle) {
        m_lastSync = QDateTime::currentDateTimeUtc();
    }
    Q_EMIT stateChanged();
    Q_EMIT finished(state == Idle, m_changed);
}
