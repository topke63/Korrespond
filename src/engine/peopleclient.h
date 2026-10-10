// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkAccessManager;

// One person (or "other contact") of the People API.
struct PersonEntry {
    QString resource; // "people/c123" or "otherContacts/c456"
    QString name; // display name, may be empty
    QStringList emails; // primary first
    QString photoUrl;
    bool deleted = false; // an incremental sync reports removals this way

    bool operator==(const PersonEntry &) const = default;
};

struct PeoplePage {
    QList<PersonEntry> entries;
    QString nextPageToken; // empty on the last page
    QString nextSyncToken; // only on the last page
};

enum class PeopleError {
    None,
    Unauthorized, // 401: the access token is no good (refresh it)
    MissingScope, // 403: the token has no contacts scope
    ApiDisabled, // 403: the People API is not enabled for the OAuth client's project
    SyncTokenExpired, // the stored sync token is too old: start over
    Temporary, // 429, 5xx, network: try again later
    Other, // anything else; the message says what
};

// Read-only access to Google's People API: the user's contacts ("connections") and "other
// contacts" (addresses Gmail collected). One request at a time per call; the caller pages.
// Never logs tokens; error messages carry no token or contact data.
class PeopleClient : public QObject
{
    Q_OBJECT

public:
    enum Source {
        Connections,
        OtherContacts,
    };
    Q_ENUM(Source)

    explicit PeopleClient(QObject *parent = nullptr);
    ~PeopleClient() override;

    void setAccessToken(const QString &accessToken);
    // The API's root, "https://people.googleapis.com" unless a test sets another.
    void setBaseUrl(const QUrl &url);

    // Asks for one page; answers with exactly one pageFetched() or failed().
    void fetchPage(Source source, const QString &syncToken = {}, const QString &pageToken = {});

    // Pure parts, for tests: the page of a JSON answer (false when it is not valid), and the error
    // class of an HTTP status and body.
    static bool parsePage(Source source, const QByteArray &json, PeoplePage &page);
    static PeopleError classifyError(int httpStatus, const QByteArray &body);

Q_SIGNALS:
    void pageFetched(PeopleClient::Source source, const PeoplePage &page);
    void failed(PeopleClient::Source source, PeopleError error, const QString &message);

private:
    QNetworkAccessManager *m_network;
    QString m_accessToken;
    QUrl m_baseUrl;
};
