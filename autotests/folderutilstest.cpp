// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "folderutils.h"

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
};

QTEST_GUILESS_MAIN(FolderUtilsTest)
#include "folderutilstest.moc"