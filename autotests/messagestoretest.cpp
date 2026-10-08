// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include "messagestore.h"

using namespace Qt::StringLiterals;

class MessageStoreTest : public QObject
{
    Q_OBJECT

private:
    static MessageSummary summary(qint64 uid, const QString &subject = u"Hello"_s)
    {
        MessageSummary s;
        s.uid = uid;
        s.subject = subject;
        s.from = u"Alice <alice@example.org>"_s;
        s.date = QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::UTC);
        s.unread = true;
        return s;
    }

    // Every .eml file under dir/messages; the store keeps its file names private.
    static QStringList emlFiles(const QString &dir)
    {
        QStringList files;
        QDirIterator it(dir + u"/messages"_s, {u"*.eml"_s}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            files << it.next();
        }
        return files;
    }

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    }

    // Counts index rows through a separate connection, so the test doesn't trust the store.
    static int rowCount(const QString &dir, const QString &folder, qint64 uid)
    {
        int count = -1;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase(u"QSQLITE"_s, u"messagestoretest-check"_s);
            db.setDatabaseName(dir + u"/mail.sqlite"_s);
            if (db.open()) {
                QSqlQuery query(db);
                query.prepare(u"SELECT COUNT(*) FROM messages WHERE folder = ? AND uid = ?"_s);
                query.addBindValue(folder);
                query.addBindValue(uid);
                if (query.exec() && query.next()) {
                    count = query.value(0).toInt();
                }
            }
            db.close();
        }
        QSqlDatabase::removeDatabase(u"messagestoretest-check"_s);
        return count;
    }

    // CRLF line endings, a NUL and high bytes: the file must keep them unchanged.
    static QByteArray rawMessage(const QByteArray &body)
    {
        return QByteArray("From: alice@example.org\r\nSubject: Hello\r\n\r\n") + body + QByteArray("\r\n\0\xff\xfe end\r\n", 11);
    }

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testOpenCreatesDatabase()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString base = dir.filePath(u"store"_s); // doesn't exist yet
        MessageStore store(base);
        QVERIFY(!store.isOpen());
        QVERIFY2(store.open(), qPrintable(store.lastError()));
        QVERIFY(store.isOpen());
        QVERIFY(QFile::exists(base + u"/mail.sqlite"_s));
        QVERIFY(QFileInfo(base + u"/messages"_s).isDir());
        QVERIFY(store.open()); // idempotent
        QCOMPARE(rowCount(base, u"INBOX"_s, 1), 0); // the table exists and is empty
    }

    void testNotOpen()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(!store.contains(u"INBOX"_s, 1));
        QVERIFY(!store.addMessage(u"INBOX"_s, 7, summary(1), rawMessage("x")));
        QVERIFY(!store.lastError().isEmpty());
        QVERIFY(emlFiles(dir.path()).isEmpty());
    }

    void testAddAndContains()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QByteArray raw = rawMessage("Body");
        QVERIFY(!store.contains(u"INBOX"_s, 42));
        QVERIFY2(store.addMessage(u"INBOX"_s, 7, summary(42), raw), qPrintable(store.lastError()));
        QVERIFY(store.contains(u"INBOX"_s, 42));
        QVERIFY(!store.contains(u"INBOX"_s, 43));
        QVERIFY(!store.contains(u"Sent"_s, 42));

        const QStringList files = emlFiles(dir.path());
        QCOMPARE(files.size(), 1);
        QCOMPARE(readFile(files.first()), raw);
        QCOMPARE(rowCount(dir.path(), u"INBOX"_s, 42), 1);
    }

    void testSameUidInTwoFolders()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(1), rawMessage("inbox")));
        QVERIFY(store.addMessage(u"Sent"_s, 7, summary(1), rawMessage("sent")));
        QVERIFY(store.contains(u"INBOX"_s, 1));
        QVERIFY(store.contains(u"Sent"_s, 1));
        QCOMPARE(emlFiles(dir.path()).size(), 2);
    }

    void testUnusualFolderName()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QString folder = u"Archive/2026/Ünïcödé ../x"_s;
        QVERIFY2(store.addMessage(folder, 7, summary(5), rawMessage("x")), qPrintable(store.lastError()));
        QVERIFY(store.contains(folder, 5));
        const QStringList files = emlFiles(dir.path());
        QCOMPARE(files.size(), 1);
        QVERIFY(QFileInfo(files.first()).canonicalFilePath().startsWith(QDir(dir.path()).canonicalPath()));
    }

    void testReAddReplaces()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(42, u"Old"_s), rawMessage("old")));
        const QByteArray newer = rawMessage("new, longer body");
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(42, u"New"_s), newer));
        QCOMPARE(rowCount(dir.path(), u"INBOX"_s, 42), 1);
        const QStringList files = emlFiles(dir.path());
        QCOMPARE(files.size(), 1);
        QCOMPARE(readFile(files.first()), newer);
    }

    void testReAddWithNewUidValidity()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(42), rawMessage("old")));
        const QByteArray newer = rawMessage("new");
        QVERIFY(store.addMessage(u"INBOX"_s, 8, summary(42), newer));
        QCOMPARE(rowCount(dir.path(), u"INBOX"_s, 42), 1);
        const QStringList files = emlFiles(dir.path());
        QCOMPARE(files.size(), 1); // the old UIDVALIDITY's file is gone
        QCOMPARE(readFile(files.first()), newer);
    }

    void testPersistsAcrossReopen()
    {
        QTemporaryDir dir;
        {
            MessageStore store(dir.path());
            QVERIFY(store.open());
            QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(42), rawMessage("x")));
        }
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.contains(u"INBOX"_s, 42));
    }

    void testTwoStoresAtOnce()
    {
        QTemporaryDir a;
        QTemporaryDir b;
        MessageStore storeA(a.path());
        MessageStore storeB(b.path());
        QVERIFY(storeA.open());
        QVERIFY(storeB.open());
        QVERIFY(storeA.addMessage(u"INBOX"_s, 7, summary(1), rawMessage("a")));
        QVERIFY(storeA.contains(u"INBOX"_s, 1));
        QVERIFY(!storeB.contains(u"INBOX"_s, 1));
    }

    void testMessagesNewestFirst()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QDateTime base = QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::UTC);
        MessageSummary older = summary(10, u"Older"_s);
        older.date = base.addDays(-1);
        MessageSummary newest = summary(5, u"Newest"_s); // smaller uid, later date: date wins
        newest.date = base.addDays(1);
        newest.unread = false;
        newest.flagged = true;
        MessageSummary middle = summary(7, u"Middle"_s);
        MessageSummary undated = summary(99, u"Undated"_s);
        undated.date = QDateTime();
        for (const MessageSummary &m : {older, undated, newest, middle}) {
            QVERIFY(store.addMessage(u"INBOX"_s, 7, m, rawMessage("x")));
        }
        QVERIFY(store.addMessage(u"Sent"_s, 7, summary(1, u"Other folder"_s), rawMessage("x")));

        const QList<MessageSummary> all = store.messages(u"INBOX"_s, -1);
        QCOMPARE(all.size(), 4);
        QCOMPARE(all.at(0).subject, u"Newest"_s);
        QCOMPARE(all.at(1).subject, u"Middle"_s);
        QCOMPARE(all.at(2).subject, u"Older"_s);
        QCOMPARE(all.at(3).subject, u"Undated"_s); // no date sorts last
        QVERIFY(!all.at(3).date.isValid());

        // Every field comes back as it was added.
        const MessageSummary &n = all.at(0);
        QCOMPARE(n.uid, qint64(5));
        QCOMPARE(n.from, newest.from);
        QCOMPARE(n.date, newest.date);
        QCOMPARE(n.unread, false);
        QCOMPARE(n.flagged, true);
        QCOMPARE(all.at(1).unread, true);
        QCOMPARE(all.at(1).flagged, false);

        const QList<MessageSummary> two = store.messages(u"INBOX"_s, 2);
        QCOMPARE(two.size(), 2);
        QCOMPARE(two.at(0).uid, qint64(5));
        QCOMPARE(two.at(1).uid, qint64(7));
        QVERIFY(store.messages(u"INBOX"_s, 0).isEmpty());
        QVERIFY(store.messages(u"Drafts"_s, -1).isEmpty());
    }

    void testRawMessage()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QByteArray raw = rawMessage("Body");
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(42), raw));
        QCOMPARE(store.rawMessage(u"INBOX"_s, 42), raw);
        QVERIFY(store.rawMessage(u"INBOX"_s, 43).isEmpty());
        QVERIFY(store.rawMessage(u"Sent"_s, 42).isEmpty());

        // A file deleted behind the store's back reads as empty, not as stale data.
        QVERIFY(QFile::remove(emlFiles(dir.path()).first()));
        QVERIFY(store.rawMessage(u"INBOX"_s, 42).isEmpty());
    }

    void testSetUnread()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(42), rawMessage("x"))); // unread
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(43), rawMessage("y")));

        QVERIFY(store.setUnread(u"INBOX"_s, 42, false));
        QList<MessageSummary> list = store.messages(u"INBOX"_s, -1);
        QCOMPARE(list.size(), 2);
        for (const MessageSummary &m : list) {
            QCOMPARE(m.unread, m.uid != 42); // only 42 changed
        }

        QVERIFY(store.setUnread(u"INBOX"_s, 42, true));
        list = store.messages(u"INBOX"_s, -1);
        for (const MessageSummary &m : list) {
            QVERIFY(m.unread);
        }

        QVERIFY(!store.setUnread(u"INBOX"_s, 44, false)); // not stored
        QVERIFY(!store.lastError().isEmpty());
        QVERIFY(!store.setUnread(u"Sent"_s, 42, false));
    }

    void testListingNotOpen()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.messages(u"INBOX"_s, -1).isEmpty());
        QVERIFY(store.rawMessage(u"INBOX"_s, 1).isEmpty());
        QVERIFY(!store.setUnread(u"INBOX"_s, 1, false));
    }

    void testTotalSize()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QCOMPARE(store.totalSize(), qint64(0));
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(1), QByteArray(100, 'a')));
        QVERIFY(store.addMessage(u"Sent"_s, 7, summary(1), QByteArray(50, 'b')));
        QCOMPARE(store.totalSize(), qint64(150));
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(1), QByteArray(30, 'c'))); // replaced
        QCOMPARE(store.totalSize(), qint64(80));
    }

    void testRemoveOlderThan()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QDateTime cutoff = QDateTime(QDate(2026, 10, 1), QTime(0, 0), QTimeZone::UTC);
        MessageSummary old = summary(1);
        old.date = cutoff.addSecs(-1);
        MessageSummary exact = summary(2);
        exact.date = cutoff;
        MessageSummary undated = summary(3);
        undated.date = QDateTime();
        MessageSummary oldSent = summary(1);
        oldSent.date = cutoff.addDays(-30);
        QVERIFY(store.addMessage(u"INBOX"_s, 7, old, rawMessage("old")));
        QVERIFY(store.addMessage(u"INBOX"_s, 7, exact, rawMessage("exact")));
        QVERIFY(store.addMessage(u"INBOX"_s, 7, undated, rawMessage("undated")));
        QVERIFY(store.addMessage(u"Sent"_s, 7, oldSent, rawMessage("sent")));
        QCOMPARE(emlFiles(dir.path()).size(), 4);

        QCOMPARE(store.removeOlderThan(cutoff), 2);
        QVERIFY(!store.contains(u"INBOX"_s, 1));
        QVERIFY(!store.contains(u"Sent"_s, 1));
        QVERIFY(store.contains(u"INBOX"_s, 2)); // not older than the cutoff itself
        QVERIFY(store.contains(u"INBOX"_s, 3)); // undated messages are kept
        QCOMPARE(rowCount(dir.path(), u"INBOX"_s, 1), 0);
        QCOMPARE(emlFiles(dir.path()).size(), 2);
        QCOMPARE(store.removeOlderThan(cutoff), 0);
    }

    void testRemoveOldestUntil()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QDateTime base = QDateTime(QDate(2026, 10, 8), QTime(12, 0), QTimeZone::UTC);
        for (int i = 1; i <= 4; ++i) { // uid 1 is the oldest, 100 bytes each
            MessageSummary m = summary(i);
            m.date = base.addDays(i);
            QVERIFY(store.addMessage(u"INBOX"_s, 7, m, QByteArray(100, 'a')));
        }
        MessageSummary undated = summary(5);
        undated.date = QDateTime();
        QVERIFY(store.addMessage(u"Sent"_s, 7, undated, QByteArray(100, 'b')));
        QCOMPARE(store.totalSize(), qint64(500));

        QCOMPARE(store.removeOldestUntil(500), 0);
        QCOMPARE(store.removeOldestUntil(250), 3); // uid 1, 2 and 3; undated ones go last
        QVERIFY(!store.contains(u"INBOX"_s, 1));
        QVERIFY(!store.contains(u"INBOX"_s, 2));
        QVERIFY(!store.contains(u"INBOX"_s, 3));
        QVERIFY(store.contains(u"INBOX"_s, 4));
        QVERIFY(store.contains(u"Sent"_s, 5));
        QCOMPARE(store.totalSize(), qint64(200));
        QCOMPARE(emlFiles(dir.path()).size(), 2);

        QCOMPARE(store.removeOldestUntil(0), 2);
        QCOMPARE(store.totalSize(), qint64(0));
        QVERIFY(emlFiles(dir.path()).isEmpty());
    }

    void testRemoveFolderAndUidValidity()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QCOMPARE(store.storedUidValidity(u"INBOX"_s), qint64(0));
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(1), rawMessage("a")));
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(2), rawMessage("b")));
        QVERIFY(store.addMessage(u"Sent"_s, 9, summary(1), rawMessage("c")));
        QCOMPARE(store.storedUidValidity(u"INBOX"_s), qint64(7));
        QCOMPARE(store.storedUidValidity(u"Sent"_s), qint64(9));

        QCOMPARE(store.removeFolder(u"INBOX"_s), 2);
        QVERIFY(store.messages(u"INBOX"_s, -1).isEmpty());
        QCOMPARE(store.storedUidValidity(u"INBOX"_s), qint64(0));
        QVERIFY(store.contains(u"Sent"_s, 1));
        const QStringList left = emlFiles(dir.path());
        QCOMPARE(left.size(), 1);
        QCOMPARE(readFile(left.first()), rawMessage("c"));
        QCOMPARE(store.removeFolder(u"INBOX"_s), 0);
    }

    void testClear()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QVERIFY(store.open());
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(1), rawMessage("a")));
        QVERIFY(store.addMessage(u"Sent"_s, 7, summary(2), rawMessage("b")));
        QFile stray(dir.filePath(u"messages/stray.eml"_s)); // no row points at it
        QVERIFY(stray.open(QIODevice::WriteOnly));
        stray.close();

        QVERIFY(store.clear());
        QCOMPARE(store.totalSize(), qint64(0));
        QCOMPARE(rowCount(dir.path(), u"INBOX"_s, 1), 0);
        QVERIFY(emlFiles(dir.path()).isEmpty());
        // Still usable afterwards.
        QVERIFY(store.addMessage(u"INBOX"_s, 7, summary(3), rawMessage("c")));
        QVERIFY(store.contains(u"INBOX"_s, 3));
    }

    void testFolders()
    {
        QTemporaryDir dir;
        QList<FolderInfo> list;
        {
            MessageStore store(dir.path());
            QVERIFY(store.open());
            QVERIFY(store.folders().isEmpty());
            FolderInfo inbox{u"INBOX"_s, u"INBOX"_s, u"inbox"_s, 0};
            FolderInfo work{u"Work/Projects"_s, u"Projects"_s, QString(), 1}; // null role
            FolderInfo sent{u"Sent"_s, u"Sent"_s, u"sent"_s, 0};
            list = {inbox, sent, work};
            QVERIFY(store.setFolders({inbox}));
            QVERIFY2(store.setFolders(list), qPrintable(store.lastError())); // replaces
        }
        // Survives reopening, in the same order.
        MessageStore store(dir.path());
        QVERIFY(store.open());
        const QList<FolderInfo> stored = store.folders();
        QCOMPARE(stored.size(), 3);
        for (qsizetype i = 0; i < list.size(); ++i) {
            QCOMPARE(stored.at(i).path, list.at(i).path);
            QCOMPARE(stored.at(i).name, list.at(i).name);
            QCOMPARE(stored.at(i).role, list.at(i).role.isNull() ? u""_s : list.at(i).role);
            QCOMPARE(stored.at(i).depth, list.at(i).depth);
        }

        QVERIFY(store.clear());
        QVERIFY(store.folders().isEmpty());
        QVERIFY(store.setFolders({}));
    }

    void testRemovalNotOpen()
    {
        QTemporaryDir dir;
        MessageStore store(dir.path());
        QCOMPARE(store.totalSize(), qint64(0));
        QCOMPARE(store.removeOlderThan(QDateTime::currentDateTimeUtc()), -1);
        QCOMPARE(store.removeOldestUntil(0), -1);
        QCOMPARE(store.removeFolder(u"INBOX"_s), -1);
        QCOMPARE(store.storedUidValidity(u"INBOX"_s), qint64(0));
        QVERIFY(!store.clear());
        QVERIFY(!store.setFolders({}));
        QVERIFY(store.folders().isEmpty());
        QVERIFY(!store.lastError().isEmpty());
    }
};

QTEST_GUILESS_MAIN(MessageStoreTest)

#include "messagestoretest.moc"
