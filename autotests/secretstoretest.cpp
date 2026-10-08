// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "secretstore.h"

#include <qt6keychain/keychain.h>

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

// A separate service and key, so the test never touches the app's real "plasma-mail" entries.
static const QString testService = u"plasma-mail-autotest"_s;
static const QString testKey = u"plasma-mail-autotest"_s;
// Generous: the keychain may ask the user for permission the first time.
static constexpr int timeoutMs = 60000;

class SecretStoreTest : public QObject
{
    Q_OBJECT

private:
    QString write(SecretStore &store, const QString &password)
    {
        QSignalSpy spy(&store, &SecretStore::passwordWritten);
        store.writePassword(testKey, password);
        if (!spy.wait(timeoutMs)) {
            return u"timeout"_s;
        }
        if (spy.count() != 1 || spy.first().at(0).toString() != testKey) {
            return u"unexpected signal"_s;
        }
        return spy.first().at(1).toString();
    }

    QList<QVariant> read(SecretStore &store)
    {
        QSignalSpy spy(&store, &SecretStore::passwordRead);
        store.readPassword(testKey);
        if (!spy.wait(timeoutMs)) {
            return {testKey, QString(), u"timeout"_s};
        }
        return spy.first();
    }

    QString remove(SecretStore &store)
    {
        QSignalSpy spy(&store, &SecretStore::passwordDeleted);
        store.deletePassword(testKey);
        if (!spy.wait(timeoutMs)) {
            return u"timeout"_s;
        }
        return spy.first().at(1).toString();
    }

private Q_SLOTS:
    void initTestCase()
    {
        if (!QKeychain::isAvailable()) {
            QSKIP("No keychain backend available (run inside a Plasma session with KWallet or Secret Service)");
        }
    }

    void cleanupTestCase()
    {
        if (QKeychain::isAvailable()) {
            SecretStore store(testService);
            remove(store);
        }
    }

    void testServiceName()
    {
        SecretStore store(testService);
        QCOMPARE(store.service(), testService);
    }

    void testRoundTrip()
    {
        SecretStore store(testService);
        const QString password = u"äpp-pässwörd 🔑 \"quoted\""_s;

        QCOMPARE(write(store, password), QString());

        const QList<QVariant> afterWrite = read(store);
        QCOMPARE(afterWrite.at(0).toString(), testKey);
        QCOMPARE(afterWrite.at(2).toString(), QString());
        QCOMPARE(afterWrite.at(1).toString(), password);

        QCOMPARE(remove(store), QString());

        const QList<QVariant> afterDelete = read(store);
        QVERIFY2(!afterDelete.at(2).toString().isEmpty(), "reading a deleted key must report an error");
        QCOMPARE(afterDelete.at(1).toString(), QString());
    }

    void testOverwrite()
    {
        SecretStore store(testService);
        QCOMPARE(write(store, u"first"_s), QString());
        QCOMPARE(write(store, u"second"_s), QString());
        QCOMPARE(read(store).at(1).toString(), u"second"_s);
        QCOMPARE(remove(store), QString());
    }

    void testDeleteMissingIsSuccess()
    {
        SecretStore store(testService);
        QCOMPARE(remove(store), QString());
        QCOMPARE(remove(store), QString());
    }
};

QTEST_GUILESS_MAIN(SecretStoreTest)
#include "secretstoretest.moc"
