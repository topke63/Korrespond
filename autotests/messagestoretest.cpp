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
};

QTEST_GUILESS_MAIN(MessageStoreTest)

#include "messagestoretest.moc"
