// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include <KAboutData>
#include <KColorSchemeManager>
#include <KColorSchemeModel>
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
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>
#include <QtWebEngineQuick>

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <memory>
#include <qpa/qwindowsysteminterface.h>

using namespace Qt::StringLiterals;

// Dev only (PLASMAMAIL_DEMO=1): a comma-separated list of milliseconds, e.g. "500,1200".
static QList<int> demoTimes(const char *name)
{
    QList<int> times;
    const QStringList parts = qEnvironmentVariable(name).split(u',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        if (const int ms = part.trimmed().toInt(); ms >= 0) {
            times.append(ms);
        }
    }
    return times;
}

// The main window (the first root object): the parent of the message windows the demo opens.
static const QObject *demoRoot = nullptr;

// Dev only: logs the visible windows and each open message window (a ComposeWindow has
// initialSubject), so the demo checks can see what is on screen and what it holds. objects= counts
// the message window objects still alive under the main window (a closed one must be gone).
static void logDemoWindows(const char *when)
{
    int visible = 0;
    for (const QWindow *window : QGuiApplication::topLevelWindows()) {
        visible += window->isVisible() ? 1 : 0;
    }
    int objects = 0;
    if (demoRoot) {
        for (const QObject *child : demoRoot->children()) {
            objects += child->property("initialSubject").isValid() ? 1 : 0;
        }
    }
    qInfo("demo: %s, visible windows=%d, message window objects=%d", when, visible, objects);
    for (const QWindow *window : QGuiApplication::topLevelWindows()) {
        if (!window->property("initialSubject").isValid()) {
            continue;
        }
        qInfo("demo: composer %s visible=%d active=%d subject=\"%s\" restored=%d waiting=%d tooLarge=%d error=\"%s\" attachments=%d initial=%d",
              qPrintable(window->objectName()),
              window->isVisible() ? 1 : 0,
              window->isActive() ? 1 : 0,
              qPrintable(window->property("initialSubject").toString()),
              window->property("restored").toBool() ? 1 : 0,
              window->property("waiting").toBool() ? 1 : 0,
              window->property("attachmentsTooLarge").toBool() ? 1 : 0,
              qPrintable(window->property("errorText").toString()),
              int(window->property("attachments").toList().size()),
              int(window->property("initialAttachments").toList().size()));
    }
}

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
    // PLASMAMAIL_SCREENSHOT_SETTINGS=<module id> (accounts, notifications, sending, storage, about) opens that settings page
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
        // PLASMAMAIL_SCREENSHOT_NEWEST=1: the newest visible window (a message window opened by the
        // app itself, e.g. by undo send).
        if (qEnvironmentVariableIntValue("PLASMAMAIL_SCREENSHOT_NEWEST") == 1) {
            const auto windows = QGuiApplication::topLevelWindows();
            for (auto it = windows.crbegin(); it != windows.crend(); ++it) {
                if (auto quick = qobject_cast<QQuickWindow *>(*it); quick && quick->isVisible()) {
                    window = quick;
                    break;
                }
            }
        }
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
    const QIcon appIcon = QIcon::fromTheme(u"org.topke.plasmamail"_s, QIcon(u":/icons/sc-apps-org.topke.plasmamail.svg"_s));
    QGuiApplication::setWindowIcon(appIcon);
    about.setProgramLogo(appIcon); // the About page
    KAboutData::setApplicationData(about);

    // Breeze look by default; QT_QUICK_CONTROLS_STYLE still wins when set.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(u"org.kde.desktop"_s);
    }

    // Dev only: PLASMAMAIL_COLOR_SCHEME=light|dark|<installed scheme> forces the whole app into
    // that KDE colour scheme (light/dark = Breeze Light/Dark). <installed scheme> is the display name
    // ("Breeze Classic") or the scheme file id ("BreezeClassic", the .colors file name); both match
    // case-insensitively. The Kirigami colour sets (sidebar, lists, headers, buttons, menus,
    // dialogs) follow. This process only: autosave is off, so nothing is written to the config.
    // Unset: system scheme.
    const QString wanted = qEnvironmentVariable("PLASMAMAIL_COLOR_SCHEME").trimmed();
    if (!wanted.isEmpty()) {
        // Written to stderr directly: Qt's qWarning/qInfo go to the journal instead when the
        // environment has JOURNAL_STREAM set, and the dev hook's messages must be seen.
        QTextStream report(stderr);
        auto *manager = KColorSchemeManager::instance();
        manager->setAutosaveChanges(false);
        QString name = wanted;
        if (wanted.compare("light"_L1, Qt::CaseInsensitive) == 0) {
            name = u"Breeze Light"_s;
        } else if (wanted.compare("dark"_L1, Qt::CaseInsensitive) == 0) {
            name = u"Breeze Dark"_s;
        }
        QAbstractItemModel *model = manager->model();
        QModelIndex found;
        QStringList available;
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            const QString schemeName = index.data(KColorSchemeModel::NameRole).toString();
            // The scheme file id: "BreezeDark" for Breeze Dark.colors (the file's base name).
            const QString schemeId = QFileInfo(index.data(KColorSchemeModel::PathRole).toString()).completeBaseName();
            available << (schemeId.isEmpty() ? schemeName : u"%1 (%2)"_s.arg(schemeName, schemeId));
            if (!found.isValid()
                && (schemeName.compare(name, Qt::CaseInsensitive) == 0 || (!schemeId.isEmpty() && schemeId.compare(name, Qt::CaseInsensitive) == 0))) {
                found = index;
            }
        }
        if (found.isValid()) {
            manager->activateScheme(found);
            // Qt's own hint follows the scheme's window lightness (the KDE platform theme may ignore it).
            const bool light = QGuiApplication::palette().window().color().lightnessF() >= 0.5;
            QGuiApplication::styleHints()->setColorScheme(light ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
            report << "PLASMAMAIL_COLOR_SCHEME: activated " << manager->activeSchemeName() << (light ? " (light)" : " (dark)") << Qt::endl;
        } else {
            report << "PLASMAMAIL_COLOR_SCHEME: unknown scheme " << wanted << " - available: " << available.join(u", "_s) << Qt::endl;
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
    demoRoot = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().constFirst();
    scheduleScreenshot(engine);

    // The tray icon and notifications; not in the screenshot and demo runs (they must not put icons
    // or popups on the desktop), unless PLASMAMAIL_SCREENSHOT_TRAY=1 for a screenshot run.
    std::unique_ptr<TrayIcon> tray;
    std::unique_ptr<MailNotifier> notifier;
    const bool screenshotRun = (!qEnvironmentVariableIsEmpty("PLASMAMAIL_SCREENSHOT") && qEnvironmentVariableIntValue("PLASMAMAIL_SCREENSHOT_TRAY") != 1)
        || qEnvironmentVariableIntValue("PLASMAMAIL_DEMO") == 1;
    auto controller = engine.singletonInstance<MailController *>("PlasmaMail", "MailController");
    if (!screenshotRun && controller && !engine.rootObjects().isEmpty()) {
        // "Keep running in the tray" is applied by MailController (lastWindowClosed), not here.
        tray = std::make_unique<TrayIcon>(controller, qobject_cast<QWindow *>(engine.rootObjects().constFirst()));
        notifier = std::make_unique<MailNotifier>(controller, qobject_cast<QWindow *>(engine.rootObjects().constFirst()));
    }

    // Dev only, with PLASMAMAIL_DEMO=1 (never any SMTP): PLASMAMAIL_DEMO_SEND=1 queues a fictional
    // message ~1 s after start, PLASMAMAIL_DEMO_UNDO=1 also undoes it ~2 s after start, and
    // PLASMAMAIL_DEMO_QUIT_AFTER=<ms> calls requestQuit() then (to check that quitting flushes pending sends).
    if (controller && qEnvironmentVariableIntValue("PLASMAMAIL_DEMO") == 1) {
        if (qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_SEND") == 1) {
            QTimer::singleShot(1000, controller, [controller] {
                controller->sendMessage({{u"account"_s, u"alice@example.com"_s},
                                         {u"to"_s, u"bob@example.com"_s},
                                         {u"subject"_s, u"Demo: lunch on Friday?"_s},
                                         {u"body"_s, u"Hello Bob,\n\nlunch on Friday?\n\nAlice\n"_s}});
            });
            if (qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_UNDO") == 1) {
                QTimer::singleShot(2000, controller, [controller] {
                    controller->undoSend(-1);
                });
            }
        }
        bool ok = false;
        const int quitAfter = qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_QUIT_AFTER", &ok);
        if (ok && quitAfter > 0) {
            QTimer::singleShot(quitAfter, controller, [controller] {
                controller->requestQuit();
            });
            // PLASMAMAIL_DEMO_QUIT_TWICE=1: a second request 300 ms later (must be ignored).
            if (qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_QUIT_TWICE") == 1) {
                QTimer::singleShot(quitAfter + 300, controller, [controller] {
                    controller->requestQuit();
                });
            }
        }
        // Window close checks: PLASMAMAIL_DEMO_COMPOSE=1 opens a message window at start;
        // PLASMAMAIL_DEMO_CLOSE_MAIN=<ms> / PLASMAMAIL_DEMO_CLOSE_COMPOSE=<ms> close the main window
        // (the first root object) / the composer (the last root object) like a window-manager close.
        if (qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_COMPOSE") == 1) {
            engine.loadFromModule("PlasmaMail", "ComposeWindow");
        }
        const int closeMain = qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_CLOSE_MAIN");
        if (closeMain > 0) {
            QTimer::singleShot(closeMain, &engine, [&engine] {
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst())) {
                    window->close();
                }
            });
        }
        const int closeComposer = qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_CLOSE_COMPOSE");
        if (closeComposer > 0) {
            QTimer::singleShot(closeComposer, &engine, [&engine] {
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constLast())) {
                    window->close();
                }
            });
        }

        // PLASMAMAIL_DEMO_COMPOSE_SEND=<ms>[,<ms>...]: message k (1-based) opens at start in its own
        // window (subject "Demo: message k", with the file PLASMAMAIL_DEMO_ATTACHMENT if set), and
        // its send() runs at the k-th time: the same function the Send button calls. Logs the
        // windows just before and after.
        const QList<int> composeSendAt = demoTimes("PLASMAMAIL_DEMO_COMPOSE_SEND");
        const QString demoAttachment = qEnvironmentVariable("PLASMAMAIL_DEMO_ATTACHMENT");
        for (int i = 0; i < composeSendAt.size(); ++i) {
            const int k = i + 1;
            const QString name = u"demoComposer%1"_s.arg(k);
            QTimer::singleShot(300, &engine, [&engine, k, name, demoAttachment] {
                QObject *root = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().constFirst();
                if (!root) {
                    return;
                }
                QVariantList attachments;
                if (!demoAttachment.isEmpty()) {
                    attachments.append(demoAttachment);
                }
                const QVariantMap data{{u"account"_s, u"alice@example.com"_s},
                                       {u"to"_s, u"bob@example.com"_s},
                                       {u"subject"_s, u"Demo: message %1"_s.arg(k)},
                                       {u"body"_s, u"Hello Bob,\n\nmessage %1 of the demo.\n\nAlice\n"_s.arg(k)},
                                       {u"attachments"_s, attachments}};
                QMetaObject::invokeMethod(root, "openDemoComposer", Q_ARG(QVariant, QVariant::fromValue(data)), Q_ARG(QString, name));
            });
            QTimer::singleShot(composeSendAt.at(i), &engine, [&engine, name, k] {
                QObject *root = engine.rootObjects().isEmpty() ? nullptr : engine.rootObjects().constFirst();
                QObject *composer = root ? root->findChild<QObject *>(name) : nullptr;
                qInfo("demo: send requested for message %d (composer %s)", k, composer ? "open" : "missing");
                logDemoWindows("before send");
                if (composer) {
                    QMetaObject::invokeMethod(composer, "send");
                }
                QTimer::singleShot(300, &engine, [] {
                    logDemoWindows("after send");
                });
            });
        }
        // PLASMAMAIL_DEMO_DUMP=<ms>[,<ms>...]: logs the open windows at those times.
        for (const int ms : demoTimes("PLASMAMAIL_DEMO_DUMP")) {
            QTimer::singleShot(ms, &engine, [] {
                logDemoWindows("dump");
            });
        }
        // PLASMAMAIL_DEMO_CLOSE_MESSAGES=<ms>[,<ms>...]: a window-manager close of every open message
        // window at those times (the closing rules of ComposeWindow apply; logs what is still open).
        for (const int ms : demoTimes("PLASMAMAIL_DEMO_CLOSE_MESSAGES")) {
            QTimer::singleShot(ms, &engine, [&engine] {
                for (QWindow *window : QGuiApplication::topLevelWindows()) {
                    if (window->property("initialSubject").isValid()) {
                        qInfo("demo: close requested for composer %s", qPrintable(window->property("initialSubject").toString()));
                        window->close();
                    }
                }
                QTimer::singleShot(300, &engine, [] {
                    logDemoWindows("after close");
                });
            });
        }
        // PLASMAMAIL_DEMO_KEY_UNDO=<ms>: a Ctrl+Z key press for the main window at <ms>, through the platform
        // key path (QWindowSystemInterface), so the window shortcuts see it as a real key press.
        // PLASMAMAIL_DEMO_KEY_UNDO_SEARCH=1 focuses the message list's search field first. Logs the focus
        // item and the pending sends before and after. The window is activated first (the key path
        // needs the focus window; the activation is processed a little later, hence the second timer).
        const int keyUndo = qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_KEY_UNDO");
        if (keyUndo > 0) {
            const bool fromSearch = qEnvironmentVariableIntValue("PLASMAMAIL_DEMO_KEY_UNDO_SEARCH") == 1;
            QTimer::singleShot(keyUndo, &engine, [&engine, controller, fromSearch] {
                auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
                if (!window) {
                    return;
                }
                window->requestActivate();
                if (fromSearch) {
                    if (auto *field = window->findChild<QQuickItem *>(u"searchField"_s)) {
                        field->forceActiveFocus();
                    }
                }
                QTimer::singleShot(300, window, [window, controller] {
                    const QQuickItem *focus = window->activeFocusItem();
                    qInfo("demo: ctrl+z, active=%d focus item=%s, pending before=%lld",
                          window->isActive() ? 1 : 0,
                          focus ? focus->metaObject()->className() : "none",
                          qlonglong(controller->property("pendingSends").toList().size()));
                    QWindowSystemInterface::handleKeyEvent<QWindowSystemInterface::SynchronousDelivery>(window,
                                                                                                        QEvent::KeyPress,
                                                                                                        Qt::Key_Z,
                                                                                                        Qt::ControlModifier,
                                                                                                        QString(QChar(0x1a)));
                    qInfo("demo: ctrl+z sent, pending after=%lld", qlonglong(controller->property("pendingSends").toList().size()));
                });
            });
        }
    }

    return app.exec();
}
