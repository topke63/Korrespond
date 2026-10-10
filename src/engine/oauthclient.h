// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

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
    // With `withContacts` the sign-in also asks for read-only access to the user's contacts and
    // "other contacts" (People API); without it the grant is the mail scope only, as before.
    void signIn(const QString &email, bool withContacts = false);

    // The scopes Google granted to the last sign-in (empty before one).
    [[nodiscard]] QStringList grantedScopes() const;
    // The scope strings a sign-in asks for.
    [[nodiscard]] static QStringList scopeTokens(bool withContacts);
    // True when the granted scopes include the contacts one.
    [[nodiscard]] static bool hasContactsScope(const QStringList &scopes);

    // Gets a fresh access token from a stored refresh token. Answers with accessTokenChanged() or
    // failed(). From then on, and after a sign-in, the token is renewed by itself about
    // 5 minutes before it expires, each time with accessTokenChanged(). A revoked refresh
    // token gives a failed() that asks the user to sign in again.
    void refresh(const QString &refreshToken);

Q_SIGNALS:
    void signedIn(const QString &accessToken, const QString &refreshToken, const QDateTime &expiresAt);
    void accessTokenChanged(const QString &accessToken, const QDateTime &expiresAt);
    void failed(const QString &message);

private:
    void finish();
    void fail(const QString &message);
    bool loadClient();

    QOAuth2AuthorizationCodeFlow *m_flow;
    QPointer<QOAuthHttpServerReplyHandler> m_replyHandler;
    bool m_signingIn = false;
    bool m_tokenInUse = false; // a refresh token is set; failures of later refreshes are reported
    bool m_tokenEmitPending = false;
};
