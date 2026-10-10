// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>
#include <QTimeZone>

#include "storagesettings.h"

using namespace Qt::StringLiterals;

class StorageSettingsTest : public QObject
{
    Q_OBJECT

private:
    static StorageSettings keepFor(int value, StorageSettings::KeepUnit unit)
    {
        StorageSettings settings;
        settings.keepValue = value;
        settings.keepUnit = unit;
        return settings; // importSince null: only keep-for counts
    }

    static QDateTime at(int y, int m, int d)
    {
        return QDateTime(QDate(y, m, d), QTime(12, 0), QTimeZone::UTC);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        KSharedConfig::openConfig(u"korrespondrc"_s)->deleteGroup(u"Storage"_s);
    }

    void testMaxBytes()
    {
        StorageSettings settings;
        settings.sizeValue = 5;
        settings.sizeUnit = StorageSettings::MB;
        QCOMPARE(settings.maxBytes(), 5LL * 1024 * 1024);
        settings.sizeUnit = StorageSettings::GB;
        QCOMPARE(settings.maxBytes(), 5LL * 1024 * 1024 * 1024); // no int overflow
        settings.sizeUnit = StorageSettings::Unlimited;
        QCOMPARE(settings.maxBytes(), qint64(0));
    }

    void testCutoffPerUnit()
    {
        const QDateTime now = at(2026, 10, 8);
        QCOMPARE(keepFor(10, StorageSettings::Days).cutoff(now), at(2026, 9, 28));
        QCOMPARE(keepFor(2, StorageSettings::Weeks).cutoff(now), at(2026, 9, 24));
        QCOMPARE(keepFor(3, StorageSettings::Months).cutoff(now), at(2026, 7, 8));
        QCOMPARE(keepFor(1, StorageSettings::Years).cutoff(now), at(2025, 10, 8));
    }

    void testCutoffMonthEnd()
    {
        QCOMPARE(keepFor(1, StorageSettings::Months).cutoff(at(2026, 3, 31)), at(2026, 2, 28));
        QCOMPARE(keepFor(1, StorageSettings::Months).cutoff(at(2028, 3, 31)), at(2028, 2, 29));
    }

    void testCutoffTakesLater()
    {
        const QDateTime now = at(2026, 10, 8);
        StorageSettings settings = keepFor(1, StorageSettings::Years);

        settings.importSince = QDate(2026, 1, 1); // later than now − 1 year
        QCOMPARE(settings.cutoff(now), QDateTime(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC));

        settings.importSince = QDate(2020, 1, 1); // earlier: keep-for wins
        QCOMPARE(settings.cutoff(now), at(2025, 10, 8));
    }

    void testCutoffOnlyStartDate()
    {
        StorageSettings settings = keepFor(1, StorageSettings::Forever);
        settings.importSince = QDate(2024, 5, 1);
        QCOMPARE(settings.cutoff(at(2026, 10, 8)), QDateTime(QDate(2024, 5, 1), QTime(0, 0), QTimeZone::UTC));
    }

    void testNoCutoff()
    {
        QVERIFY(keepFor(1, StorageSettings::Forever).cutoff(at(2026, 10, 8)).isNull());
    }

    void testDefaults()
    {
        const StorageSettings settings = StorageSettings::defaults(QDate(2026, 10, 8));
        QVERIFY(!settings.enabled);
        QCOMPARE(settings.importSince, QDate(2025, 10, 8));
        QCOMPARE(settings.keepValue, 1);
        QCOMPARE(settings.keepUnit, StorageSettings::Years);
        QCOMPARE(settings.maxBytes(), 1024LL * 1024 * 1024);
    }

    void testLoadWithoutConfigGivesDefaults()
    {
        QCOMPARE(StorageSettings::load(), StorageSettings::defaults(QDate::currentDate()));
    }

    void testSaveLoadRoundTrip()
    {
        StorageSettings settings;
        settings.enabled = true;
        settings.importSince = QDate(2025, 3, 15);
        settings.keepValue = 6;
        settings.keepUnit = StorageSettings::Months;
        settings.sizeValue = 250;
        settings.sizeUnit = StorageSettings::MB;
        settings.save();
        QCOMPARE(StorageSettings::load(), settings);
    }

    void testSaveLoadAllMail()
    {
        StorageSettings settings = StorageSettings::defaults(QDate(2026, 10, 8));
        settings.importSince = QDate(); // all mail must not come back as the default date
        settings.keepUnit = StorageSettings::Forever;
        settings.sizeUnit = StorageSettings::Unlimited;
        settings.save();
        QCOMPARE(StorageSettings::load(), settings);
    }
};

QTEST_GUILESS_MAIN(StorageSettingsTest)

#include "storagesettingstest.moc"
