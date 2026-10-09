// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "smtpclient.h"

#include <KLocalizedString>
#include <KSMTP/LoginJob>
#include <KSMTP/SendJob>
#include <KSMTP/Session>

#include <QTimer>

using namespace Qt::StringLiterals;

// Nothing in the exchange should take longer than this.
static constexpr int timeoutMs = 60000;

SmtpClient::SmtpClient(QObject *parent)
    : QObject(parent)
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(timeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        finish(i18n("The mail server did not answer in time."));
    });
}

SmtpClient::~SmtpClient()
{
    if (m_session) {
        m_session->deleteLater();
    }
}

bool SmtpClient::isSending() const
{
    return m_sending;
}

void SmtpClient::send(const Account &account, const QString &secret, const OutgoingMessage &message)
{
    if (m_sending) {
        Q_EMIT failed(i18n("Another message is still being sent."));
        return;
    }
    m_built = message.build(QDateTime::currentDateTime());
    if (m_built.recipients.isEmpty()) {
        Q_EMIT failed(i18n("There is no recipient."));
        return;
    }
    if (account.smtpHost.isEmpty() || account.smtpPort == 0) {
        Q_EMIT failed(i18n("This account has no outgoing mail server."));
        return;
    }
    m_sending = true;
    m_account = account;
    m_secret = secret;
    m_timeout->start();

    m_session = new KSmtp::Session(account.smtpHost, account.smtpPort, this);
    // Encrypted always: port 465 speaks TLS from the first byte, the others upgrade with STARTTLS.
    m_session->setEncryptionMode(account.smtpPort == 465 ? KSmtp::Session::TLS : KSmtp::Session::STARTTLS);
    connect(m_session, &KSmtp::Session::connectionError, this, [this](const QString &error) {
        finish(i18n("Could not connect to %1: %2", m_account.smtpHost, error));
    });
    connect(m_session, &KSmtp::Session::stateChanged, this, [this](KSmtp::Session::State state) {
        if (state == KSmtp::Session::NotAuthenticated) {
            login();
        } else if (state == KSmtp::Session::Disconnected && m_sending) {
            finish(i18n("The connection to %1 was closed.", m_account.smtpHost));
        }
    });
    m_session->open();
}

void SmtpClient::login()
{
    auto job = new KSmtp::LoginJob(m_session);
    job->setUserName(m_account.email);
    job->setPassword(m_secret);
    if (m_account.authMethod == "oauth"_L1) {
        job->setPreferedAuthMode(KSmtp::LoginJob::XOAuth2);
    }
    connect(job, &KJob::result, this, [this](KJob *result) {
        if (!m_sending) {
            return;
        }
        if (result->error() == KSmtp::LoginJob::TokenExpired) {
            finish(i18n("The Google sign-in has expired. Please sign in again."));
        } else if (result->error()) {
            finish(m_account.authMethod == "oauth"_L1 ? i18n("The server did not accept the Google sign-in. Please sign in again.")
                                                      : i18n("The mail server did not accept the email address or password."));
        } else {
            sendData();
        }
    });
    job->start();
}

void SmtpClient::sendData()
{
    auto job = new KSmtp::SendJob(m_session);
    job->setFrom(m_account.email);
    job->setTo(m_built.recipients);
    job->setData(m_built.data);
    connect(job, &KJob::result, this, [this](KJob *result) {
        if (m_sending) {
            finish(result->error() ? i18n("The mail server refused the message: %1", result->errorString()) : QString());
        }
    });
    job->start();
}

void SmtpClient::finish(const QString &error)
{
    if (!m_sending) {
        return;
    }
    m_sending = false;
    m_timeout->stop();
    m_secret.clear();
    const QByteArray data = std::exchange(m_built.data, QByteArray());
    m_built = {};
    if (m_session) {
        m_session->quit();
        m_session->deleteLater();
        m_session.clear();
    }
    if (error.isEmpty()) {
        Q_EMIT sent(data);
    } else {
        Q_EMIT failed(error);
    }
}
