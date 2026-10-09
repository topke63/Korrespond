// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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

OAuthClient::OAuthClient(QObject *parent)
    : QObject(parent)
    , m_flow(new QOAuth2AuthorizationCodeFlow(this))
{
    m_flow->setAuthorizationUrl(QUrl(authorizationUrl));
    m_flow->setTokenUrl(QUrl(tokenUrl));
    m_flow->setRequestedScopeTokens({mailScope});
    m_flow->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);

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
        Q_EMIT signedIn(m_flow->token(), m_flow->refreshToken(), m_flow->expirationAt());
    });
    // A refusal on Google's page (e.g. the user pressed Cancel) arrives here first, then as
    // requestFailed(); fail() reports only the first, more precise message.
    connect(m_flow, &QAbstractOAuth2::serverReportedErrorOccurred, this, [this](const QString &error, const QString &description) {
        if (error == "access_denied"_L1) {
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

void OAuthClient::signIn(const QString &email)
{
    finish();

    const GoogleOAuthSettings settings = GoogleOAuthSettings::load();
    if (!settings.isConfigured()) {
        Q_EMIT failed(i18n("No Google OAuth client is set up. Add ClientId and ClientSecret to the [GoogleOAuth] group of plasmamailrc."));
        return;
    }
    m_flow->setClientIdentifier(settings.clientId);
    m_flow->setClientIdentifierSharedKey(settings.clientSecret);

    m_replyHandler = new QOAuthHttpServerReplyHandler(this);
    m_replyHandler->setCallbackHost(u"127.0.0.1"_s);
    m_replyHandler->setCallbackText(i18n("Plasma Mail received Google's answer. You can close this page and go back to the app."));
    if (!m_replyHandler->listen(QHostAddress::LocalHost, 0)) {
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
    if (!m_signingIn) {
        return;
    }
    finish();
    Q_EMIT failed(message);
}
