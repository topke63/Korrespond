// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "addresshistory.h"

using namespace Qt::StringLiterals;

class AddressHistoryTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFromToCcBcc()
    {
        const QByteArray raw =
            "From: Alice Example <alice@example.com>\r\n"
            "To: Bob <bob@example.net>, carol@example.org\r\n"
            "Cc: \"Doe, John\" <john@example.com>, ALICE@example.com\r\n"
            "Bcc: Eve <eve@example.net>\r\n"
            "Subject: Hi\r\n\r\nBody with <fake@example.com> in it\r\n";
        const QList<SeenAddress> list = seenAddressesInRaw(raw);
        QCOMPARE(list.size(), 5); // Alice once, the body is not read
        QCOMPARE(list.at(0), (SeenAddress{u"alice@example.com"_s, u"Alice Example"_s}));
        QCOMPARE(list.at(1), (SeenAddress{u"bob@example.net"_s, u"Bob"_s}));
        QCOMPARE(list.at(2), (SeenAddress{u"carol@example.org"_s, QString()}));
        QCOMPARE(list.at(3).email, u"john@example.com"_s);
        QCOMPARE(list.at(3).name, u"Doe, John"_s);
        QCOMPARE(list.at(4).email, u"eve@example.net"_s);
    }

    void testNoHeadersAndGarbage()
    {
        QVERIFY(seenAddressesInRaw("Subject: nothing\r\n\r\nbody").isEmpty());
        QVERIFY(seenAddressesInRaw(QByteArray()).isEmpty());
        QVERIFY(seenAddressesInRaw("From: not an address\r\n\r\n").isEmpty());
    }

    void testFormatMailbox()
    {
        QCOMPARE(formatMailbox(u"Alice Example"_s, u"alice@example.com"_s), u"Alice Example <alice@example.com>"_s);
        QCOMPARE(formatMailbox(QString(), u"alice@example.com"_s), u"alice@example.com"_s);
        QCOMPARE(formatMailbox(u"  "_s, u"alice@example.com"_s), u"alice@example.com"_s);
        QCOMPARE(formatMailbox(u"Doe, John"_s, u"j@example.com"_s), u"\"Doe, John\" <j@example.com>"_s);
        QCOMPARE(formatMailbox(u"Say \"hi\""_s, u"j@example.com"_s), u"\"Say \\\"hi\\\"\" <j@example.com>"_s);
        // The formatted text reads back as the same address.
        const QList<SeenAddress> back = seenAddressesInRaw(("To: " + formatMailbox(u"Doe, John"_s, u"j@example.com"_s).toUtf8() + "\r\n\r\n"));
        QCOMPARE(back.size(), 1);
        QCOMPARE(back.first().email, u"j@example.com"_s);
        QCOMPARE(back.first().name, u"Doe, John"_s);
    }

    void testAutomatedAddresses()
    {
        QVERIFY(isAutomatedAddress(u"noreply@example.com"_s));
        QVERIFY(isAutomatedAddress(u"No-Reply@shop.example"_s));
        QVERIFY(isAutomatedAddress(u"do-not-reply@bank.example"_s));
        QVERIFY(isAutomatedAddress(u"MAILER-DAEMON@mx.example"_s));
        QVERIFY(!isAutomatedAddress(u"alice@example.com"_s));
        QVERIFY(!isAutomatedAddress(u"replyto@example.com"_s));
        QVERIFY(!isAutomatedAddress(u"info@noreply.example"_s)); // only the local part counts
    }
};

QTEST_GUILESS_MAIN(AddressHistoryTest)

#include "addresshistorytest.moc"
