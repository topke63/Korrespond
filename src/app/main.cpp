// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KAboutData>
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
#include <QTimer>
#include <QtWebEngineQuick>

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
    QTimer::singleShot(delay, &engine, [&engine, file] {
        // The window in front (e.g. the settings window), else the main window.
        auto window = qobject_cast<QQuickWindow *>(QGuiApplication::focusWindow());
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
