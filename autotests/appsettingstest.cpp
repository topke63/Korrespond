// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits>

#include <KConfigGroup>
#include <KSharedConfig>

#include <QStandardPaths>
#include <QTest>

#include "appsettings.h"

using namespace Qt::StringLiterals;

class AppSettingsTest : public QObject
{
    Q_OBJECT

private:
    static KConfigGroup group()
    {
        return KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"General"_s);
    }

private Q_SLOTS:
    void initTestCase()
    {
        // Before the first openConfig(): the config file then lives in a test directory.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        KConfigGroup g = group();
        g.deleteGroup();
        g.sync();
    }

    void testDefaults()
    {
        const AppSettings settings = AppSettings::load();
        QVERIFY(!settings.keepInTray);
        QCOMPARE(settings.checkMinutes, 5);
        QVERIFY(settings.pushEnabled);
        QCOMPARE(settings.sidebarWidth, 252);
        QCOMPARE(settings.listWidth, 360);
        QVERIFY(!settings.sidebarCollapsed);
        QVERIFY(settings.collapsedAccounts.isEmpty());
        QVERIFY(settings.hiddenFolders.isEmpty());
        QVERIFY(!settings.showHiddenFolders);
        QCOMPARE(settings.undoSendSeconds, 10);
        QVERIFY(!settings.conversationView);
        QCOMPARE(settings.windowWidth, 0);
        QCOMPARE(settings.windowHeight, 0);
        QVERIFY(settings.contactsEnabled);
    }

    void testRoundTrip()
    {
        AppSettings settings;
        settings.keepInTray = true;
        settings.checkMinutes = 15;
        settings.pushEnabled = false;
        settings.sidebarWidth = 300;
        settings.listWidth = 600;
        settings.sidebarCollapsed = true;
        // An address with a comma and a backslash must survive the list encoding.
        settings.collapsedAccounts = {u"bob@example.com"_s, u"we,ird\\x@example.com"_s};
        settings.save();
        QCOMPARE(AppSettings::load(), settings);
    }

    void testContactsSwitchRoundTrip()
    {
        for (const bool enabled : {false, true}) {
            AppSettings settings;
            settings.contactsEnabled = enabled;
            settings.save();
            QCOMPARE(AppSettings::load().contactsEnabled, enabled);
        }
    }

    void testUndoSendRoundTrip()
    {
        for (const int seconds : {0, 5, 10, 20, 30}) {
            AppSettings settings;
            settings.undoSendSeconds = seconds;
            settings.save();
            QCOMPARE(AppSettings::load().undoSendSeconds, seconds);
        }
    }

    void testUndoSendSnapsWhenSaved()
    {
        // The in-memory value is snapped by the same helper that load() uses.
        QCOMPARE(AppSettings::snapUndoSend(7), 5);
        QCOMPARE(AppSettings::snapUndoSend(-4), 0);
        AppSettings settings;
        settings.undoSendSeconds = 26;
        settings.save();
        QCOMPARE(group().readEntry("UndoSendSeconds", 0), 30);
    }

    void testUndoSendSnapsToTheNearestChoice()
    {
        // Nearest choice wins; a tie (15, 25) goes to the smaller one.
        const QList<QPair<int, int>> cases = {{7, 5},
                                              {8, 10},
                                              {3, 5},
                                              {2, 0},
                                              {14, 10},
                                              {15, 10},
                                              {16, 20},
                                              {25, 20},
                                              {26, 30},
                                              {100, 30},
                                              {-3, 0},
                                              {-100, 0},
                                              {std::numeric_limits<int>::min(), 0},
                                              {std::numeric_limits<int>::max(), 30}};
        for (const auto &[stored, expected] : cases) {
            KConfigGroup g = group();
            g.writeEntry("UndoSendSeconds", stored);
            g.sync();
            QCOMPARE(AppSettings::load().undoSendSeconds, expected);
        }
    }

    void testGarbageUndoSendFallsBackToTheDefault()
    {
        KConfigGroup g = group();
        g.writeEntry("UndoSendSeconds", "later");
        g.sync();
        QCOMPARE(AppSettings::load().undoSendSeconds, 10);
    }

    void testHiddenFoldersRoundTrip()
    {
        AppSettings settings;
        settings.showHiddenFolders = true;
        // Awkward addresses and paths: commas, slashes, backslashes, dots, brackets, equals, spaces.
        settings.hiddenFolders.insert(u"we,ird\\x.y@example.com"_s, {u"Projects/Alpha"_s, u"a,b"_s, u"back\\slash"_s, u"[Gmail]/All Mail"_s, u"x=y [z]"_s});
        settings.hiddenFolders.insert(u"bob@example.com"_s, {u"Archive.2024"_s, u"INBOX/Sub"_s});
        settings.save();
        const AppSettings loaded = AppSettings::load();
        QCOMPARE(loaded.hiddenFolders, settings.hiddenFolders);
        QVERIFY(loaded.showHiddenFolders);
        QCOMPARE(loaded, settings);
        // Removing an account's list is saved too (no stale keys).
        settings.hiddenFolders.remove(u"bob@example.com"_s);
        settings.save();
        QCOMPARE(AppSettings::load().hiddenFolders, settings.hiddenFolders);
    }

    void testValuesAreClampedWhenLoaded()
    {
        KConfigGroup g = group();
        g.writeEntry("CheckMinutes", 0);
        g.sync();
        QCOMPARE(AppSettings::load().checkMinutes, 1);
        g.writeEntry("CheckMinutes", 500);
        g.sync();
        QCOMPARE(AppSettings::load().checkMinutes, 60);
        g.writeEntry("CheckMinutes", -3);
        g.sync();
        QCOMPARE(AppSettings::load().checkMinutes, 1);
    }

    void testValuesAreClampedWhenSaved()
    {
        AppSettings settings;
        settings.checkMinutes = 1000;
        settings.save();
        QCOMPARE(group().readEntry("CheckMinutes", 0), 60);
        settings.checkMinutes = 0;
        settings.save();
        QCOMPARE(group().readEntry("CheckMinutes", 0), 1);
    }

    void testGarbageMinutesFallBackToTheDefault()
    {
        KConfigGroup g = group();
        g.writeEntry("CheckMinutes", "soon");
        g.sync();
        QCOMPARE(AppSettings::load().checkMinutes, 5);
    }

    void testPanelWidthsAreClampedWhenLoaded()
    {
        KConfigGroup g = group();
        g.writeEntry("SidebarWidth", 10);
        g.writeEntry("ListWidth", 5000);
        g.sync();
        AppSettings settings = AppSettings::load();
        QCOMPARE(settings.sidebarWidth, AppSettings::minSidebarWidth);
        QCOMPARE(settings.listWidth, AppSettings::maxListWidth);
        g.writeEntry("SidebarWidth", 9999);
        g.writeEntry("ListWidth", -1);
        g.sync();
        settings = AppSettings::load();
        QCOMPARE(settings.sidebarWidth, AppSettings::maxSidebarWidth);
        QCOMPARE(settings.listWidth, AppSettings::minListWidth);
    }

    void testPanelWidthsAreClampedWhenSaved()
    {
        AppSettings settings;
        settings.sidebarWidth = 1;
        settings.listWidth = 100000;
        settings.save();
        QCOMPARE(group().readEntry("SidebarWidth", 0), 160);
        QCOMPARE(group().readEntry("ListWidth", 0), 800);
    }

    void testGarbagePanelWidthsFallBackToTheDefaults()
    {
        KConfigGroup g = group();
        g.writeEntry("SidebarWidth", "wide");
        g.writeEntry("ListWidth", "wider");
        g.sync();
        const AppSettings settings = AppSettings::load();
        QCOMPARE(settings.sidebarWidth, AppSettings::defaultSidebarWidth);
        QCOMPARE(settings.listWidth, AppSettings::defaultListWidth);
    }

    void testConversationViewRoundTrip()
    {
        AppSettings settings;
        settings.conversationView = true;
        settings.save();
        QVERIFY(AppSettings::load().conversationView);
        settings.conversationView = false;
        settings.save();
        QVERIFY(!AppSettings::load().conversationView);
    }

    void testWindowSizeRoundTrip()
    {
        AppSettings settings;
        settings.windowWidth = 1765;
        settings.windowHeight = 906;
        settings.save();
        const AppSettings loaded = AppSettings::load();
        QCOMPARE(loaded.windowWidth, 1765);
        QCOMPARE(loaded.windowHeight, 906);
        KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"General"_s).writeEntry("WindowWidth", -5);
        QCOMPARE(AppSettings::load().windowWidth, 0); // nonsense is ignored
    }

    void testSavingKeepsOtherGroups()
    {
        KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"Storage"_s).writeEntry("Enabled", true);
        AppSettings().save();
        QVERIFY(KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"Storage"_s).readEntry("Enabled", false));
    }
};

QTEST_GUILESS_MAIN(AppSettingsTest)

#include "appsettingstest.moc"
