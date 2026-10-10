// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "peopleclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

PeopleClient::PeopleClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
    , m_baseUrl(u"https://people.googleapis.com"_s)
{
}

PeopleClient::~PeopleClient() = default;

void PeopleClient::setAccessToken(const QString &accessToken)
{
    m_accessToken = accessToken;
}

void PeopleClient::setBaseUrl(const QUrl &url)
{
    m_baseUrl = url;
}

bool PeopleClient::parsePage(Source source, const QByteArray &json, PeoplePage &page)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    const QJsonObject root = document.object();
    page = {};
    page.nextPageToken = root.value("nextPageToken"_L1).toString();
    page.nextSyncToken = root.value("nextSyncToken"_L1).toString();

    const QJsonArray people = root.value(source == Connections ? "connections"_L1 : "otherContacts"_L1).toArray();
    for (const QJsonValue &value : people) {
        const QJsonObject person = value.toObject();
        PersonEntry entry;
        entry.resource = person.value("resourceName"_L1).toString();
        if (entry.resource.isEmpty()) {
            continue;
        }
        entry.deleted = person.value("metadata"_L1).toObject().value("deleted"_L1).toBool();
        const QJsonArray names = person.value("names"_L1).toArray();
        if (!names.isEmpty()) {
            entry.name = names.first().toObject().value("displayName"_L1).toString();
        }
        // The primary address first; the others in the order Google lists them.
        QStringList others;
        const QJsonArray emails = person.value("emailAddresses"_L1).toArray();
        for (const QJsonValue &emailValue : emails) {
            const QJsonObject email = emailValue.toObject();
            const QString address = email.value("value"_L1).toString().trimmed();
            if (address.isEmpty()) {
                continue;
            }
            if (email.value("metadata"_L1).toObject().value("primary"_L1).toBool()) {
                entry.emails.prepend(address);
            } else {
                others.append(address);
            }
        }
        entry.emails.append(others);
        const QJsonArray photos = person.value("photos"_L1).toArray();
        for (const QJsonValue &photoValue : photos) {
            const QJsonObject photo = photoValue.toObject();
            // A default (generated) picture is not the person's photo.
            if (!photo.value("default"_L1).toBool() && !photo.value("url"_L1).toString().isEmpty()) {
                entry.photoUrl = photo.value("url"_L1).toString();
                break;
            }
        }
        page.entries.append(entry);
    }
    return true;
}

PeopleError PeopleClient::classifyError(int httpStatus, const QByteArray &body)
{
    const QString text = QString::fromUtf8(body).toLower();
    if (httpStatus == 401) {
        return PeopleError::Unauthorized;
    }
    if (httpStatus == 403) {
        if (text.contains(u"service_disabled"_s) || text.contains(u"has not been used"_s) || text.contains(u"is disabled"_s)) {
            return PeopleError::ApiDisabled;
        }
        return PeopleError::MissingScope; // insufficient scopes, or another permission problem
    }
    if (text.contains(u"expired_sync_token"_s) || text.contains(u"sync token is expired"_s) || text.contains(u"expired sync token"_s)) {
        return PeopleError::SyncTokenExpired;
    }
    if (httpStatus == 429 || (httpStatus >= 500 && httpStatus < 600) || httpStatus == 0) {
        return PeopleError::Temporary;
    }
    return PeopleError::Other;
}

void PeopleClient::fetchPage(Source source, const QString &syncToken, const QString &pageToken)
{
    QUrl url(m_baseUrl);
    QUrlQuery query;
    if (source == Connections) {
        url.setPath(u"/v1/people/me/connections"_s);
        query.addQueryItem(u"personFields"_s, u"names,emailAddresses,photos"_s);
    } else {
        url.setPath(u"/v1/otherContacts"_s);
        query.addQueryItem(u"readMask"_s, u"names,emailAddresses,photos"_s);
    }
    query.addQueryItem(u"pageSize"_s, u"1000"_s);
    query.addQueryItem(u"requestSyncToken"_s, u"true"_s);
    if (!syncToken.isEmpty()) {
        query.addQueryItem(u"syncToken"_s, syncToken);
    }
    if (!pageToken.isEmpty()) {
        query.addQueryItem(u"pageToken"_s, pageToken);
    }
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(30000);

    QNetworkReply *reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, source] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        if (status >= 200 && status < 300) {
            PeoplePage page;
            if (parsePage(source, body, page)) {
                Q_EMIT pageFetched(source, page);
            } else {
                Q_EMIT failed(source, PeopleError::Other, u"The contacts answer could not be read."_s);
            }
            return;
        }
        const PeopleError error = classifyError(status, body);
        // The message is ours: nothing from the body (it can echo request details).
        QString message;
        switch (error) {
        case PeopleError::Unauthorized:
            message = u"Google did not accept the access token."_s;
            break;
        case PeopleError::MissingScope:
            message = u"This account has not allowed access to contacts."_s;
            break;
        case PeopleError::ApiDisabled:
            message = u"The People API is not enabled for the Google project of this app."_s;
            break;
        case PeopleError::SyncTokenExpired:
            message = u"The contacts sync state expired."_s;
            break;
        case PeopleError::Temporary:
            message = status == 0 ? u"Could not reach Google."_s : u"Google is busy (HTTP %1)."_s.arg(status);
            break;
        default:
            message = u"Contacts request failed (HTTP %1)."_s.arg(status);
            break;
        }
        Q_EMIT failed(source, error, message);
    });
}
