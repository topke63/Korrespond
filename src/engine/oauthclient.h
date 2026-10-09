// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDateTime>
#include <QObject>
#include <QPointer>

class QOAuth2AuthorizationCodeFlow;
class QOAuthHttpServerReplyHandler;

// Google sign-in for IMAP: authorization code flow with PKCE (S256) in the user's browser,
// redirected to a one-shot HTTP server on 127.0.0.1 (random port). Asks for the full mail
// scope with access_type=offline and prompt=consent, so Google always returns a refresh token.
// Signals never carry tokens in error messages.
class OAuthClient : public QObject
{
    Q_OBJECT
public:
    explicit OAuthClient(QObject *parent = nullptr);
    ~OAuthClient() override;

    // Opens the browser on Google's sign-in page, with `email` pre-filled. Answers with exactly
    // one signedIn() or failed(); a new call cancels a sign-in still waiting for the browser.
    void signIn(const QString &email);

Q_SIGNALS:
    void signedIn(const QString &accessToken, const QString &refreshToken, const QDateTime &expiresAt);
    void failed(const QString &message);

private:
    void finish();
    void fail(const QString &message);

    QOAuth2AuthorizationCodeFlow *m_flow;
    QPointer<QOAuthHttpServerReplyHandler> m_replyHandler;
    bool m_signingIn = false;
};
