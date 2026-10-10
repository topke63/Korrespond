// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "imapidlewatcher.h"

#include <KIMAP/CapabilitiesJob>
#include <KIMAP/IdleJob>
#include <KIMAP/LoginJob>
#include <KIMAP/SelectJob>
#include <KIMAP/Session>

#include <QLoggingCategory>
#include <QMetaEnum>

using namespace Qt::StringLiterals;

Q_LOGGING_CATEGORY(idleLog, "korrespond.idle")

ImapIdleWatcher::ImapIdleWatcher(QObject *parent)
    : QObject(parent)
    , m_restartInterval(std::chrono::duration_cast<std::chrono::milliseconds>(IdlePolicy::restartInterval()))
{
    m_restartTimer.setSingleShot(true);
    m_reconnectTimer.setSingleShot(true);

    connect(&m_restartTimer, &QTimer::timeout, this, [this] {
        if (m_state == Idling && m_idle) {
            m_restarting = true;
            m_idle->stop(); // its result starts the next IDLE on the same session
        }
    });
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (m_state == Failed) {
            connectNow();
        }
    });
}

ImapIdleWatcher::~ImapIdleWatcher()
{
    ++m_generation;
    dropSession();
}

ImapIdleWatcher::State ImapIdleWatcher::state() const
{
    return m_state;
}

void ImapIdleWatcher::setDelayScaleForTests(double scale)
{
    m_delayScale = scale;
}

void ImapIdleWatcher::setClockForTests(std::function<std::chrono::steady_clock::time_point()> clock)
{
    m_clock = std::move(clock);
}

void ImapIdleWatcher::setRestartIntervalForTests(std::chrono::milliseconds interval)
{
    m_restartInterval = interval;
}

std::chrono::steady_clock::time_point ImapIdleWatcher::now() const
{
    return m_clock ? m_clock() : std::chrono::steady_clock::now();
}

void ImapIdleWatcher::setState(State state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    qCDebug(idleLog) << "state" << QMetaEnum::fromType<State>().valueToKey(state);
    Q_EMIT stateChanged();
}

void ImapIdleWatcher::start(const Account &account, CredentialsProvider provider)
{
    ++m_generation;
    m_restartTimer.stop();
    m_reconnectTimer.stop();
    dropSession();
    if (!provider) {
        qCWarning(idleLog) << "start() without credentials provider";
        setState(Failed); // no timer: nothing is retried
        return;
    }
    m_account = account;
    m_provider = std::move(provider);
    m_policy = IdlePolicy();
    connectNow();
}

void ImapIdleWatcher::stop()
{
    ++m_generation;
    m_restartTimer.stop();
    m_reconnectTimer.stop();
    dropSession();
    setState(Stopped);
}

void ImapIdleWatcher::dropSession()
{
    m_restarting = false;
    if (m_session) {
        m_session->disconnect(this);
        m_session->close();
        m_session->deleteLater();
    }
    m_session = nullptr;
    m_idle = nullptr;
}

void ImapIdleWatcher::connectNow()
{
    const quint64 generation = ++m_generation;
    dropSession();
    setState(Connecting);
    if (generation != m_generation || !m_provider) {
        return;
    }
    m_provider([this, generation](const QString &secret) {
        if (generation != m_generation) {
            return;
        }
        if (secret.isEmpty()) {
            fail(generation);
            return;
        }
        login(secret, generation);
    });
}

void ImapIdleWatcher::fail(quint64 generation)
{
    if (generation != m_generation) {
        return;
    }
    ++m_generation; // late answers of the old session are ignored
    m_restartTimer.stop();
    dropSession();
    m_policy.reportFailure(now());
    const auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(m_policy.nextReconnectDelay() * m_delayScale);
    setState(Failed);
    if (m_state == Failed) {
        m_reconnectTimer.start(std::max(delay, std::chrono::milliseconds(1)));
    }
}

// Same login as ImapClient::session(); duplicated because that one is bound to ImapClient's state.
void ImapIdleWatcher::login(const QString &secret, quint64 generation)
{
    m_connectedPending = true;
    m_session = new KIMAP::Session(m_account.imapHost, m_account.imapPort, this);
    m_session->setTimeout(60);
    connect(m_session, &KIMAP::Session::connectionLost, this, [this, generation] {
        fail(generation);
    });
    connect(m_session, &KIMAP::Session::connectionFailed, this, [this, generation] {
        fail(generation);
    });

    auto login = new KIMAP::LoginJob(m_session);
    login->setUserName(m_account.email);
    login->setPassword(secret);
    login->setEncryptionMode(m_account.imapPort == 143 ? KIMAP::LoginJob::STARTTLS : KIMAP::LoginJob::SSLorTLS);
    login->setAuthenticationMode(m_account.authMethod == "oauth"_L1 ? KIMAP::LoginJob::XOAuth2 : KIMAP::LoginJob::ClearText);
    connect(login, &KJob::result, this, [this, generation](KJob *job) {
        if (generation != m_generation) {
            return;
        }
        if (job->error()) {
            fail(generation);
        }
    });
    login->start();

    auto capabilities = new KIMAP::CapabilitiesJob(m_session);
    connect(capabilities, &KJob::result, this, [this, generation](KJob *job) {
        if (generation != m_generation) {
            return;
        }
        if (job->error()) {
            fail(generation);
            return;
        }
        const QStringList caps = static_cast<KIMAP::CapabilitiesJob *>(job)->capabilities();
        if (!caps.contains(u"IDLE"_s, Qt::CaseInsensitive)) {
            ++m_generation;
            m_restartTimer.stop();
            dropSession();
            setState(Unavailable);
            return;
        }
        // Read-only (EXAMINE): watching must never change flags such as \Recent.
        auto select = new KIMAP::SelectJob(m_session);
        select->setMailBox(u"INBOX"_s);
        select->setOpenReadOnly(true);
        connect(select, &KJob::result, this, [this, generation](KJob *selectJob) {
            if (generation != m_generation) {
                return;
            }
            if (selectJob->error()) {
                fail(generation);
                return;
            }
            startIdle(generation);
        });
        select->start();
    });
    capabilities->start();
}

void ImapIdleWatcher::startIdle(quint64 generation)
{
    if (generation != m_generation || !m_session) {
        return;
    }
    m_restarting = false;
    auto idle = new KIMAP::IdleJob(m_session);
    m_idle = idle;
    connect(idle, &KIMAP::IdleJob::mailBoxStats, this, [this, generation] {
        if (generation == m_generation) {
            Q_EMIT inboxChanged();
        }
    });
    connect(idle, &KIMAP::IdleJob::mailBoxMessageFlagsChanged, this, [this, generation] {
        if (generation == m_generation) {
            Q_EMIT inboxChanged();
        }
    });
    connect(idle, &KJob::result, this, [this, generation, idle](KJob *job) {
        if (generation != m_generation || idle != m_idle) {
            return;
        }
        if (job->error() || !m_restarting) {
            fail(generation); // an IDLE that ends by itself means the connection is not usable
            return;
        }
        startIdle(generation);
    });
    idle->start();
    m_restartTimer.start(m_restartInterval);
    if (m_connectedPending) {
        m_connectedPending = false;
        m_policy.reportConnected(now());
    }
    setState(Idling);
}
