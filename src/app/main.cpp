// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KAboutData>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "mailcontroller.h"

#include <QAbstractItemModel>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QtWebEngineQuick>

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
            controller->openMessage(model->index(0, 0).data(uidRole).toLongLong());
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
    QGuiApplication app(argc, argv);

    KLocalizedString::setApplicationDomain("plasma-mail");
    KAboutData about(u"plasma-mail"_s, i18nc("@title", "Plasma Mail"), u"0.1"_s,
                     i18nc("@info", "A simple IMAP mail client for Plasma"), KAboutLicense::GPL_V2,
                     i18nc("@info:credit", "© 2026 topke"), QString(), QString(),
                     QString()); // no bug address: not a KDE project, so no bugs.kde.org link
    // Not "org.kde.…" (the default), so the About page shows no KDE donate/get-involved links.
    about.setDesktopFileName(u"plasma-mail"_s);
    about.setLicense(KAboutLicense::GPL_V2, KAboutLicense::OrLaterVersions); // matches the SPDX headers
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

    return app.exec();
}
