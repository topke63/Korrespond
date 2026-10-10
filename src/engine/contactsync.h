// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "contactstore.h"
#include "peopleclient.h"

#include <QDateTime>
#include <QObject>
#include <QTimer>

// Keeps one Google account's contacts and "other contacts" in a ContactStore: a full sync when
// there is no sync token (or Google says it expired), incremental syncs after that. Read-only,
// never writes to Google. Every failure is non-fatal: it ends the run with a state and the next
// run (a timer, or the user) tries again; mail is never involved.
class ContactSync : public QObject
{
    Q_OBJECT

public:
    enum State {
        Idle, // not run yet, or the last run worked
        Syncing,
        MissingScope, // the account has not allowed access to contacts
        ApiDisabled, // the People API is off in the Google project
        Failed, // another error; errorText() says which
    };
    Q_ENUM(State)

    // `store` stays owned by the caller and must outlive this object.
    ContactSync(const QString &account, ContactStore *store, QObject *parent = nullptr);
    ~ContactSync() override;

    // The People API (tests point it at a fake server).
    [[nodiscard]] PeopleClient *client() const;
    void setAccessToken(const QString &accessToken);

    // Starts a run; ignored while one runs. Both sources, one after the other.
    void start();
    // Runs start() now and then every `hours` hours (default 6) until stop().
    void startPeriodic(int hours = 6);
    void stop();
    // Temporary failures are retried once after this long (default 60 s).
    void setRetryInterval(int milliseconds);

    [[nodiscard]] State state() const;
    [[nodiscard]] QString errorText() const;
    [[nodiscard]] QDateTime lastSync() const; // the last run that finished without error

Q_SIGNALS:
    void stateChanged();
    // The access token was refused (401): refresh it with setAccessToken(), then start() again.
    void tokenRejected();
    // A run ended; `changed` is true when the store has other content now.
    void finished(bool ok, bool changed);

private:
    void fetch(PeopleClient::Source source, const QString &pageToken);
    void onPage(PeopleClient::Source source, const PeoplePage &page);
    void onFailed(PeopleClient::Source source, PeopleError error, const QString &message);
    void nextSource();
    void end(State state, const QString &error);
    [[nodiscard]] static ContactStore::Source storeSource(PeopleClient::Source source);

    QString m_account;
    ContactStore *m_store;
    PeopleClient *m_client;
    QTimer *m_periodic;
    QTimer *m_retry;
    State m_state = Idle;
    QString m_error;
    QDateTime m_lastSync;
    int m_retryInterval = 60000;
    int m_sourceIndex = 0; // 0 = connections, 1 = other contacts
    bool m_changed = false;
    bool m_retried = false; // the temporary-failure retry of this run was used
    bool m_expiredOnce = false; // this source already restarted after an expired token
    PeopleClient::Source m_retrySource = PeopleClient::Connections;
    QString m_retryPage;
    QString m_syncToken; // of the source being synced, "" for a full sync
};
