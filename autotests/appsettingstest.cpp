// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

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
        settings.save();
        QCOMPARE(AppSettings::load(), settings);
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

    void testSavingKeepsOtherGroups()
    {
        KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"Storage"_s).writeEntry("Enabled", true);
        AppSettings().save();
        QVERIFY(KSharedConfig::openConfig(u"plasmamailrc"_s)->group(u"Storage"_s).readEntry("Enabled", false));
    }
};

QTEST_GUILESS_MAIN(AppSettingsTest)

#include "appsettingstest.moc"
