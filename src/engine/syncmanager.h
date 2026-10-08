// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "storagesettings.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

class ImapClient;
class MessageStore;
struct MessageSummary;

// Copies mail from the server into the MessageStore within the storage settings. Only reads
// from the server; never changes anything there. One folder at a time.
class SyncManager : public QObject
{
    Q_OBJECT
public:
    // client and store must outlive this object.
    SyncManager(ImapClient *client, MessageStore *store, QObject *parent = nullptr);

    void setSettings(const StorageSettings &settings);
    [[nodiscard]] bool isSyncing() const;

    // Drops the folder's local copies if its UIDVALIDITY changed, downloads messages since the
    // cutoff that aren't stored yet (newest first, in batches), then applies retention.
    // A run downloads at most maxBytes; retention then removes the oldest copies overall, so
    // the store ends up with the newest mail that fits. Emits folderSynced() or errorOccurred().
    void syncFolder(const QString &folder);

Q_SIGNALS:
    // done of total messages downloaded so far in this run.
    void progress(const QString &folder, int done, int total);
    void folderSynced(const QString &folder);
    void errorOccurred(const QString &message);

private:
    void onSearchFinished(const QString &folder, qint64 uidValidity, const QList<qint64> &uids);
    void onRawMessageFetched(const QString &folder, const MessageSummary &summary, const QByteArray &raw);
    void onRawFetchFinished(const QString &folder);
    void onClientError(const QString &message);
    void fetchNextBatch();
    void finish();
    void fail(const QString &message);

    ImapClient *m_client;
    MessageStore *m_store;
    StorageSettings m_settings;

    // State of the running sync; m_folder is empty when idle.
    QString m_folder;
    qint64 m_uidValidity = 0;
    QDateTime m_cutoff;
    QList<qint64> m_pending; // uids still to fetch, newest first
    int m_total = 0;
    int m_done = 0;
    qint64 m_bytes = 0; // downloaded this run
    bool m_full = false; // the next message would pass maxBytes
};
