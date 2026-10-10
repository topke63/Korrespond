// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QSignalSpy>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslServer>
#include <QSslSocket>
#include <QTcpServer>
#include <QTest>

#include "imapidlewatcher.h"

using namespace Qt::StringLiterals;

// Throw-away self-signed certificate for 127.0.0.1, used only by the fake IMAP server below.
// Throw-away self-signed test certificate for 127.0.0.1 only; not a secret.
static const char kTestCertPem[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDGjCCAgKgAwIBAgIUHXOSkyy/xPdt8eJigk6HQTZfxRkwDQYJKoZIhvcNAQEL\n"
    "BQAwFDESMBAGA1UEAwwJMTI3LjAuMC4xMB4XDTI2MTAwOTIwMTE0MloXDTM2MTAw\n"
    "NjIwMTE0MlowFDESMBAGA1UEAwwJMTI3LjAuMC4xMIIBIjANBgkqhkiG9w0BAQEF\n"
    "AAOCAQ8AMIIBCgKCAQEAsr0+3iGTmgfcMhUDkdzSHnScjhi4I7Z8Iz2gsM+0U1jV\n"
    "xahTaoTNyzD6Uq/SEqyJE2vwf1ngYQSgcJrqHrWHXM/TSjyiSKDjoBzfbcnIFwt7\n"
    "C0HRyVYnThnEjX5pT3ontSh33mxdjteOpIUJFLxJFCJTYOsYo351RoAuYEhfa3fp\n"
    "ZqsFXmVhLGGWQPf3DS7cG9qOY1RIDBvbS8L2skbYgHo4bgiADwN/8VhRY309ruCA\n"
    "VaOWkfZ6e7sLXwleJtVXzxZjTQ4LnIVRQVVXs0pLH0pjwg6r+LXZZX1O1yzgKdoy\n"
    "RGnnrkQrvigDw8gWwWa+oBlD3oneeNKX2b8VJBlOjwIDAQABo2QwYjAdBgNVHQ4E\n"
    "FgQULqlBCydCW6Zr4Nh8sPIFr2QYzz0wHwYDVR0jBBgwFoAULqlBCydCW6Zr4Nh8\n"
    "sPIFr2QYzz0wDwYDVR0TAQH/BAUwAwEB/zAPBgNVHREECDAGhwR/AAABMA0GCSqG\n"
    "SIb3DQEBCwUAA4IBAQBDFCN7AjLZtmUdSjkb9kLTvKvdJswCPXwFQnRWKpsAyv8Z\n"
    "miYcxAziB8tcmtGP0BD3ef6Cy8VzJEVSyyt7KyKs+rY/L72LFQxH4CjuQUBqnbLy\n"
    "Ozg3dNDdZNXgubHEFnlifWedE4PiMN7B0peqP9LD09XUzseinqtKV0+RGxbabCuw\n"
    "7fKdVzgNpFcGmLMSchTaPpDzriKFgrssPfUBnosB9KRVxtiUsMPu0hUpxk8UnFpo\n"
    "fZNNHL5UeT0p1r33tq+LOev3+ZX2qw+LJxACkKghgUMB+MDn4zmKDEKV0DH93dEW\n"
    "QXNo1+xqDKX514w4pDwhZ1knw2zEpIwvyBlSEaMa\n"
    "-----END CERTIFICATE-----\n";

static const char kTestKeyPem[] =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQCyvT7eIZOaB9wy\n"
    "FQOR3NIedJyOGLgjtnwjPaCwz7RTWNXFqFNqhM3LMPpSr9ISrIkTa/B/WeBhBKBw\n"
    "muoetYdcz9NKPKJIoOOgHN9tycgXC3sLQdHJVidOGcSNfmlPeie1KHfebF2O146k\n"
    "hQkUvEkUIlNg6xijfnVGgC5gSF9rd+lmqwVeZWEsYZZA9/cNLtwb2o5jVEgMG9tL\n"
    "wvayRtiAejhuCIAPA3/xWFFjfT2u4IBVo5aR9np7uwtfCV4m1VfPFmNNDguchVFB\n"
    "VVezSksfSmPCDqv4tdllfU7XLOAp2jJEaeeuRCu+KAPDyBbBZr6gGUPeid540pfZ\n"
    "vxUkGU6PAgMBAAECggEARViD9CWdwQRsq2eYv3YndVZzyPOROZXausd1uTOUeEO+\n"
    "iJXCa1DEESHa9MqvG7yh4A2gPKCPm8RyRM3tqGVwc5NUG/jDU59MQPoCu3MudLZp\n"
    "EhiCvnT1AemgZu8VB5dSoYfmXb64W6uI9Y6B0/ArACtDIxaDNCdjc6tWktdxYd14\n"
    "T4NfauSMmtLgEggEWUp0xWBgB7I0xlebK92EKlGTzzfBc/zMjlzFl0rrVpiU4Kvt\n"
    "y+2ZCbc1bJLJ5holY4cAkZlfEMay6PYuZAcr+YAkHBO2RJ+fXjibNg83x+dujjbZ\n"
    "H1Sj1ypSP5NG8ZKMVx26DJ1VdV8snoBzdbgv/X+6kQKBgQDxwJ7DFvfdEYaZ9eUx\n"
    "uEERIPzDRe0NjQopCaVuG5EG0+bSTrBJhtTsNhlCSZdUyPeXn6akkFUIkSqMavST\n"
    "BdoYPjicBLZ/OjchRzp3zNtmRSL+CwAd1VHVzNGOecQ1be3NQguZMvKUEpasQzSp\n"
    "dwlgtmMAdwBJp3mPEafLYKOysQKBgQC9Re38ApdzRF3waJzWdvyVHyF1O2oNjSAm\n"
    "Ld8MvbJwmnE141sBqsSf3O3z95TQvzMKsU/7w62lcpv9NIcNjWHXuJifhEAXsQh3\n"
    "Y4yKOQWRq9Y2srQVB20pZqM0gXswF2eFSo9eC0xDH4sicbSG8VSkHDsDIwba/dlD\n"
    "hJojSnjlPwKBgQCIuK8RFkRHukmteOuOGu5xiX5p0XonV8SPQO1WQDDc86pu9iGs\n"
    "85wnmtDeNFBpmt6r2ZadKyNsHO07TKHVJFIbs9TAJ8ouWfYj0+y1JB5Q0Hjhw8bl\n"
    "GGeXyqYZNbRJo0oB84ArvCyEoZ6k/b0iujeLgpqrAKIhBZ4x04JZ+uWZgQKBgQCs\n"
    "iLt5F2+xfmfXh7U4dvymfttD/2YcdkIRK42BEsc/iDmc8uR5r45PX9h1pYG9jB1e\n"
    "KOYNTYHUKJZziVd3DiNbvwU5nBfEwPdDxbZOJc6izK6X6+zmon9q1wn9Ce8elqci\n"
    "RSZNhyT8pxtwyY/T1tfeRXGx2VMde3t65byQYQm1hwKBgDTkR//w/qiedvGfxNNv\n"
    "EACDo4UcjEBKWpaQuRxUlg1CE40MSNjY7Oyu40KpF6VgZ1tit7NiwWJctOKB4CGO\n"
    "OEIwCiYlxhQYsrFHK9Rt04I4Nbvv0Mv+xYD37iDn7qFZbxlbgtHSmgQN2fIzmMq5\n"
    "n4Lk3jeWVN4xg0HOUw9/uNy0\n"
    "-----END PRIVATE KEY-----\n";

// A tiny IMAP4 server over implicit TLS on 127.0.0.1, enough for ImapIdleWatcher: greeting,
// CAPABILITY (with or without IDLE), LOGIN, EXAMINE/SELECT, IDLE and DONE.
class FakeImapServer : public QObject
{
    Q_OBJECT
public:
    explicit FakeImapServer(bool withIdle, QObject *parent = nullptr)
        : QObject(parent)
        , m_withIdle(withIdle)
    {
        QSslConfiguration config;
        config.setLocalCertificate(QSslCertificate(QByteArray(kTestCertPem), QSsl::Pem));
        config.setPrivateKey(QSslKey(QByteArray(kTestKeyPem), QSsl::Rsa));
        m_server.setSslConfiguration(config);
        connect(&m_server, &QTcpServer::pendingConnectionAvailable, this, &FakeImapServer::acceptClient);
    }

    bool listen()
    {
        return m_server.listen(QHostAddress::LocalHost, 0);
    }
    quint16 port() const
    {
        return m_server.serverPort();
    }
    int connections() const
    {
        return m_connections;
    }
    QStringList commands() const
    {
        return m_commands;
    }

    // Unsolicited EXISTS/RECENT pair, as a server sends when mail arrives during IDLE.
    void pushInboxChange()
    {
        send("* 3 EXISTS\r\n* 1 RECENT\r\n");
    }
    // Server side close (no BYE).
    void dropClient()
    {
        if (m_client) {
            m_client->abort();
        }
    }

private:
    void acceptClient()
    {
        auto *socket = static_cast<QSslSocket *>(m_server.nextPendingConnection());
        ++m_connections;
        m_client = socket;
        m_buffer.clear();
        m_idling = false;
        send("* OK fake server ready\r\n");
        connect(socket, &QSslSocket::readyRead, this, [this, socket] {
            m_buffer += socket->readAll();
            int end;
            while ((end = m_buffer.indexOf("\r\n")) >= 0) {
                const QByteArray line = m_buffer.left(end);
                m_buffer.remove(0, end + 2);
                handleLine(line);
            }
        });
        connect(socket, &QSslSocket::disconnected, socket, &QObject::deleteLater);
    }

    void handleLine(const QByteArray &line)
    {
        if (m_idling) {
            if (line.trimmed().toUpper() == "DONE") {
                m_idling = false;
                send(m_idleTag + " OK IDLE terminated\r\n");
            }
            return;
        }
        const QList<QByteArray> parts = line.split(' ');
        if (parts.size() < 2) {
            return;
        }
        const QByteArray tag = parts.at(0);
        const QByteArray name = parts.at(1).toUpper();
        m_commands << QString::fromLatin1(name);
        if (name == "CAPABILITY") {
            send("* CAPABILITY IMAP4rev1" + QByteArray(m_withIdle ? " IDLE" : "") + "\r\n" + tag + " OK CAPABILITY completed\r\n");
        } else if (name == "EXAMINE" || name == "SELECT") {
            send("* FLAGS (\\Seen)\r\n* 2 EXISTS\r\n* 0 RECENT\r\n* OK [UIDVALIDITY 1] ok\r\n* OK [UIDNEXT 3] ok\r\n" + tag + " OK [READ-ONLY] completed\r\n");
        } else if (name == "IDLE") {
            m_idling = true;
            m_idleTag = tag;
            send("+ idling\r\n");
        } else if (name == "LOGOUT") {
            send("* BYE bye\r\n" + tag + " OK LOGOUT completed\r\n");
        } else {
            send(tag + " OK completed\r\n");
        }
    }

    void send(const QByteArray &data)
    {
        if (m_client) {
            m_client->write(data);
        }
    }

    QSslServer m_server;
    QSslSocket *m_client = nullptr;
    bool m_withIdle;
    bool m_idling = false;
    QByteArray m_idleTag;
    QByteArray m_buffer;
    int m_connections = 0;
    QStringList m_commands;
};

class ImapIdleWatcherTest : public QObject
{
    Q_OBJECT

    static quint16 closedPort()
    {
        QTcpServer server;
        server.listen(QHostAddress::LocalHost, 0);
        const quint16 port = server.serverPort();
        server.close();
        return port;
    }

    static Account account()
    {
        Account a;
        a.email = u"test@example.invalid"_s;
        a.imapHost = u"127.0.0.1"_s;
        a.imapPort = closedPort();
        return a;
    }

    static Account accountFor(quint16 port)
    {
        Account a;
        a.email = u"test@example.invalid"_s;
        a.imapHost = u"127.0.0.1"_s;
        a.imapPort = port; // not 143, so implicit TLS is used
        return a;
    }

    static ImapIdleWatcher::CredentialsProvider secret(const QString &value, int *calls = nullptr)
    {
        return [value, calls](std::function<void(const QString &)> done) {
            if (calls) {
                ++*calls;
            }
            done(value);
        };
    }

private Q_SLOTS:
    // Trusts the throw-away certificate for the fake servers only; restored in cleanupTestCase().
    void initTestCase()
    {
        m_savedTlsConfig = QSslConfiguration::defaultConfiguration();
        QSslConfiguration config = m_savedTlsConfig;
        config.setCaCertificates({QSslCertificate(QByteArray(kTestCertPem), QSsl::Pem)});
        QSslConfiguration::setDefaultConfiguration(config);
    }

    void cleanupTestCase()
    {
        QSslConfiguration::setDefaultConfiguration(m_savedTlsConfig);
    }

    void closedPortFails()
    {
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.001);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Stopped);
        QSignalSpy spy(&watcher, &ImapIdleWatcher::stateChanged);
        watcher.start(account(), secret(u"pw"_s));
        QCOMPARE(watcher.state(), ImapIdleWatcher::Connecting);
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Failed, 10000);
        QVERIFY(spy.count() >= 2);
        watcher.stop();
    }

    void retriesAndAsksCredentialsEachTime()
    {
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.01); // 0.3 s, 0.6 s, 1.2 s
        int calls = 0;
        int connecting = 0;
        connect(&watcher, &ImapIdleWatcher::stateChanged, this, [&] {
            if (watcher.state() == ImapIdleWatcher::Connecting) {
                ++connecting;
            }
        });
        watcher.start(account(), secret(u"pw"_s, &calls));
        QTRY_VERIFY_WITH_TIMEOUT(connecting >= 3, 10000);
        QVERIFY(calls >= 3);
        watcher.stop();
    }

    void emptySecretFailsWithoutConnecting()
    {
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(1000); // no retry during the test
        watcher.start(account(), secret(QString()));
        QCOMPARE(watcher.state(), ImapIdleWatcher::Failed);
        QTest::qWait(100);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Failed);
        watcher.stop();
        QCOMPARE(watcher.state(), ImapIdleWatcher::Stopped);
    }

    void stopWhileConnecting()
    {
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.001);
        watcher.start(account(), secret(u"pw"_s));
        QCOMPARE(watcher.state(), ImapIdleWatcher::Connecting);
        QSignalSpy states(&watcher, &ImapIdleWatcher::stateChanged);
        QSignalSpy inbox(&watcher, &ImapIdleWatcher::inboxChanged);
        watcher.stop();
        QCOMPARE(watcher.state(), ImapIdleWatcher::Stopped);
        QCOMPARE(states.count(), 1);
        QTest::qWait(500);
        QCOMPARE(states.count(), 1);
        QCOMPARE(inbox.count(), 0);
        watcher.stop(); // again: nothing
        QCOMPARE(states.count(), 1);
    }

    void stopWhileWaitingForReconnect()
    {
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.01);
        watcher.start(account(), secret(u"pw"_s));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Failed, 10000);
        QSignalSpy states(&watcher, &ImapIdleWatcher::stateChanged);
        watcher.stop();
        QTest::qWait(800);
        QCOMPARE(states.count(), 1);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Stopped);
    }

    void idleReportsInboxChange()
    {
        FakeImapServer server(true);
        QVERIFY(server.listen());
        ImapIdleWatcher watcher;
        QSignalSpy inbox(&watcher, &ImapIdleWatcher::inboxChanged);
        watcher.start(accountFor(server.port()), secret(u"pw"_s));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Idling, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(server.commands().contains(u"IDLE"_s), 5000);
        QVERIFY(server.commands().contains(u"EXAMINE"_s));
        QVERIFY(!server.commands().contains(u"SELECT"_s));
        QCOMPARE(inbox.count(), 0);
        server.pushInboxChange();
        QTRY_VERIFY_WITH_TIMEOUT(inbox.count() >= 1, 5000);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Idling);
        watcher.stop();
        QCOMPARE(watcher.state(), ImapIdleWatcher::Stopped);
    }

    void noIdleCapabilityIsUnavailableWithoutRetry()
    {
        FakeImapServer server(false);
        QVERIFY(server.listen());
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.001);
        watcher.start(accountFor(server.port()), secret(u"pw"_s));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Unavailable, 10000);
        QTest::qWait(300);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Unavailable);
        QCOMPARE(server.connections(), 1);
        QVERIFY(!server.commands().contains(u"IDLE"_s));
        watcher.stop();
    }

    void serverDropReconnectsAndIdlesAgain()
    {
        FakeImapServer server(true);
        QVERIFY(server.listen());
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.01); // first reconnect after 0.3 s
        QSignalSpy inbox(&watcher, &ImapIdleWatcher::inboxChanged);
        watcher.start(accountFor(server.port()), secret(u"pw"_s));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Idling, 10000);
        server.dropClient();
        QTRY_COMPARE_WITH_TIMEOUT(server.connections(), 2, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Idling, 10000);
        server.pushInboxChange();
        QTRY_VERIFY_WITH_TIMEOUT(inbox.count() >= 1, 5000);
        QCOMPARE(server.connections(), 2);
        watcher.stop();
    }

    void emptyProviderFailsWithoutRetry()
    {
        ImapIdleWatcher watcher;
        watcher.setDelayScaleForTests(0.001);
        QSignalSpy states(&watcher, &ImapIdleWatcher::stateChanged);
        watcher.start(account(), ImapIdleWatcher::CredentialsProvider());
        QCOMPARE(watcher.state(), ImapIdleWatcher::Failed);
        QCOMPARE(states.count(), 1);
        QTest::qWait(500);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Failed);
        QCOMPARE(states.count(), 1);
        watcher.stop();
    }

    void idleRestartKeepsConnection()
    {
        FakeImapServer server(true);
        QVERIFY(server.listen());
        ImapIdleWatcher watcher;
        watcher.setRestartIntervalForTests(std::chrono::milliseconds(300));
        QSignalSpy inbox(&watcher, &ImapIdleWatcher::inboxChanged);
        watcher.start(accountFor(server.port()), secret(u"pw"_s));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Idling, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(server.commands().count(u"IDLE"_s) >= 2, 10000);
        QCOMPARE(server.connections(), 1);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Idling);
        const int idles = server.commands().count(u"IDLE"_s);
        QTRY_VERIFY_WITH_TIMEOUT(server.commands().count(u"IDLE"_s) > idles, 10000);
        QCOMPARE(server.connections(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Idling, 5000);
        server.pushInboxChange();
        QTRY_VERIFY_WITH_TIMEOUT(inbox.count() >= 1, 5000);
        QCOMPARE(server.connections(), 1);
        watcher.stop();
    }

    // Opt-in (KORRESPOND_LONG_TESTS=1): KIMAP's default socket timeout is 60 s. A quiet IDLE
    // must not be dropped by it, because KIMAP's IdleJob turns the timeout off while idling.
    void quietIdleSurvivesSessionTimeout()
    {
        if (!qEnvironmentVariableIsSet("KORRESPOND_LONG_TESTS")) {
            QSKIP("set KORRESPOND_LONG_TESTS=1 to run (takes about 70 s)");
        }
        FakeImapServer server(true);
        QVERIFY(server.listen());
        ImapIdleWatcher watcher;
        watcher.start(accountFor(server.port()), secret(u"pw"_s));
        QTRY_COMPARE_WITH_TIMEOUT(watcher.state(), ImapIdleWatcher::Idling, 10000);
        QTest::qWait(70000);
        QCOMPARE(watcher.state(), ImapIdleWatcher::Idling);
        QCOMPARE(server.connections(), 1);
        watcher.stop();
    }

private:
    QSslConfiguration m_savedTlsConfig;
};

QTEST_GUILESS_MAIN(ImapIdleWatcherTest)
#include "imapidlewatchertest.moc"
