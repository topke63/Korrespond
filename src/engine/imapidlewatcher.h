// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "idlepolicy.h"

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <chrono>
#include <functional>

namespace KIMAP
{
class IdleJob;
class Session;
}

// Keeps one own IMAP connection in IDLE on INBOX (read-only) and says when the server reports a
// change. It never reads messages. Reconnects with the delays of IdlePolicy.
class ImapIdleWatcher : public QObject
{
    Q_OBJECT
public:
    enum State {
        Stopped,
        Connecting,
        Idling,
        Unavailable, // the server has no IDLE; no retries
        Failed, // waiting for the next reconnect
    };
    Q_ENUM(State)

    // Asked before every login, so an access token can be refreshed first. An empty secret
    // counts as a failure (retried later).
    using CredentialsProvider = std::function<void(std::function<void(const QString &secret)>)>;

    explicit ImapIdleWatcher(QObject *parent = nullptr);
    ~ImapIdleWatcher() override;

    [[nodiscard]] State state() const;

    // An empty provider is a programming error: the state becomes Failed, nothing connects and
    // nothing is retried.
    void start(const Account &account, CredentialsProvider provider);
    // After this, only the stateChanged() to Stopped is emitted.
    void stop();

    // For tests: multiplies the reconnect delays; replaces the clock given to IdlePolicy.
    void setDelayScaleForTests(double scale);
    void setClockForTests(std::function<std::chrono::steady_clock::time_point()> clock);
    // For tests: how long one IDLE runs before it is restarted (default IdlePolicy::restartInterval()).
    void setRestartIntervalForTests(std::chrono::milliseconds interval);

Q_SIGNALS:
    void stateChanged();
    void inboxChanged();

private:
    void connectNow();
    void login(const QString &secret, quint64 generation);
    void startIdle(quint64 generation);
    void fail(quint64 generation);
    void dropSession();
    void setState(State state);
    [[nodiscard]] std::chrono::steady_clock::time_point now() const;

    State m_state = Stopped;
    Account m_account;
    CredentialsProvider m_provider;
    IdlePolicy m_policy;
    QPointer<KIMAP::Session> m_session;
    QPointer<KIMAP::IdleJob> m_idle;
    QTimer m_restartTimer;
    QTimer m_reconnectTimer;
    quint64 m_generation = 0;
    bool m_restarting = false;
    bool m_connectedPending = false; // set at login, cleared when IdlePolicy::reportConnected() is called
    std::chrono::milliseconds m_restartInterval;
    double m_delayScale = 1.0;
    std::function<std::chrono::steady_clock::time_point()> m_clock;
};
