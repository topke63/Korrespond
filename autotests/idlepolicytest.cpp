// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QTest>

#include "idlepolicy.h"

using namespace std::chrono;

class IdlePolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void restartInterval()
    {
        QCOMPARE(IdlePolicy::restartInterval(), minutes(20));
    }

    void delaySequence()
    {
        IdlePolicy policy;
        for (int expected : {30, 60, 120, 300, 300, 300}) {
            QCOMPARE(policy.nextReconnectDelay(), seconds(expected));
        }
    }

    void resetAfterStable()
    {
        IdlePolicy policy;
        const auto t = steady_clock::now();
        policy.nextReconnectDelay();
        policy.nextReconnectDelay();
        policy.reportConnected(t);
        policy.reportFailure(t + minutes(3));
        QCOMPARE(policy.nextReconnectDelay(), seconds(30));
        QCOMPARE(policy.nextReconnectDelay(), seconds(60));
    }

    void noResetWhenUnstable()
    {
        IdlePolicy policy;
        const auto t = steady_clock::now();
        QCOMPARE(policy.nextReconnectDelay(), seconds(30));
        policy.reportConnected(t);
        policy.reportFailure(t + seconds(10));
        QCOMPARE(policy.nextReconnectDelay(), seconds(60));
    }

    void failureWithoutConnection()
    {
        IdlePolicy policy;
        const auto t = steady_clock::now();
        policy.nextReconnectDelay();
        policy.reportFailure(t + hours(1));
        QCOMPARE(policy.nextReconnectDelay(), seconds(60));
    }

    void unstableCyclesGrowToCap()
    {
        IdlePolicy policy;
        auto t = steady_clock::now();
        seconds last{0};
        for (int i = 0; i < 8; ++i) {
            policy.reportConnected(t);
            t += seconds(30);
            policy.reportFailure(t);
            last = policy.nextReconnectDelay();
            t += last;
        }
        QCOMPARE(last, seconds(300));
    }

    void pollInterval_data()
    {
        QTest::addColumn<bool>("idling");
        QTest::addColumn<int>("check");
        QTest::addColumn<int>("expected");
        QTest::newRow("idling") << true << 1 << 15;
        QTest::newRow("polling 1") << false << 1 << 1;
        QTest::newRow("polling 0") << false << 0 << 1;
        QTest::newRow("polling negative") << false << -5 << 1;
        QTest::newRow("polling 30") << false << 30 << 30;
    }

    void pollInterval()
    {
        QFETCH(bool, idling);
        QFETCH(int, check);
        QFETCH(int, expected);
        QCOMPARE(IdlePolicy::pollInterval(idling, check), minutes(expected));
    }
};

QTEST_GUILESS_MAIN(IdlePolicyTest)

#include "idlepolicytest.moc"
