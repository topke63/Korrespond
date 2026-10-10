// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "foldermodel.h"

#include <QAbstractItemModelTester>
#include <QStandardPaths>
#include <QTest>

using namespace Qt::StringLiterals;

static FolderInfo folder(const QString &path, const QString &name, const QString &role, int depth = 0)
{
    FolderInfo info;
    info.path = path;
    info.name = name;
    info.role = role;
    info.depth = depth;
    return info;
}

static QList<FolderInfo> sampleFolders()
{
    return {
        folder(u"INBOX"_s, u"INBOX"_s, u"inbox"_s),
        folder(u"Sent"_s, u"Sent"_s, u"sent"_s),
        folder(u"Drafts"_s, u"Drafts"_s, u"drafts"_s),
        folder(u"Work/Projects"_s, u"Projects"_s, QString(), 1),
    };
}

class FolderModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

    void testModelConsistency()
    {
        FolderModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setFolders(sampleFolders());
        model.setFolders({});
        model.setFolders(sampleFolders());
        QCOMPARE(model.rowCount(), 4);
    }

    void testEmptyModel()
    {
        FolderModel model;
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.data(model.index(0), FolderModel::PathRole), QVariant());
        QCOMPARE(model.inboxPath(), QString());
    }

    void testRoles()
    {
        FolderModel model;
        model.setFolders(sampleFolders());

        const QModelIndex inbox = model.index(0);
        QCOMPARE(inbox.data(FolderModel::PathRole).toString(), u"INBOX"_s);
        QCOMPARE(inbox.data(FolderModel::NameRole).toString(), u"Inbox"_s); // translated, not "INBOX"
        QCOMPARE(inbox.data(Qt::DisplayRole).toString(), u"Inbox"_s);
        QCOMPARE(inbox.data(FolderModel::FolderRoleRole).toString(), u"inbox"_s);
        QCOMPARE(inbox.data(FolderModel::DepthRole).toInt(), 0);

        const QModelIndex nested = model.index(3);
        QCOMPARE(nested.data(FolderModel::NameRole).toString(), u"Projects"_s);
        QCOMPARE(nested.data(FolderModel::DepthRole).toInt(), 1);

        const QHash<int, QByteArray> names = model.roleNames();
        QCOMPARE(names.value(FolderModel::PathRole), "path");
        QCOMPARE(names.value(FolderModel::NameRole), "name");
        QCOMPARE(names.value(FolderModel::FolderRoleRole), "folderRole");
        QCOMPARE(names.value(FolderModel::DepthRole), "depth");
        QCOMPARE(names.value(FolderModel::IconNameRole), "iconName");
    }

    void testIconNames_data()
    {
        QTest::addColumn<QString>("role");
        QTest::addColumn<QString>("icon");
        QTest::newRow("inbox") << u"inbox"_s << u"mail-folder-inbox"_s;
        QTest::newRow("sent") << u"sent"_s << u"mail-folder-sent"_s;
        QTest::newRow("drafts") << u"drafts"_s << u"document-edit"_s;
        QTest::newRow("trash") << u"trash"_s << u"user-trash"_s;
        QTest::newRow("junk") << u"junk"_s << u"mail-mark-junk"_s;
        QTest::newRow("archive") << u"archive"_s << u"mail-folder-outbox"_s;
        QTest::newRow("all") << u"all"_s << u"mail-folder-outbox"_s;
        QTest::newRow("flagged") << u"flagged"_s << u"mail-mark-important"_s;
        QTest::newRow("other") << QString() << u"folder-mail"_s;
    }

    void testIconNames()
    {
        QFETCH(QString, role);
        QFETCH(QString, icon);
        FolderModel model;
        model.setFolders({folder(u"X"_s, u"X"_s, role)});
        QCOMPARE(model.index(0).data(FolderModel::IconNameRole).toString(), icon);
    }

    void testDisplayName()
    {
        FolderModel model;
        model.setFolders(sampleFolders());
        QCOMPARE(model.displayName(u"INBOX"_s), u"Inbox"_s);
        QCOMPARE(model.displayName(u"Sent"_s), u"Sent"_s);
        QCOMPARE(model.displayName(u"Work/Projects"_s), u"Projects"_s);
        QCOMPARE(model.displayName(u"Missing/Folder"_s), u"Missing/Folder"_s);
    }

    void testInboxPath()
    {
        FolderModel model;
        QList<FolderInfo> folders = sampleFolders();
        std::swap(folders[0], folders[2]); // inbox not first
        model.setFolders(folders);
        QCOMPARE(model.inboxPath(), u"INBOX"_s);

        model.setFolders({folder(u"Sent"_s, u"Sent"_s, u"sent"_s), folder(u"Drafts"_s, u"Drafts"_s, u"drafts"_s)});
        QCOMPARE(model.inboxPath(), u"Sent"_s); // falls back to the first folder

        model.setFolders({});
        QCOMPARE(model.inboxPath(), QString());
    }
};

QTEST_GUILESS_MAIN(FolderModelTest)
#include "foldermodeltest.moc"
