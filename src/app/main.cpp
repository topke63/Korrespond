// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KAboutData>
#include <KColorSchemeManager>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "appsettings.h"
#include "mailcontroller.h"
#include "mailnotifier.h"
#include "trayicon.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTimer>
#include <QtWebEngineQuick>

#include <QFile>
#include <memory>

using namespace Qt::StringLiterals;

// Dev only: PLASMAMAIL_SCREENSHOT=<file.png> saves the app window in front (never the screen) after
// PLASMAMAIL_SCREENSHOT_DELAY ms (default 6000) and quits. PLASMAMAIL_SCREENSHOT_OPEN=1 first
// opens the newest message of the current folder, halfway through the delay.
static void scheduleScreenshot(QQmlApplicationEngine &engine)
{
    const QString file = qEnvironmentVariable("PLASMAMAIL_SCREENSHOT");
    if (file.isEmpty()) {
        return;
    }
    bool ok = false;
    int delay = qEnvironmentVariableIntValue("PLASMAMAIL_SCREENSHOT_DELAY", &ok);
    if (!ok || delay <= 0) {
        delay = 6000;
    }

    if (qEnvironmentVariableIntValue("PLASMAMAIL_SCREENSHOT_OPEN") == 1) {
        QTimer::singleShot(delay / 2, &engine, [&engine] {
            auto controller = engine.singletonInstance<MailController *>("PlasmaMail", "MailController");
            QAbstractItemModel *model = controller ? controller->messageModel() : nullptr;
            if (!model || model->rowCount() == 0) {
                return;
            }
            const int uidRole = model->roleNames().key("uid");
            const int accountRole = model->roleNames().key("account");
            controller->openMessage(model->index(0, 0).data(accountRole).toString(), model->index(0, 0).data(uidRole).toLongLong());
        });
    }
    // PLASMAMAIL_SCREENSHOT_SAMPLE=<file.eml> shows that local message in the reading pane (parsed
    // and rendered as usual; no account, server, cache or flag is touched), a third of the way in.
    const QString sample = qEnvironmentVariable("PLASMAMAIL_SCREENSHOT_SAMPLE");
    if (!sample.isEmpty()) {
        QTimer::singleShot(delay / 3, &engine, [&engine, sample] {
            QFile sampleFile(sample);
            auto controller = engine.singletonInstance<MailController *>("PlasmaMail", "MailController");
            if (controller && sampleFile.open(QIODevice::ReadOnly)) {
                controller->showSampleMessage(sampleFile.readAll());
            } else {
                qWarning("Could not read the sample message %s", qPrintable(sample));
            }
        });
    }
    // PLASMAMAIL_SCREENSHOT_SETTINGS=<module id> (accounts, storage, about) opens that settings page
    // a third of the way in.
    const QString settingsModule = qEnvironmentVariable("PLASMAMAIL_SCREENSHOT_SETTINGS");
    if (!settingsModule.isEmpty()) {
        QTimer::singleShot(delay / 3, &engine, [&engine, settingsModule] {
            if (!engine.rootObjects().isEmpty()) {
                QMetaObject::invokeMethod(engine.rootObjects().constFirst(), "openSettingsModule", Q_ARG(QString, settingsModule));
            }
        });
    }
    // PLASMAMAIL_SCREENSHOT_COMPOSE=new|reply opens the message window (empty, or filled like a
    // reply) and screenshots that one.
    const QString compose = qEnvironmentVariable("PLASMAMAIL_SCREENSHOT_COMPOSE");
    QQuickWindow *composer = nullptr;
    if (!compose.isEmpty()) {
        if (compose == "reply"_L1) {
            engine.setInitialProperties({{u"initialTo"_s, u"Alice <alice@example.org>"_s},
                                         {u"initialSubject"_s, u"Re: Lunch"_s},
                                         {u"initialBody"_s, u"\n\nOn 8. 10. 2026, Alice wrote:\n> Hello Bob,\n>\n> lunch?\n"_s}});
        }
        engine.loadFromModule("PlasmaMail", "ComposeWindow");
        composer = qobject_cast<QQuickWindow *>(engine.rootObjects().constLast());
    }
    QTimer::singleShot(delay, &engine, [&engine, file, composer] {
        // The window in front (e.g. the settings window), else the main window.
        auto window = composer ? composer : qobject_cast<QQuickWindow *>(QGuiApplication::focusWindow());
        if (!window && !engine.rootObjects().isEmpty()) {
            window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        }
        const bool saved = window && window->grabWindow().save(file);
        if (!saved) {
            qWarning("Could not save the screenshot to %s", qPrintable(file));
        }
        QCoreApplication::exit(saved ? 0 : 1);
    });
}

int main(int argc, char *argv[])
{
    // QtWebEngine must be set up before the application object exists.
    QtWebEngineQuick::initialize();
    // QApplication (not QGuiApplication): the tray icon's menu is a QMenu.
    QApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("plasma-mail");
    KAboutData about(u"plasma-mail"_s,
                     i18nc("@title", "Plasma Mail"),
                     u"0.1"_s,
                     i18nc("@info", "A simple IMAP mail client for Plasma"),
                     KAboutLicense::GPL_V2,
                     i18nc("@info:credit", "© 2026 topke"),
                     QString(),
                     QString(),
                     QString()); // no bug address: not a KDE project, so no bugs.kde.org link
    // Not "org.kde.…" (the default), so the About page shows no KDE donate/get-involved links.
    about.setDesktopFileName(u"plasma-mail"_s);
    about.setLicense(KAboutLicense::GPL_V2, KAboutLicense::OrLaterVersions); // matches the SPDX headers
    // The installed themed icon, else the copy inside the executable.
    const QIcon appIcon = QIcon::fromTheme(u"plasma-mail"_s, QIcon(u":/icons/sc-apps-plasma-mail.svg"_s));
    QGuiApplication::setWindowIcon(appIcon);
    about.setProgramLogo(appIcon); // the About page
    KAboutData::setApplicationData(about);

    // Breeze look by default; QT_QUICK_CONTROLS_STYLE still wins when set.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    // Dev only: PLASMAMAIL_COLOR_SCHEME=light|dark|<installed scheme name> forces the whole app into
    // that KDE colour scheme (light/dark = Breeze Light/Dark; names match case-insensitively, e.g.
    // "Breeze Classic"), which the Kirigami colour sets (sidebar, lists, headers, buttons, menus,
    // dialogs) follow. This process only: autosave is off, so nothing is written to the config.
    // Unset: system scheme.
    const QString wanted = qEnvironmentVariable("PLASMAMAIL_COLOR_SCHEME").trimmed();
    if (!wanted.isEmpty()) {
        auto *manager = KColorSchemeManager::instance();
        manager->setAutosaveChanges(false);
        const QString name = wanted.compare("light"_L1, Qt::CaseInsensitive) == 0 ? u"Breeze Light"_s
            : wanted.compare("dark"_L1, Qt::CaseInsensitive) == 0                 ? u"Breeze Dark"_s
                                                                                  : wanted;
        QAbstractItemModel *model = manager->model();
        QModelIndex found;
        QStringList available;
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            const QString schemeName = index.data(Qt::DisplayRole).toString();
            available << schemeName;
            if (!found.isValid() && schemeName.compare(name, Qt::CaseInsensitive) == 0) {
                found = index;
            }
        }
        if (found.isValid()) {
            manager->activateScheme(found);
            // Qt's own hint follows the scheme's window lightness (the KDE platform theme may ignore it).
            const bool light = QGuiApplication::palette().window().color().lightnessF() >= 0.5;
            QGuiApplication::styleHints()->setColorScheme(light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
            qInfo().noquote() << "PLASMAMAIL_COLOR_SCHEME: activated" << manager->activeSchemeName() << (light ? "(light)" : "(dark)");
        } else {
            qWarning().noquote() << "PLASMAMAIL_COLOR_SCHEME: unknown scheme" << wanted << "- available:" << available.join(u", "_s);
        }
    }

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [] {
            QCoreApplication::exit(1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule("PlasmaMail", "Main");
    scheduleScreenshot(engine);

    // The tray icon; not in the screenshot runs (they must not put icons on the desktop), unless
    // PLASMAMAIL_SCREENSHOT_TRAY=1.
    std::unique_ptr<TrayIcon> tray;
    std::unique_ptr<MailNotifier> notifier;
    const bool screenshotRun = !qEnvironmentVariableIsEmpty("PLASMAMAIL_SCREENSHOT") && qEnvironmentVariableIntValue("PLASMAMAIL_SCREENSHOT_TRAY") != 1;
    auto controller = engine.singletonInstance<MailController *>("PlasmaMail", "MailController");
    if (!screenshotRun && controller && !engine.rootObjects().isEmpty()) {
        tray = std::make_unique<TrayIcon>(controller, qobject_cast<QWindow *>(engine.rootObjects().constFirst()));
        tray->setKeepInTray(AppSettings::load().keepInTray);
        // The tray follows the "keep running in the tray" option as it is changed in Settings.
        QObject::connect(controller, &MailController::appSettingsChanged, tray.get(), [controller, trayPtr = tray.get()] {
            trayPtr->setKeepInTray(controller->keepInTray());
        });
        notifier = std::make_unique<MailNotifier>(controller, qobject_cast<QWindow *>(engine.rootObjects().constFirst()));
    }

    return app.exec();
}
