// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "secretstore.h"

#include <qt6keychain/keychain.h>

#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

// A separate service and key, so the test never touches the app's real "korrespond" entries.
static const QString testService = u"korrespond-autotest"_s;
static const QString testKey = u"korrespond-autotest"_s;
static const QString legacyTestService = u"korrespond-autotest-legacy"_s;
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
            SecretStore legacy(legacyTestService);
            remove(legacy);
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

    // An entry written under the old service name is found, and copied to the current one.
    void testLegacyServiceFallback()
    {
        SecretStore legacy(legacyTestService);
        SecretStore store(testService);
        store.setLegacyService(legacyTestService);
        remove(store);
        QCOMPARE(write(legacy, u"old-secret"_s), QString());

        const QList<QVariant> first = read(store);
        QCOMPARE(first.at(2).toString(), QString());
        QCOMPARE(first.at(1).toString(), u"old-secret"_s);

        // The copy is written in the background: wait until the current service has it.
        SecretStore plain(testService);
        QTRY_COMPARE_WITH_TIMEOUT(read(plain).at(1).toString(), u"old-secret"_s, timeoutMs);

        QCOMPARE(remove(legacy), QString());
        QCOMPARE(remove(store), QString());
    }

    void testLegacyServiceMissingEverywhere()
    {
        SecretStore store(testService);
        store.setLegacyService(legacyTestService);
        remove(store);
        const QList<QVariant> result = read(store);
        QVERIFY(!result.at(2).toString().isEmpty());
        QCOMPARE(result.at(1).toString(), QString());
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
