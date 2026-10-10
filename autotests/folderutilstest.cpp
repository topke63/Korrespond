// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "folderutils.h"
#include "syncmanager.h"

#include <QTest>

using namespace Qt::StringLiterals;

// Define comparison operator for FolderInfo
inline bool operator==(const FolderInfo &a, const FolderInfo &b)
{
    return a.path == b.path && a.name == b.name && a.role == b.role && a.depth == b.depth;
}

class FolderUtilsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testFolderRoleFromFlags_data()
    {
        QTest::addColumn<QString>("path");
        QTest::addColumn<QList<QByteArray>>("flags");
        QTest::addColumn<QString>("expectedRole");

        // Test INBOX folder
        QTest::newRow("inbox") << u"INBOX"_s << QList<QByteArray>() << u"inbox"_s;

        // Test standard special-use flags
        QTest::newRow("sent") << u"Sent"_s << QList<QByteArray>{u"\\sent"_s.toUtf8()} << u"sent"_s;

        QTest::newRow("drafts") << u"Drafts"_s << QList<QByteArray>{u"\\drafts"_s.toUtf8()} << u"drafts"_s;

        QTest::newRow("trash") << u"Trash"_s << QList<QByteArray>{u"\\trash"_s.toUtf8()} << u"trash"_s;

        QTest::newRow("junk") << u"Junk"_s << QList<QByteArray>{u"\\junk"_s.toUtf8()} << u"junk"_s;

        QTest::newRow("archive") << u"Archive"_s << QList<QByteArray>{u"\\archive"_s.toUtf8()} << u"archive"_s;

        QTest::newRow("all") << u"All"_s << QList<QByteArray>{u"\\all"_s.toUtf8()} << u"all"_s;

        QTest::newRow("flagged") << u"Flagged"_s << QList<QByteArray>{u"\\flagged"_s.toUtf8()} << u"flagged"_s;

        // Test uppercase flags (real IMAP servers send these)
        QTest::newRow("sent_upper") << u"Sent"_s << QList<QByteArray>{u"\\SENT"_s.toUtf8()} << u"sent"_s;

        QTest::newRow("drafts_upper") << u"Drafts"_s << QList<QByteArray>{u"\\DRAFTS"_s.toUtf8()} << u"drafts"_s;

        QTest::newRow("sent_mixed") << u"Sent"_s << QList<QByteArray>{u"\\Sent"_s.toUtf8()} << u"sent"_s;

        // Test non-special folders
        QTest::newRow("normal_folder") << u"Work/Projects"_s << QList<QByteArray>{u"\\foo"_s.toUtf8()} << QString();
        QTest::newRow("no_flags") << u"Custom"_s << QList<QByteArray>() << QString();

        // Test lowercase inbox path
        QTest::newRow("inbox_lowercase") << u"inbox"_s << QList<QByteArray>() << u"inbox"_s;

        // Gmail: special folders live under [Gmail]/ and carry other flags as well
        QTest::newRow("gmail_all") << u"[Gmail]/All Mail"_s << QList<QByteArray>{"\\HasNoChildren", "\\All"} << u"all"_s;
        QTest::newRow("gmail_important") << u"[Gmail]/Important"_s << QList<QByteArray>{"\\HasNoChildren", "\\Important"} << u"important"_s;
        QTest::newRow("gmail_starred") << u"[Gmail]/Starred"_s << QList<QByteArray>{"\\HasNoChildren", "\\Flagged"} << u"flagged"_s;
        QTest::newRow("gmail_sent") << u"[Gmail]/Sent Mail"_s << QList<QByteArray>{"\\HasNoChildren", "\\Sent"} << u"sent"_s;
        QTest::newRow("gmail_spam") << u"[Gmail]/Spam"_s << QList<QByteArray>{"\\HasNoChildren", "\\Junk"} << u"junk"_s;
        QTest::newRow("gmail_label") << u"Receipts"_s << QList<QByteArray>{"\\HasNoChildren"} << QString();
    }

    void testFolderRoleFromFlags()
    {
        QFETCH(QString, path);
        QFETCH(QList<QByteArray>, flags);
        QFETCH(QString, expectedRole);

        QCOMPARE(folderRoleFromFlags(path, flags), expectedRole);
    }

    void testSortFolders_data()
    {
        QTest::addColumn<QList<FolderInfo>>("input");
        QTest::addColumn<QList<FolderInfo>>("expected");

        // Test sorting with special folders
        FolderInfo inbox{u"INBOX"_s, u"Inbox"_s, u"inbox"_s};
        FolderInfo flagged{u"Flagged"_s, u"Flagged"_s, u"flagged"_s};
        FolderInfo drafts{u"Drafts"_s, u"Drafts"_s, u"drafts"_s};
        FolderInfo sent{u"Sent"_s, u"Sent"_s, u"sent"_s};
        FolderInfo archive{u"Archive"_s, u"Archive"_s, u"archive"_s};
        FolderInfo all{u"All"_s, u"All"_s, u"all"_s};
        FolderInfo junk{u"Junk"_s, u"Junk"_s, u"junk"_s};
        FolderInfo trash{u"Trash"_s, u"Trash"_s, u"trash"_s};
        FolderInfo custom1{u"Custom1"_s, u"Custom1"_s, QString()};
        FolderInfo custom2{u"Custom2"_s, u"Custom2"_s, QString()};

        // Test all special folders in order
        QList<FolderInfo> specialFolders;
        specialFolders << archive << junk << drafts << flagged << inbox << sent << trash << all;
        QList<FolderInfo> expectedSpecialFolders;
        expectedSpecialFolders << inbox << flagged << drafts << sent << archive << all << junk << trash;
        QTest::newRow("special_folders") << specialFolders << expectedSpecialFolders;

        // Test mixed case
        QList<FolderInfo> mixedCase;
        mixedCase << custom2 << inbox << custom1 << sent;
        QList<FolderInfo> expectedMixedCase;
        expectedMixedCase << inbox << sent << custom1 << custom2;
        QTest::newRow("mixed_case") << mixedCase << expectedMixedCase;

        // Test alphabetical for non-special folders
        QList<FolderInfo> alphabetical;
        alphabetical << custom2 << custom1 << inbox;
        QList<FolderInfo> expectedAlphabetical;
        expectedAlphabetical << inbox << custom1 << custom2;
        QTest::newRow("alphabetical") << alphabetical << expectedAlphabetical;

        // Test case-insensitive sorting (e.g. apple vs Banana)
        FolderInfo apple{u"apple"_s, u"apple"_s, QString()};
        FolderInfo banana{u"Banana"_s, u"Banana"_s, QString()};
        QList<FolderInfo> caseInsensitiveSort;
        caseInsensitiveSort << banana << apple;
        QList<FolderInfo> expectedCaseInsensitiveSort;
        expectedCaseInsensitiveSort << apple << banana;
        QTest::newRow("case_insensitive_sort") << caseInsensitiveSort << expectedCaseInsensitiveSort;
    }

    void testSortFolders()
    {
        QFETCH(QList<FolderInfo>, input);
        QFETCH(QList<FolderInfo>, expected);

        QList<FolderInfo> folders = input;
        sortFolders(folders);

        QCOMPARE(folders, expected);
    }

    void testSortImportantAfterAllMail()
    {
        QList<FolderInfo> folders = {
            {u"[Gmail]/Trash"_s, u"Trash"_s, u"trash"_s, 1},
            {u"[Gmail]/Important"_s, u"Important"_s, u"important"_s, 1},
            {u"[Gmail]/Spam"_s, u"Spam"_s, u"junk"_s, 1},
            {u"[Gmail]/All Mail"_s, u"All Mail"_s, u"all"_s, 1},
        };
        sortFolders(folders);
        QStringList roles;
        for (const FolderInfo &folder : folders) {
            roles << folder.role;
        }
        QCOMPARE(roles, (QStringList{u"all"_s, u"important"_s, u"junk"_s, u"trash"_s}));
    }

    void testWhichFoldersAreStored_data()
    {
        QTest::addColumn<QString>("role");
        QTest::addColumn<bool>("stored");

        QTest::newRow("inbox") << u"inbox"_s << true;
        QTest::newRow("sent") << u"sent"_s << true;
        QTest::newRow("drafts") << u"drafts"_s << true;
        QTest::newRow("archive") << u"archive"_s << true;
        QTest::newRow("plain folder") << QString() << true;
        QTest::newRow("trash") << u"trash"_s << false;
        QTest::newRow("junk") << u"junk"_s << false;
        // Gmail folders that only repeat mail from other folders
        QTest::newRow("all") << u"all"_s << false;
        QTest::newRow("flagged") << u"flagged"_s << false;
        QTest::newRow("important") << u"important"_s << false;
    }

    void testWhichFoldersAreStored()
    {
        QFETCH(QString, role);
        QFETCH(bool, stored);

        FolderInfo folder;
        folder.path = u"Some folder"_s;
        folder.role = role;
        QCOMPARE(SyncManager::isChosen(folder), stored);
    }

    static FolderInfo folder(const QString &path, const QString &role)
    {
        FolderInfo info;
        info.path = path;
        info.name = path;
        info.role = role;
        return info;
    }

    void testFolderForRole()
    {
        const QList<FolderInfo> folders = {folder(u"INBOX"_s, u"inbox"_s), folder(u"Trash"_s, u"trash"_s), folder(u"Old Trash"_s, u"trash"_s)};
        QCOMPARE(folderForRole(folders, u"trash"_s), u"Trash"_s); // the first one
        QCOMPARE(folderForRole(folders, u"inbox"_s), u"INBOX"_s);
        QVERIFY(folderForRole(folders, u"archive"_s).isEmpty());
        QVERIFY(folderForRole({}, u"trash"_s).isEmpty());
    }

    void testCanHideFolder()
    {
        const QList<FolderInfo> folders = {folder(u"INBOX"_s, u"inbox"_s), folder(u"Archive"_s, u"archive"_s), folder(u"Projects/Alpha"_s, QString())};
        QVERIFY(!canHideFolder(folders, u"INBOX"_s));
        QVERIFY(!canHideFolder(folders, u"Inbox"_s));
        QVERIFY(!canHideFolder(folders, u"inbox"_s));
        QVERIFY(!canHideFolder(folders, QString()));
        // A server whose inbox role folder has another name.
        QVERIFY(!canHideFolder({folder(u"Posteingang"_s, u"inbox"_s)}, u"Posteingang"_s));
        QVERIFY(canHideFolder(folders, u"Archive"_s));
        QVERIFY(canHideFolder(folders, u"Projects/Alpha"_s));
        QVERIFY(canHideFolder(folders, u"INBOX/Sub"_s));
        // Folder list not loaded yet: the inbox cannot be recognised by role, so refuse.
        QVERIFY(!canHideFolder({}, u"Posteingang"_s));
        QVERIFY(!canHideFolder({}, u"Archive"_s));
    }

    void testArchiveTarget()
    {
        // Yahoo-like: a real Archive folder.
        QCOMPARE(archiveTarget({folder(u"INBOX"_s, u"inbox"_s), folder(u"Archive"_s, u"archive"_s)}), u"Archive"_s);
        // Gmail-like: no archive folder, All Mail does the job.
        QCOMPARE(archiveTarget({folder(u"INBOX"_s, u"inbox"_s), folder(u"[Gmail]/All Mail"_s, u"all"_s)}), u"[Gmail]/All Mail"_s);
        // Both: the archive folder wins.
        QCOMPARE(archiveTarget({folder(u"[Gmail]/All Mail"_s, u"all"_s), folder(u"Archive"_s, u"archive"_s)}), u"Archive"_s);
        // Neither: nothing to archive to.
        QVERIFY(archiveTarget({folder(u"INBOX"_s, u"inbox"_s), folder(u"Trash"_s, u"trash"_s)}).isEmpty());
        QVERIFY(archiveTarget({}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(FolderUtilsTest)
#include "folderutilstest.moc"