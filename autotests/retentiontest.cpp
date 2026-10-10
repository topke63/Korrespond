// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "retention.h"

using namespace Qt::StringLiterals;

static constexpr qint64 kb = 1024;

class RetentionTest : public QObject
{
    Q_OBJECT

private:
    const QDateTime now = QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::UTC);

    // Storage on, no start date, kept forever, no size limit: nothing is ever removed.
    static StorageSettings keepEverything()
    {
        StorageSettings settings;
        settings.enabled = true;
        settings.keepUnit = StorageSettings::Forever;
        settings.sizeUnit = StorageSettings::Unlimited;
        return settings;
    }

    // Adds an INBOX message daysAgo days before now, size bytes long.
    bool add(MessageStore &store, qint64 uid, int daysAgo, qint64 size)
    {
        MessageSummary summary;
        summary.uid = uid;
        summary.subject = u"Message %1"_s.arg(uid);
        summary.date = now.addDays(-daysAgo);
        return store.addMessage(u"INBOX"_s, 7, summary, QByteArray(size, 'x'));
    }

    static int emlCount(const QString &dir)
    {
        int count = 0;
        QDirIterator it(dir + u"/messages"_s, {u"*.eml"_s}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            ++count;
        }
        return count;
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testTimeCutoff()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(add(store, 1, 40, kb)); // older than 30 days
        QVERIFY(add(store, 2, 31, kb));
        QVERIFY(add(store, 3, 29, kb));
        QVERIFY(add(store, 4, 1, kb));

        StorageSettings settings = keepEverything();
        settings.keepValue = 30;
        settings.keepUnit = StorageSettings::Days;
        const RetentionResult result = applyRetention(store, settings, now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 2);
        QCOMPARE(result.remainingBytes, 2 * kb);
        QVERIFY(!store.contains(u"INBOX"_s, 1));
        QVERIFY(!store.contains(u"INBOX"_s, 2));
        QVERIFY(store.contains(u"INBOX"_s, 3));
        QVERIFY(store.contains(u"INBOX"_s, 4));
        QCOMPARE(emlCount(dir.path()), 2);
    }

    void testImportSinceCutoff()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(add(store, 1, 10, kb));
        QVERIFY(add(store, 2, 2, kb));

        StorageSettings settings = keepEverything();
        settings.importSince = now.date().addDays(-5);
        const RetentionResult result = applyRetention(store, settings, now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 1);
        QVERIFY(store.contains(u"INBOX"_s, 2));
    }

    void testSizeLimit()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        for (int i = 1; i <= 5; ++i) { // uid 1 is the oldest; 5 × 300 KB = 1500 KB
            QVERIFY(add(store, i, 10 - i, 300 * kb));
        }

        StorageSettings settings = keepEverything();
        settings.sizeValue = 1;
        settings.sizeUnit = StorageSettings::MB;
        const RetentionResult result = applyRetention(store, settings, now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 2); // 900 KB left: stops at the first total under the limit
        QCOMPARE(result.remainingBytes, 900 * kb);
        QVERIFY(result.remainingBytes <= settings.maxBytes());
        QVERIFY(!store.contains(u"INBOX"_s, 1));
        QVERIFY(!store.contains(u"INBOX"_s, 2));
        QVERIFY(store.contains(u"INBOX"_s, 3));
        QCOMPARE(emlCount(dir.path()), 3);
    }

    void testSizeLimitExactFit()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(add(store, 1, 2, 512 * kb));
        QVERIFY(add(store, 2, 1, 512 * kb));

        StorageSettings settings = keepEverything();
        settings.sizeUnit = StorageSettings::MB; // exactly 1 MB stored: at the limit is fine
        const RetentionResult result = applyRetention(store, settings, now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 0);
        QCOMPARE(result.remainingBytes, 1024 * kb);
    }

    void testCutoffAndSizeTogether()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(add(store, 1, 60, 100 * kb)); // too old
        QVERIFY(add(store, 2, 20, 600 * kb)); // oldest after the cutoff: over the size limit
        QVERIFY(add(store, 3, 10, 300 * kb));
        QVERIFY(add(store, 4, 1, 300 * kb));

        StorageSettings settings = keepEverything();
        settings.keepValue = 1;
        settings.keepUnit = StorageSettings::Months;
        settings.sizeValue = 1;
        settings.sizeUnit = StorageSettings::MB;
        const RetentionResult result = applyRetention(store, settings, now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 2);
        QCOMPARE(result.remainingBytes, 600 * kb);
        QVERIFY(!store.contains(u"INBOX"_s, 1));
        QVERIFY(!store.contains(u"INBOX"_s, 2));
        QVERIFY(store.contains(u"INBOX"_s, 3));
        QVERIFY(store.contains(u"INBOX"_s, 4));
    }

    void testDisabledClearsEverything()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(add(store, 1, 1, kb));
        QVERIFY(add(store, 2, 2, kb));
        MessageSummary undated;
        undated.uid = 3;
        QVERIFY(store.addMessage(u"Sent"_s, 7, undated, QByteArray(kb, 'y')));

        StorageSettings settings = keepEverything();
        settings.enabled = false;
        const RetentionResult result = applyRetention(store, settings, now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 3);
        QCOMPARE(result.remainingBytes, qint64(0));
        QCOMPARE(emlCount(dir.path()), 0);
    }

    void testForeverUnlimitedKeepsEverything()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(add(store, 1, 3650, 300 * kb));
        QVERIFY(add(store, 2, 1, 300 * kb));

        const RetentionResult result = applyRetention(store, keepEverything(), now);
        QVERIFY(result.ok);
        QCOMPARE(result.removed, 0);
        QCOMPARE(result.remainingBytes, 600 * kb);
        QCOMPARE(emlCount(dir.path()), 2);
    }

    void testStoreNotOpen()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        StorageSettings settings = keepEverything();
        settings.keepUnit = StorageSettings::Days;
        QVERIFY(!applyRetention(store, settings, now).ok);
        settings.enabled = false;
        QVERIFY(!applyRetention(store, settings, now).ok);
    }
};

QTEST_GUILESS_MAIN(RetentionTest)

#include "retentiontest.moc"
