// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "account.h"
#include "outgoingmessage.h"

#include <QObject>
#include <QPointer>

namespace KSmtp
{
class Session;
}
class QTimer;

// Sends one message through the account's SMTP server, over an encrypted connection only
// (implicit TLS on port 465, otherwise STARTTLS; there is no unencrypted fallback). Logs in with
// the password, or with the access token (XOAUTH2) for a Google account.
class SmtpClient : public QObject
{
    Q_OBJECT
public:
    explicit SmtpClient(QObject *parent = nullptr);
    ~SmtpClient() override;

    // `secret` is the password, or for an "oauth" account the access token. Answers with exactly
    // one sent() or failed(); a second send() while one is running fails at once.
    void send(const Account &account, const QString &secret, const OutgoingMessage &message);
    [[nodiscard]] bool isSending() const;

Q_SIGNALS:
    // `data` is the message as it went out (for a copy in the Sent folder).
    void sent(const QByteArray &data);
    // A readable text, never containing the password or a token.
    void failed(const QString &message);

private:
    void finish(const QString &error); // an empty error means success
    void login();
    void sendData();

    QPointer<KSmtp::Session> m_session;
    QTimer *m_timeout;
    Account m_account;
    QString m_secret;
    OutgoingMessage::Built m_built;
    bool m_sending = false;
};
