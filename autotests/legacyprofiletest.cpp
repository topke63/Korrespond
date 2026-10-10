// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "legacyprofile.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

static void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

class LegacyProfileTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void oldNamesAreCopiedAndMoved()
    {
        QTemporaryDir config;
        QTemporaryDir data;
        writeFile(config.filePath(u"plasmamailrc"_s), "[General]\nWindowWidth=900\n");
        writeFile(data.filePath(u"plasma-mail/accounts/a@example.com/mail.db"_s), "cache");

        const auto result = LegacyProfile::migrate(config.path(), data.path());

        QVERIFY(result.configCopied);
        QVERIFY(result.dataMoved);
        QCOMPARE(readFile(config.filePath(u"korrespondrc"_s)), QByteArray("[General]\nWindowWidth=900\n"));
        QVERIFY(QFileInfo::exists(config.filePath(u"plasmamailrc"_s))); // the old file stays
        QCOMPARE(readFile(data.filePath(u"korrespond/accounts/a@example.com/mail.db"_s)), QByteArray("cache"));
        QVERIFY(!QFileInfo::exists(data.filePath(u"plasma-mail"_s)));
    }

    void existingNewProfileIsNeverOverwritten()
    {
        QTemporaryDir config;
        QTemporaryDir data;
        writeFile(config.filePath(u"plasmamailrc"_s), "old");
        writeFile(config.filePath(u"korrespondrc"_s), "new");
        writeFile(data.filePath(u"plasma-mail/old.db"_s), "old");
        writeFile(data.filePath(u"korrespond/new.db"_s), "new");

        const auto result = LegacyProfile::migrate(config.path(), data.path());

        QVERIFY(!result.configCopied);
        QVERIFY(!result.dataMoved);
        QCOMPARE(readFile(config.filePath(u"korrespondrc"_s)), QByteArray("new"));
        QVERIFY(QFileInfo::exists(data.filePath(u"plasma-mail/old.db"_s)));
        QVERIFY(QFileInfo::exists(data.filePath(u"korrespond/new.db"_s)));
    }

    void nothingToDoOnAFreshInstall()
    {
        QTemporaryDir config;
        QTemporaryDir data;

        const auto result = LegacyProfile::migrate(config.path(), data.path());

        QVERIFY(!result.configCopied);
        QVERIFY(!result.dataMoved);
        QVERIFY(!QFileInfo::exists(config.filePath(u"korrespondrc"_s)));
    }

    void secondRunDoesNothing()
    {
        QTemporaryDir config;
        QTemporaryDir data;
        writeFile(config.filePath(u"plasmamailrc"_s), "x");
        writeFile(data.filePath(u"plasma-mail/a"_s), "x");
        QVERIFY(LegacyProfile::migrate(config.path(), data.path()).dataMoved);

        const auto again = LegacyProfile::migrate(config.path(), data.path());

        QVERIFY(!again.configCopied);
        QVERIFY(!again.dataMoved);
    }
};

QTEST_GUILESS_MAIN(LegacyProfileTest)
#include "legacyprofiletest.moc"
