// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "oauthclient.h"
#include "oauthsettings.h"

#include <KLocalizedString>

#include <QDesktopServices>
#include <QHostAddress>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>

using namespace Qt::StringLiterals;

static const auto authorizationUrl = u"https://accounts.google.com/o/oauth2/v2/auth"_s;
static const auto tokenUrl = u"https://oauth2.googleapis.com/token"_s;
static const auto mailScope = "https://mail.google.com/"_ba;
static const auto contactsScope = "https://www.googleapis.com/auth/contacts.readonly"_ba;
static const auto otherContactsScope = "https://www.googleapis.com/auth/contacts.other.readonly"_ba;

OAuthClient::OAuthClient(QObject *parent)
    : QObject(parent)
    , m_flow(new QOAuth2AuthorizationCodeFlow(this))
{
    m_flow->setAuthorizationUrl(QUrl(authorizationUrl));
    m_flow->setTokenUrl(QUrl(tokenUrl));
    m_flow->setRequestedScopeTokens({mailScope});
    m_flow->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
    m_flow->setAutoRefresh(true);
    m_flow->setRefreshLeadTime(std::chrono::minutes(5));

    connect(m_flow, &QAbstractOAuth::authorizeWithBrowser, this, [](const QUrl &url) {
        QDesktopServices::openUrl(url);
    });
    connect(m_flow, &QAbstractOAuth::granted, this, [this] {
        if (!m_signingIn) {
            return;
        }
        // Without a refresh token the account couldn't reconnect at the next start.
        if (m_flow->refreshToken().isEmpty()) {
            fail(i18n("Google did not return a refresh token. Please sign in again."));
            return;
        }
        finish();
        m_tokenInUse = true;
        Q_EMIT signedIn(m_flow->token(), m_flow->refreshToken(), m_flow->expirationAt());
    });
    // A refresh sets the token and its expiry one after the other; emit once both are in.
    connect(m_flow, &QAbstractOAuth::tokenChanged, this, [this] {
        if (m_signingIn || !m_tokenInUse || m_tokenEmitPending) {
            return;
        }
        m_tokenEmitPending = true;
        QMetaObject::invokeMethod(
            this,
            [this] {
                m_tokenEmitPending = false;
                if (m_tokenInUse && !m_flow->token().isEmpty()) {
                    Q_EMIT accessTokenChanged(m_flow->token(), m_flow->expirationAt());
                }
            },
            Qt::QueuedConnection);
    });
    // A refusal on Google's page (e.g. the user pressed Cancel) arrives here first, then as
    // requestFailed(); fail() reports only the first, more precise message.
    connect(m_flow, &QAbstractOAuth2::serverReportedErrorOccurred, this, [this](const QString &error, const QString &description) {
        if (error == "invalid_grant"_L1) {
            m_tokenInUse = false;
            fail(i18n("Google no longer accepts this sign-in (it was revoked or expired). Please sign in again."));
        } else if (error == "access_denied"_L1) {
            fail(i18n("Google sign-in was cancelled or access was denied."));
        } else {
            fail(i18n("Google sign-in failed: %1", description.isEmpty() ? error : description));
        }
    });
    connect(m_flow, &QAbstractOAuth::requestFailed, this, [this](QAbstractOAuth::Error error) {
        switch (error) {
        case QAbstractOAuth::Error::NetworkError:
            fail(i18n("Could not reach Google. Check the network connection."));
            break;
        case QAbstractOAuth::Error::OAuthCallbackNotVerified:
            fail(i18n("Google's answer did not match this sign-in. Please try again."));
            break;
        default:
            fail(i18n("Google sign-in failed."));
            break;
        }
    });
}

OAuthClient::~OAuthClient() = default;

QStringList OAuthClient::scopeTokens(bool withContacts)
{
    QStringList scopes = {QString::fromLatin1(mailScope)};
    if (withContacts) {
        scopes << QString::fromLatin1(contactsScope) << QString::fromLatin1(otherContactsScope);
    }
    return scopes;
}

bool OAuthClient::hasContactsScope(const QStringList &scopes)
{
    return scopes.contains(QString::fromLatin1(contactsScope));
}

QStringList OAuthClient::grantedScopes() const
{
    QStringList scopes;
    const auto granted = m_flow->grantedScopeTokens();
    for (const QByteArray &scope : granted) {
        scopes << QString::fromLatin1(scope);
    }
    scopes.sort();
    return scopes;
}

void OAuthClient::signIn(const QString &email, bool withContacts)
{
    finish();
    if (!loadClient()) {
        return;
    }
    QSet<QByteArray> requested;
    for (const QString &scope : scopeTokens(withContacts)) {
        requested.insert(scope.toLatin1());
    }
    m_flow->setRequestedScopeTokens(requested);

    m_replyHandler = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, this);
    m_replyHandler->setCallbackHost(u"127.0.0.1"_s);
    m_replyHandler->setCallbackText(i18n("Korrespond received Google's answer. You can close this page and go back to the app."));
    // The constructor with an address already listens (loopback only, free port).
    if (!m_replyHandler->isListening()) {
        finish();
        Q_EMIT failed(i18n("Could not start the local sign-in listener."));
        return;
    }
    connect(m_replyHandler, &QAbstractOAuthReplyHandler::tokenRequestErrorOccurred, this, [this](QAbstractOAuth::Error, const QString &errorString) {
        fail(i18n("Google did not hand out a token: %1", errorString));
    });
    m_flow->setReplyHandler(m_replyHandler);

    m_flow->setModifyParametersFunction([email](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant> *parameters) {
        if (stage == QAbstractOAuth::Stage::RequestingAuthorization) {
            parameters->replace(u"access_type"_s, u"offline"_s);
            parameters->replace(u"prompt"_s, u"consent"_s);
            parameters->replace(u"login_hint"_s, email);
        }
    });

    m_signingIn = true;
    m_flow->grant();
}

// Gives the flow the user's Google client; reports failed() when there is none.
bool OAuthClient::loadClient()
{
    const GoogleOAuthSettings settings = GoogleOAuthSettings::load();
    if (!settings.isConfigured()) {
        Q_EMIT failed(i18n("No Google OAuth client is set up. Add ClientId and ClientSecret to the [GoogleOAuth] group of korrespondrc."));
        return false;
    }
    m_flow->setClientIdentifier(settings.clientId);
    m_flow->setClientIdentifierSharedKey(settings.clientSecret);
    return true;
}

void OAuthClient::refresh(const QString &refreshToken)
{
    finish();
    m_tokenInUse = false;
    if (refreshToken.isEmpty()) {
        Q_EMIT failed(i18n("There is no saved Google sign-in. Please sign in again."));
        return;
    }
    if (!loadClient()) {
        return;
    }
    m_flow->setRefreshToken(refreshToken);
    m_tokenInUse = true;
    m_flow->refreshTokens();
}

// Stops listening on the loopback port; the flow keeps its tokens.
void OAuthClient::finish()
{
    m_signingIn = false;
    if (m_replyHandler) {
        m_replyHandler->close();
        m_replyHandler->deleteLater();
    }
}

void OAuthClient::fail(const QString &message)
{
    if (!m_signingIn && !m_tokenInUse) {
        return;
    }
    finish();
    Q_EMIT failed(message);
}
