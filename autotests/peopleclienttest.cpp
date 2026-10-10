// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include "peopleclient.h"

using namespace Qt::StringLiterals;

Q_DECLARE_METATYPE(PeoplePage)
Q_DECLARE_METATYPE(PeopleError)
Q_DECLARE_METATYPE(PeopleClient::Source)

// A one-shot HTTP server: answers every request with the configured status and body, and
// records the request lines and Authorization headers it saw.
class FakeServer : public QTcpServer
{
    Q_OBJECT

public:
    int status = 200;
    QByteArray body;
    QStringList requestLines;
    QStringList authorizations;

    FakeServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                const QByteArray request = socket->readAll();
                requestLines << QString::fromLatin1(request.left(request.indexOf("\r\n")));
                for (const QByteArray &line : request.split('\n')) {
                    if (line.toLower().startsWith("authorization:")) {
                        authorizations << QString::fromLatin1(line.trimmed());
                    }
                }
                socket->write("HTTP/1.1 " + QByteArray::number(status) + " X\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
    }

    QUrl url() const
    {
        return QUrl(u"http://127.0.0.1:%1"_s.arg(serverPort()));
    }
};

class PeopleClientTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        qRegisterMetaType<PeoplePage>();
        qRegisterMetaType<PeopleError>();
        qRegisterMetaType<PeopleClient::Source>();
    }

    void testParseConnections()
    {
        const QByteArray json = R"({
          "connections": [
            {"resourceName": "people/c1", "names": [{"displayName": "Alice Example"}],
             "emailAddresses": [{"value": "alice.work@example.com"}, {"value": "alice@example.com", "metadata": {"primary": true}}],
             "photos": [{"url": "https:\u002f\u002fx\u002fdefault.jpg", "default": true}, {"url": "https:\u002f\u002fx\u002fa.jpg"}]},
            {"resourceName": "people/c2", "metadata": {"deleted": true}},
            {"names": [{"displayName": "No resource"}]}
          ],
          "nextPageToken": "p2", "nextSyncToken": "s1"
        })";
        PeoplePage page;
        QVERIFY(PeopleClient::parsePage(PeopleClient::Connections, json, page));
        QCOMPARE(page.entries.size(), 2); // the one without a resource name is dropped
        QCOMPARE(page.nextPageToken, u"p2"_s);
        QCOMPARE(page.nextSyncToken, u"s1"_s);
        const PersonEntry alice = page.entries.at(0);
        QCOMPARE(alice.name, u"Alice Example"_s);
        QCOMPARE(alice.emails, (QStringList{u"alice@example.com"_s, u"alice.work@example.com"_s})); // primary first
        QCOMPARE(alice.photoUrl, u"https://x/a.jpg"_s);
        QVERIFY(!alice.deleted);
        QVERIFY(page.entries.at(1).deleted);
        QVERIFY(page.entries.at(1).emails.isEmpty());
    }

    void testParseOtherContactsAndEmpty()
    {
        PeoplePage page;
        QVERIFY(PeopleClient::parsePage(PeopleClient::OtherContacts,
                                        R"({"otherContacts": [{"resourceName": "otherContacts/c9", "emailAddresses": [{"value": "x@example.org"}]}]})",
                                        page));
        QCOMPARE(page.entries.size(), 1);
        QCOMPARE(page.entries.first().emails, QStringList{u"x@example.org"_s});
        QVERIFY(page.nextSyncToken.isEmpty());

        QVERIFY(PeopleClient::parsePage(PeopleClient::Connections, "{}", page)); // nobody yet
        QVERIFY(page.entries.isEmpty());
        QVERIFY(!PeopleClient::parsePage(PeopleClient::Connections, "not json", page));
        QVERIFY(!PeopleClient::parsePage(PeopleClient::Connections, "[1]", page));
    }

    void testClassifyError()
    {
        QCOMPARE(PeopleClient::classifyError(401, "{}"), PeopleError::Unauthorized);
        QCOMPARE(PeopleClient::classifyError(403, R"({"error":{"status":"PERMISSION_DENIED","message":"Request had insufficient authentication scopes."}})"),
                 PeopleError::MissingScope);
        QCOMPARE(PeopleClient::classifyError(403, R"({"error":{"details":[{"reason":"SERVICE_DISABLED"}]}})"), PeopleError::ApiDisabled);
        QCOMPARE(
            PeopleClient::classifyError(
                400,
                R"({"error":{"message":"Sync token is expired. Clear local cache and retry call without the sync token.","details":[{"reason":"EXPIRED_SYNC_TOKEN"}]}})"),
            PeopleError::SyncTokenExpired);
        QCOMPARE(PeopleClient::classifyError(429, "{}"), PeopleError::Temporary);
        QCOMPARE(PeopleClient::classifyError(503, ""), PeopleError::Temporary);
        QCOMPARE(PeopleClient::classifyError(0, ""), PeopleError::Temporary);
        QCOMPARE(PeopleClient::classifyError(404, "{}"), PeopleError::Other);
    }

    void testFetchPageSendsTokenAndQuery()
    {
        FakeServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = R"({"connections": [{"resourceName": "people/c1", "emailAddresses": [{"value": "a@example.com"}]}], "nextSyncToken": "S"})";

        PeopleClient client;
        client.setBaseUrl(server.url());
        client.setAccessToken(u"tok-123"_s);
        QSignalSpy fetched(&client, &PeopleClient::pageFetched);
        client.fetchPage(PeopleClient::Connections, u"old-sync"_s, u"page-2"_s);
        QVERIFY(fetched.wait(5000));
        const PeoplePage page = fetched.first().at(1).value<PeoplePage>();
        QCOMPARE(page.entries.size(), 1);
        QCOMPARE(page.nextSyncToken, u"S"_s);

        QCOMPARE(server.requestLines.size(), 1);
        const QString line = server.requestLines.first();
        QVERIFY(line.startsWith(u"GET /v1/people/me/connections?"_s));
        QVERIFY(line.contains(u"syncToken=old-sync"_s));
        QVERIFY(line.contains(u"pageToken=page-2"_s));
        QVERIFY(line.contains(u"requestSyncToken=true"_s));
        QCOMPARE(server.authorizations.size(), 1);
        QVERIFY(server.authorizations.first().contains(u"Bearer tok-123"_s));
    }

    void testFetchOtherContactsPath()
    {
        FakeServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.body = R"({"otherContacts": []})";
        PeopleClient client;
        client.setBaseUrl(server.url());
        QSignalSpy fetched(&client, &PeopleClient::pageFetched);
        client.fetchPage(PeopleClient::OtherContacts);
        QVERIFY(fetched.wait(5000));
        QVERIFY(server.requestLines.first().startsWith(u"GET /v1/otherContacts?"_s));
        QVERIFY(server.requestLines.first().contains(u"readMask="_s));
    }

    void testFailureIsReportedWithoutBodyText()
    {
        FakeServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.status = 403;
        server.body = R"({"error":{"message":"Request had insufficient authentication scopes. secret-detail-xyz"}})";
        PeopleClient client;
        client.setBaseUrl(server.url());
        QSignalSpy failed(&client, &PeopleClient::failed);
        client.fetchPage(PeopleClient::Connections);
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(1).value<PeopleError>(), PeopleError::MissingScope);
        QVERIFY(!failed.first().at(2).toString().contains(u"secret-detail"_s));
    }

    void testNetworkErrorIsTemporary()
    {
        quint16 port;
        {
            QTcpServer probe;
            QVERIFY(probe.listen(QHostAddress::LocalHost));
            port = probe.serverPort();
        } // closed again: nothing listens there
        PeopleClient client;
        client.setBaseUrl(QUrl(u"http://127.0.0.1:%1"_s.arg(port)));
        QSignalSpy failed(&client, &PeopleClient::failed);
        client.fetchPage(PeopleClient::Connections);
        QVERIFY(failed.wait(5000));
        QCOMPARE(failed.first().at(1).value<PeopleError>(), PeopleError::Temporary);
    }
};

QTEST_GUILESS_MAIN(PeopleClientTest)

#include "peopleclienttest.moc"
