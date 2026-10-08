// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "syncmanager.h"

#include "imapclient.h"
#include "messagestore.h"
#include "retention.h"

#include <algorithm>

using namespace Qt::StringLiterals;

static constexpr int batchSize = 20;

SyncManager::SyncManager(ImapClient *client, MessageStore *store, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_store(store)
{
    connect(m_client, &ImapClient::searchFinished, this, &SyncManager::onSearchFinished);
    connect(m_client, &ImapClient::rawMessageFetched, this, &SyncManager::onRawMessageFetched);
    connect(m_client, &ImapClient::rawFetchFinished, this, &SyncManager::onRawFetchFinished);
    connect(m_client, &ImapClient::errorOccurred, this, &SyncManager::onClientError);
}

void SyncManager::setSettings(const StorageSettings &settings)
{
    m_settings = settings;
}

bool SyncManager::isSyncing() const
{
    return !m_folder.isEmpty();
}

bool SyncManager::isChosen(const FolderInfo &folder)
{
    return folder.role != u"trash"_s && folder.role != u"junk"_s;
}

void SyncManager::syncFolder(const QString &folder)
{
    if (isSyncing()) {
        Q_EMIT errorOccurred(u"A sync is already running."_s);
        return;
    }
    startFolder(folder);
}

void SyncManager::stop()
{
    m_folder.clear();
    m_pending.clear();
    m_queue.clear();
    m_syncingAll = false;
}

void SyncManager::syncAll(const QList<FolderInfo> &folders)
{
    if (isSyncing()) {
        Q_EMIT errorOccurred(u"A sync is already running."_s);
        return;
    }
    m_queue.clear();
    for (const FolderInfo &folder : folders) {
        if (isChosen(folder)) {
            m_queue.append(folder.path);
        }
    }
    m_syncingAll = true;
    if (m_queue.isEmpty()) {
        m_syncingAll = false;
        Q_EMIT syncFinished();
        return;
    }
    startFolder(m_queue.takeFirst());
}

void SyncManager::startFolder(const QString &folder)
{
    m_folder = folder;
    m_pending.clear();
    m_total = m_done = 0;
    m_bytes = 0;
    m_full = false;
    if (!m_settings.enabled) {
        finish(); // retention clears the store
        return;
    }
    m_cutoff = m_settings.cutoff(QDateTime::currentDateTime());
    // SINCE only knows whole days; messages from earlier that day are skipped when they arrive.
    m_client->searchSince(folder, m_cutoff.isValid() ? m_cutoff.date() : QDate());
}

void SyncManager::onSearchFinished(const QString &folder, qint64 uidValidity, const QList<qint64> &uids)
{
    if (folder != m_folder || !m_pending.isEmpty() || m_total > 0) {
        return;
    }
    const qint64 stored = m_store->storedUidValidity(folder);
    if (stored != 0 && stored != uidValidity && m_store->removeFolder(folder) < 0) {
        fail(m_store->lastError());
        return;
    }
    m_uidValidity = uidValidity;
    for (qint64 uid : uids) {
        if (!m_store->contains(folder, uid)) {
            m_pending.append(uid);
        }
    }
    // Higher uids arrived later, so this is newest first.
    std::sort(m_pending.begin(), m_pending.end(), std::greater<>());
    m_total = int(m_pending.size());
    if (m_pending.isEmpty()) {
        finish();
        return;
    }
    Q_EMIT progress(folder, 0, m_total);
    fetchNextBatch();
}

void SyncManager::fetchNextBatch()
{
    if (m_full || m_pending.isEmpty()) {
        finish();
        return;
    }
    const QList<qint64> batch = m_pending.mid(0, batchSize);
    m_pending.remove(0, batch.size());
    m_client->fetchRawMessages(m_folder, batch);
}

void SyncManager::onRawMessageFetched(const QString &folder, const MessageSummary &summary, const QByteArray &raw)
{
    if (folder != m_folder || m_full) {
        return;
    }
    // Arrival date (what SINCE checks) and the Date header can differ; the header decides.
    if (m_cutoff.isValid() && summary.date.isValid() && summary.date < m_cutoff) {
        ++m_done;
        Q_EMIT progress(folder, m_done, m_total);
        return;
    }
    const qint64 maxBytes = m_settings.maxBytes();
    if (maxBytes > 0 && m_bytes + raw.size() > maxBytes) {
        m_full = true;
        return;
    }
    if (!m_store->addMessage(folder, m_uidValidity, summary, raw)) {
        fail(m_store->lastError());
        return;
    }
    m_bytes += raw.size();
    ++m_done;
    Q_EMIT progress(folder, m_done, m_total);
}

void SyncManager::onRawFetchFinished(const QString &folder)
{
    if (folder != m_folder) {
        return;
    }
    fetchNextBatch();
}

void SyncManager::onClientError(const QString &message)
{
    Q_UNUSED(message)
    if (isSyncing()) {
        fail({});
    }
}

void SyncManager::finish()
{
    const RetentionResult result = applyRetention(*m_store, m_settings, QDateTime::currentDateTime());
    const QString folder = m_folder;
    m_folder.clear();
    if (!result.ok) {
        fail(m_store->lastError());
        return;
    }
    Q_EMIT folderSynced(folder);
    if (!m_syncingAll) {
        return;
    }
    if (!m_queue.isEmpty()) {
        startFolder(m_queue.takeFirst());
        return;
    }
    m_syncingAll = false;
    Q_EMIT syncFinished();
}

void SyncManager::fail(const QString &message)
{
    m_folder.clear();
    m_pending.clear();
    m_queue.clear();
    m_syncingAll = false;
    if (message.isEmpty()) {
        Q_EMIT aborted();
    } else {
        Q_EMIT errorOccurred(message);
    }
}
