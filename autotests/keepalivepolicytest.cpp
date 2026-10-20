// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "keepalivepolicy.h"

using namespace std::chrono;

class KeepAlivePolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testInterval()
    {
        QCOMPARE(KeepAlivePolicy::probeInterval(), minutes(4));
    }

    void testProbesAnIdleConnectionAfterTheInterval()
    {
        QVERIFY(!KeepAlivePolicy::shouldProbe(true, false, seconds(0)));
        QVERIFY(!KeepAlivePolicy::shouldProbe(true, false, minutes(3) + seconds(59)));
        QVERIFY(KeepAlivePolicy::shouldProbe(true, false, minutes(4)));
        QVERIFY(KeepAlivePolicy::shouldProbe(true, false, hours(1)));
    }

    void testNeverProbesWhenNotConnectedOrBusy()
    {
        QVERIFY(!KeepAlivePolicy::shouldProbe(false, false, hours(1)));
        QVERIFY(!KeepAlivePolicy::shouldProbe(true, true, hours(1)));
        QVERIFY(!KeepAlivePolicy::shouldProbe(false, true, hours(1)));
    }
};

QTEST_GUILESS_MAIN(KeepAlivePolicyTest)

#include "keepalivepolicytest.moc"
