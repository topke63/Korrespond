// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "addresshistory.h"
#include "folderutils.h"
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
    // Runs syncFolder() for each chosen folder in turn (see isChosen()), then emits
    // syncFinished(). The first error stops the run; errorOccurred() or aborted() is then the
    // last signal.
    // Each folder may download up to maxBytes before retention trims the store.
    void syncAll(const QList<FolderInfo> &folders);

    // Ends the running sync without a signal, e.g. before the client disconnects (a disconnect
    // reports no error, so the run would otherwise wait forever). Late results are ignored.
    void stop();

    // Whether syncAll() stores this folder's mail: not Trash, Junk, or Gmail's All Mail, Starred
    // and Important.
    [[nodiscard]] static bool isChosen(const FolderInfo &folder);

Q_SIGNALS:
    // done of total messages downloaded so far in this run.
    void progress(const QString &folder, int done, int total);
    void folderSynced(const QString &folder);
    // A message was stored: the addresses in its headers and when it was sent (ms since the epoch,
    // 0 when it has no date).
    void addressesSeen(const QList<SeenAddress> &addresses, qint64 whenMs);
    void syncFinished();
    // A local storage error stopped the run.
    void errorOccurred(const QString &message);
    // An ImapClient error stopped the run. Not repeated here: whoever uses the client shows it.
    void aborted();

private:
    void onSearchFinished(const QString &folder, qint64 uidValidity, const QList<qint64> &uids);
    void onRawMessageFetched(const QString &folder, const MessageSummary &summary, const QByteArray &raw);
    void onRawFetchFinished(const QString &folder);
    void onClientError(const QString &message);
    void startFolder(const QString &folder);
    void fetchNextBatch();
    void finish();
    // Stops the run; emits errorOccurred(message), or aborted() when message is empty.
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
    QStringList m_queue; // folders syncAll() has still to do
    bool m_syncingAll = false;
};
