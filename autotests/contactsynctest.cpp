// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

#include "contactstore.h"
#include "contactsync.h"

using namespace Qt::StringLiterals;

// Answers requests in order with queued (status, body) pairs and records each request line.
class QueueServer : public QTcpServer
{
    Q_OBJECT

public:
    struct Answer {
        int status;
        QByteArray body;
    };
    QList<Answer> answers;
    QStringList requests;
    QStringList methods;

    QueueServer()
    {
        connect(this, &QTcpServer::newConnection, this, [this] {
            QTcpSocket *socket = nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                const QByteArray request = socket->readAll();
                const QString line = QString::fromLatin1(request.left(request.indexOf("\r\n")));
                requests << line;
                methods << line.section(u' ', 0, 0);
                const Answer answer = answers.isEmpty() ? Answer{500, "{}"} : answers.takeFirst();
                socket->write("HTTP/1.1 " + QByteArray::number(answer.status) + " X\r\nContent-Type: application/json\r\nContent-Length: "
                              + QByteArray::number(answer.body.size()) + "\r\nConnection: close\r\n\r\n" + answer.body);
                socket->disconnectFromHost();
            });
        });
    }

    QUrl url() const
    {
        return QUrl(u"http://127.0.0.1:%1"_s.arg(serverPort()));
    }
};

class ContactSyncTest : public QObject
{
    Q_OBJECT

private:
    static QByteArray person(const QString &id, const QString &name, const QString &email, bool deleted = false)
    {
        QString json = u"{\"resourceName\":\"people/%1\""_s.arg(id);
        if (deleted) {
            json += u",\"metadata\":{\"deleted\":true}"_s;
        } else {
            json += u",\"names\":[{\"displayName\":\"%1\"}],\"emailAddresses\":[{\"value\":\"%2\"}]"_s.arg(name, email);
        }
        return (json + u"}"_s).toUtf8();
    }

    static QByteArray connections(const QList<QByteArray> &people, const QString &pageToken = {}, const QString &syncToken = {})
    {
        QByteArray body = "{\"connections\":[" + people.join(',') + "]";
        if (!pageToken.isEmpty()) {
            body += ",\"nextPageToken\":\"" + pageToken.toUtf8() + "\"";
        }
        if (!syncToken.isEmpty()) {
            body += ",\"nextSyncToken\":\"" + syncToken.toUtf8() + "\"";
        }
        return body + "}";
    }

    static QByteArray others(const QList<QByteArray> &people, const QString &syncToken = {})
    {
        QByteArray body = "{\"otherContacts\":[" + people.join(',').replace("people/", "otherContacts/") + "]";
        if (!syncToken.isEmpty()) {
            body += ",\"nextSyncToken\":\"" + syncToken.toUtf8() + "\"";
        }
        return body + "}";
    }

private Q_SLOTS:
    void testFullSyncWithTwoPages()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{200, connections({person(u"1"_s, u"Alice"_s, u"alice@example.com"_s)}, u"P2"_s)},
                          {200, connections({person(u"2"_s, u"Bob"_s, u"bob@example.com"_s)}, {}, u"SYNC1"_s)},
                          {200, others({person(u"9"_s, u"Zed"_s, u"zed@example.com"_s)}, u"OSYNC1"_s)}};
        ContactSync sync(u"me@gmail.com"_s, &store);
        sync.client()->setBaseUrl(server.url());
        sync.setAccessToken(u"tok"_s);
        QSignalSpy finished(&sync, &ContactSync::finished);
        sync.start();
        QCOMPARE(sync.state(), ContactSync::Syncing);
        QVERIFY(finished.wait(5000));
        QVERIFY(finished.first().at(0).toBool());
        QVERIFY(finished.first().at(1).toBool());
        QCOMPARE(sync.state(), ContactSync::Idle);
        QVERIFY(sync.lastSync().isValid());
        QCOMPARE(store.suggest(u"ali"_s, 8).size(), 1);
        QCOMPARE(store.suggest(u"bob"_s, 8).size(), 1);
        QCOMPARE(store.suggest(u"zed"_s, 8).first().source, int(ContactStore::Other));
        QCOMPARE(store.syncToken(u"me@gmail.com"_s, ContactStore::Google), u"SYNC1"_s);
        QCOMPARE(store.syncToken(u"me@gmail.com"_s, ContactStore::Other), u"OSYNC1"_s);
        QCOMPARE(server.requests.size(), 3);
        QVERIFY(server.requests.at(1).contains(u"pageToken=P2"_s));
        QVERIFY(server.requests.at(2).contains(u"/v1/otherContacts"_s));
        for (const QString &method : std::as_const(server.methods)) {
            QCOMPARE(method, u"GET"_s); // read-only
        }
    }

    void testIncrementalSyncWithDeletion()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        const QString account = u"me@gmail.com"_s;
        QVERIFY(store.upsertPerson(account, ContactStore::Google, u"people/1"_s, u"Alice"_s, {u"alice@example.com"_s}));
        QVERIFY(store.upsertPerson(account, ContactStore::Google, u"people/2"_s, u"Bob"_s, {u"bob@example.com"_s}));
        QVERIFY(store.setSyncToken(account, ContactStore::Google, u"OLD"_s));
        QVERIFY(store.setSyncToken(account, ContactStore::Other, u"OOLD"_s));

        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{200, connections({person(u"2"_s, {}, {}, true), person(u"3"_s, u"Cy"_s, u"cy@example.com"_s)}, {}, u"NEW"_s)},
                          {200, others({}, u"ONEW"_s)}};
        ContactSync sync(account, &store);
        sync.client()->setBaseUrl(server.url());
        QSignalSpy finished(&sync, &ContactSync::finished);
        sync.start();
        QVERIFY(finished.wait(5000));
        QVERIFY(server.requests.at(0).contains(u"syncToken=OLD"_s));
        QVERIFY(server.requests.at(1).contains(u"syncToken=OOLD"_s));
        QVERIFY(store.suggest(u"bob"_s, 8).isEmpty());
        QCOMPARE(store.suggest(u"alice"_s, 8).size(), 1);
        QCOMPARE(store.suggest(u"cy"_s, 8).size(), 1);
        QCOMPARE(store.syncToken(account, ContactStore::Google), u"NEW"_s);
    }

    void testExpiredTokenStartsOver()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        const QString account = u"me@gmail.com"_s;
        QVERIFY(store.upsertPerson(account, ContactStore::Google, u"people/1"_s, u"Stale"_s, {u"stale@example.com"_s}));
        QVERIFY(store.setSyncToken(account, ContactStore::Google, u"OLD"_s));

        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{400, R"({"error":{"message":"Sync token is expired.","details":[{"reason":"EXPIRED_SYNC_TOKEN"}]}})"},
                          {200, connections({person(u"5"_s, u"Fresh"_s, u"fresh@example.com"_s)}, {}, u"S2"_s)},
                          {200, others({}, u"O2"_s)}};
        ContactSync sync(account, &store);
        sync.client()->setBaseUrl(server.url());
        QSignalSpy finished(&sync, &ContactSync::finished);
        sync.start();
        QVERIFY(finished.wait(5000));
        QVERIFY(finished.first().at(0).toBool());
        QVERIFY(store.suggest(u"stale"_s, 8).isEmpty());
        QCOMPARE(store.suggest(u"fresh"_s, 8).size(), 1);
        QVERIFY(!server.requests.at(1).contains(u"syncToken"_s)); // the second request has no token
        QCOMPARE(store.syncToken(account, ContactStore::Google), u"S2"_s);
    }

    void testMissingScopeSetsStateWithoutTouchingStore()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"people/1"_s, u"Keep"_s, {u"keep@example.com"_s}));
        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{403, R"({"error":{"message":"Request had insufficient authentication scopes."}})"}};
        ContactSync sync(u"a"_s, &store);
        sync.client()->setBaseUrl(server.url());
        QSignalSpy finished(&sync, &ContactSync::finished);
        sync.start();
        QVERIFY(finished.wait(5000));
        QVERIFY(!finished.first().at(0).toBool());
        QCOMPARE(sync.state(), ContactSync::MissingScope);
        QVERIFY(!sync.errorText().isEmpty());
        QCOMPARE(store.suggest(u"keep"_s, 8).size(), 1);
        QCOMPARE(server.requests.size(), 1); // did not go on to other contacts
    }

    void testBusyIsRetriedOnceThenWorks()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{429, "{}"}, {200, connections({person(u"1"_s, u"Alice"_s, u"alice@example.com"_s)}, {}, u"S"_s)}, {200, others({}, u"O"_s)}};
        ContactSync sync(u"a"_s, &store);
        sync.setRetryInterval(50);
        sync.client()->setBaseUrl(server.url());
        QSignalSpy finished(&sync, &ContactSync::finished);
        sync.start();
        QVERIFY(finished.wait(5000));
        QVERIFY(finished.first().at(0).toBool());
        QCOMPARE(server.requests.size(), 3);
        QCOMPARE(store.suggest(u"alice"_s, 8).size(), 1);
    }

    void testSecondBusyAnswerEndsTheRun()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{503, "{}"}, {503, "{}"}};
        ContactSync sync(u"a"_s, &store);
        sync.setRetryInterval(20);
        sync.client()->setBaseUrl(server.url());
        QSignalSpy finished(&sync, &ContactSync::finished);
        sync.start();
        QVERIFY(finished.wait(5000));
        QVERIFY(!finished.first().at(0).toBool());
        QCOMPARE(sync.state(), ContactSync::Failed);
        QVERIFY(store.syncToken(u"a"_s, ContactStore::Google).isEmpty());
    }

    void testRejectedTokenAsksForANewOne()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{401, "{}"}};
        ContactSync sync(u"a"_s, &store);
        sync.client()->setBaseUrl(server.url());
        QSignalSpy rejected(&sync, &ContactSync::tokenRejected);
        sync.start();
        QVERIFY(rejected.wait(5000));
        QCOMPARE(sync.state(), ContactSync::Failed);
    }

    void testStopDuringARun()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QueueServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        server.answers = {{200, connections({}, {}, u"S"_s)}, {200, others({}, u"O"_s)}};
        ContactSync sync(u"a"_s, &store);
        sync.client()->setBaseUrl(server.url());
        sync.start();
        sync.stop();
        QCOMPARE(sync.state(), ContactSync::Idle);
        QTest::qWait(300); // the answer arrives and is ignored
        QVERIFY(store.syncToken(u"a"_s, ContactStore::Google).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ContactSyncTest)

#include "contactsynctest.moc"
