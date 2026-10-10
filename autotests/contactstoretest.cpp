// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTemporaryDir>
#include <QTest>

#include "contactstore.h"

using namespace Qt::StringLiterals;

class ContactStoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testUpsertSuggestAndNameFor()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        const QString account = u"me@gmail.com"_s;
        QVERIFY(store.upsertPerson(account, ContactStore::Google, u"people/c1"_s, u"Alice Example"_s, {u"alice@example.com"_s, u"alice.work@example.org"_s}));
        QVERIFY(store.upsertPerson(account, ContactStore::Google, u"people/c2"_s, u"Bob Builder"_s, {u"bob@example.net"_s}));

        auto list = store.suggest(u"ali"_s, 8);
        QCOMPARE(list.size(), 2); // both addresses of Alice
        QCOMPARE(list.first().email, u"alice@example.com"_s);
        QCOMPARE(list.first().name, u"Alice Example"_s);
        // A later word of the name matches too, and case does not matter.
        QCOMPARE(store.suggest(u"BUIL"_s, 8).size(), 1);
        QCOMPARE(store.suggest(u"bob@"_s, 8).size(), 1);
        QVERIFY(store.suggest(u"xample"_s, 8).isEmpty()); // not a start of a word or the address
        QVERIFY(store.suggest(QString(), 8).isEmpty());
        QCOMPARE(store.suggest(u"a"_s, 1).size(), 1); // limit
        QCOMPARE(store.nameFor(u"ALICE@example.com"_s), u"Alice Example"_s);
        QVERIFY(store.nameFor(u"nobody@example.com"_s).isEmpty());
    }

    void testLikeWildcardsAreLiteral()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"p1"_s, u"Al_ice"_s, {u"al_ice@example.com"_s}));
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"p2"_s, u"Alxice"_s, {u"alxice@example.com"_s}));
        QCOMPARE(store.suggest(u"al_"_s, 8).size(), 1);
        QVERIFY(store.suggest(u"%"_s, 8).isEmpty());
    }

    void testUpsertReplacesAndRemoves()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"p1"_s, u"Alice"_s, {u"a1@example.com"_s, u"a2@example.com"_s}));
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"p1"_s, u"Alice B"_s, {u"a2@example.com"_s})); // a1 is gone
        QVERIFY(store.suggest(u"a1"_s, 8).isEmpty());
        QCOMPARE(store.suggest(u"a2"_s, 8).first().name, u"Alice B"_s);
        QVERIFY(store.removePerson(u"a"_s, ContactStore::Google, u"p1"_s));
        QVERIFY(store.suggest(u"a2"_s, 8).isEmpty());
        QVERIFY(store.removePerson(u"a"_s, ContactStore::Google, u"nope"_s)); // nothing to remove is fine
    }

    void testRankingAndOneEntryPerAddress()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        const QString a = u"me@gmail.com"_s;
        QVERIFY(store.addHistory(a, u"carl@example.com"_s, u"Carl H"_s, 5000));
        QVERIFY(store.addHistory(a, u"carla@example.com"_s, QString(), 9000));
        QVERIFY(store.upsertPerson(a, ContactStore::Other, u"otherContacts/c1"_s, u"Carl Other"_s, {u"carlo@example.com"_s}));
        QVERIFY(store.upsertPerson(a, ContactStore::Google, u"people/c9"_s, QString(), {u"carl@example.com"_s})); // same address, nameless
        const auto list = store.suggest(u"carl"_s, 8);
        QCOMPARE(list.size(), 3);
        QCOMPARE(list.at(0).email, u"carl@example.com"_s); // the Google entry...
        QCOMPARE(list.at(0).source, int(ContactStore::Google));
        QCOMPARE(list.at(0).name, u"Carl H"_s); // ...borrows the name seen in mail
        QCOMPARE(list.at(1).email, u"carlo@example.com"_s); // then Other
        QCOMPARE(list.at(2).email, u"carla@example.com"_s); // then History
    }

    void testHistoryKeepsNewestNameAndTime()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        const QString a = u"me@gmail.com"_s;
        QVERIFY(store.addHistory(a, u"Dan@Example.com"_s, u"Dan Old"_s, 1000));
        QVERIFY(store.addHistory(a, u"dan@example.com"_s, u"Daniel New"_s, 3000));
        QVERIFY(store.addHistory(a, u"dan@example.com"_s, u"Dan Older"_s, 2000)); // older: ignored
        QVERIFY(store.addHistory(a, u"dan@example.com"_s, QString(), 4000)); // newer, no name: keeps the name
        QVERIFY(store.addHistory(a, u"not an address"_s, u"x"_s, 1)); // ignored without error
        const auto list = store.suggest(u"dan"_s, 8);
        QCOMPARE(list.size(), 1);
        QCOMPARE(list.first().name, u"Daniel New"_s);
        QCOMPARE(store.count(ContactStore::History), 1);
    }

    void testClearAndSyncTokens()
    {
        QTemporaryDir dir;
        ContactStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"p1"_s, u"One"_s, {u"one@example.com"_s}));
        QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Other, u"o1"_s, u"Two"_s, {u"two@example.com"_s}));
        QVERIFY(store.upsertPerson(u"b"_s, ContactStore::Google, u"p1"_s, u"Three"_s, {u"three@example.com"_s}));
        QVERIFY(store.setSyncToken(u"a"_s, ContactStore::Google, u"tok"_s));
        QCOMPARE(store.syncToken(u"a"_s, ContactStore::Google), u"tok"_s);
        QVERIFY(store.syncToken(u"a"_s, ContactStore::Other).isEmpty());

        QVERIFY(store.clearSource(u"a"_s, ContactStore::Google));
        QVERIFY(store.suggest(u"one"_s, 8).isEmpty());
        QVERIFY(store.syncToken(u"a"_s, ContactStore::Google).isEmpty());
        QCOMPARE(store.suggest(u"two"_s, 8).size(), 1);
        QVERIFY(store.clearAccount(u"a"_s));
        QVERIFY(store.suggest(u"two"_s, 8).isEmpty());
        QCOMPARE(store.suggest(u"three"_s, 8).size(), 1); // another account stays
        QVERIFY(store.clearAll());
        QVERIFY(store.suggest(u"three"_s, 8).isEmpty());
    }

    void testPersistsAndClosedStoreIsHarmless()
    {
        QTemporaryDir dir;
        {
            ContactStore store(dir.path());
            QVERIFY(store.open());
            QVERIFY(store.upsertPerson(u"a"_s, ContactStore::Google, u"p1"_s, u"Eve"_s, {u"eve@example.com"_s}));
        }
        ContactStore again(dir.path());
        QVERIFY(again.suggest(u"eve"_s, 8).isEmpty()); // not opened yet
        QVERIFY(again.open());
        QCOMPARE(again.suggest(u"eve"_s, 8).size(), 1);

        ContactStore closed(dir.path());
        QVERIFY(!closed.upsertPerson(u"a"_s, ContactStore::Google, u"p"_s, u"x"_s, {u"x@example.com"_s}));
        QVERIFY(!closed.lastError().isEmpty());
        QVERIFY(closed.nameFor(u"eve@example.com"_s).isEmpty());
    }
};

QTEST_GUILESS_MAIN(ContactStoreTest)

#include "contactstoretest.moc"
