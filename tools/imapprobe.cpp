// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// imap-probe: manual check of the engine's IMAP code against a real account.
//
//   PLASMAMAIL_EMAIL=you@yahoo.com build/bin/imap-probe
//
// Environment: PLASMAMAIL_PROVIDER (yahoo, gmail, outlook; default yahoo), PLASMAMAIL_EMAIL,
// PLASMAMAIL_PASSWORD (optional: if unset, the password is asked for on the terminal without
// echo, so it never ends up in shell history). The password is never printed.
//
//   PLASMAMAIL_EMAIL=you@yahoo.com PLASMAMAIL_STORE_DIR=/tmp/store build/bin/imap-probe --sync
//
// --sync stores the chosen folders in a local store instead, then prints messages stored per
// folder and the total size. Also reads PLASMAMAIL_STORE_DIR (required), PLASMAMAIL_SINCE
// (yyyy-MM-dd, default all mail) and PLASMAMAIL_MAX_MB (default unlimited). Mail is kept
// forever otherwise. Only reads from the server.

#include "account.h"
#include "imapclient.h"
#include "messagestore.h"
#include "storagesettings.h"
#include "syncmanager.h"

#include <KMime/Message>

#include <QCoreApplication>
#include <QTimer>

#include <termios.h>
#include <unistd.h>

#include <iostream>
#include <string>

using namespace Qt::StringLiterals;

static constexpr int timeoutMs = 60000;
static constexpr int headerCount = 10;

static QString readPasswordFromTerminal()
{
    if (!isatty(STDIN_FILENO)) {
        return {};
    }
    std::cerr << "App password (not shown): " << std::flush;
    termios old{};
    tcgetattr(STDIN_FILENO, &old);
    termios silent = old;
    silent.c_lflag &= ~tcflag_t(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &silent);
    std::string line;
    std::getline(std::cin, line);
    tcsetattr(STDIN_FILENO, TCSANOW, &old);
    std::cerr << std::endl;
    return QString::fromStdString(line);
}

static void print(const QString &text)
{
    std::cout << text.toStdString() << std::endl;
}

static QString megabytes(qint64 bytes)
{
    return QString::number(double(bytes) / (1024 * 1024), 'f', 2);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const bool syncMode = app.arguments().contains(u"--sync"_s);

    const QString provider = qEnvironmentVariable("PLASMAMAIL_PROVIDER", u"yahoo"_s);
    Account account;
    if (!Account::applyPreset(account, provider)) {
        std::cerr << "Unknown PLASMAMAIL_PROVIDER: " << provider.toStdString() << " (use yahoo, gmail or outlook)" << std::endl;
        return 1;
    }
    account.email = qEnvironmentVariable("PLASMAMAIL_EMAIL");
    if (account.email.isEmpty()) {
        std::cerr << "Set PLASMAMAIL_EMAIL to the account's address." << std::endl;
        return 1;
    }
    QString password = qEnvironmentVariable("PLASMAMAIL_PASSWORD");
    if (password.isEmpty()) {
        password = readPasswordFromTerminal();
    }
    if (password.isEmpty()) {
        std::cerr << "No password: set PLASMAMAIL_PASSWORD or run in a terminal to be asked." << std::endl;
        return 1;
    }

    // --sync: storage settings from the environment, checked before connecting.
    const QString storeDir = qEnvironmentVariable("PLASMAMAIL_STORE_DIR");
    StorageSettings settings;
    settings.enabled = true;
    settings.keepUnit = StorageSettings::Forever;
    settings.sizeUnit = StorageSettings::Unlimited;
    if (syncMode) {
        if (storeDir.isEmpty()) {
            std::cerr << "Set PLASMAMAIL_STORE_DIR to a directory for the local store." << std::endl;
            return 1;
        }
        if (qEnvironmentVariableIsSet("PLASMAMAIL_SINCE")) {
            settings.importSince = QDate::fromString(qEnvironmentVariable("PLASMAMAIL_SINCE"), Qt::ISODate);
            if (!settings.importSince.isValid()) {
                std::cerr << "PLASMAMAIL_SINCE must be a date like 2026-01-31." << std::endl;
                return 1;
            }
        }
        if (qEnvironmentVariableIsSet("PLASMAMAIL_MAX_MB")) {
            bool ok = false;
            settings.sizeValue = qEnvironmentVariableIntValue("PLASMAMAIL_MAX_MB", &ok);
            if (!ok || settings.sizeValue < 1) {
                std::cerr << "PLASMAMAIL_MAX_MB must be a whole number of megabytes, at least 1." << std::endl;
                return 1;
            }
            settings.sizeUnit = StorageSettings::MB;
        }
    }
    MessageStore store(storeDir);
    if (syncMode && !store.open()) {
        std::cerr << "ERROR: cannot open the store: " << store.lastError().toStdString() << std::endl;
        return 1;
    }

    print(u"Connecting to %1:%2 as %3"_s.arg(account.imapHost).arg(account.imapPort).arg(account.email));

    ImapClient client;
    client.setAccount(account, password);
    QString inbox;

    // The first error ends the run; later ones are consequences of it.
    QObject::connect(&client, &ImapClient::errorOccurred, &app, [](const QString &message) {
        std::cerr << "ERROR: " << message.toStdString() << std::endl;
        QCoreApplication::exit(1);
    });
    // A sync can take long; the timeout restarts on every sign of progress.
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.setInterval(timeoutMs);
    QObject::connect(&timeout, &QTimer::timeout, &app, [] {
        std::cerr << "ERROR: no answer within " << timeoutMs / 1000 << " s" << std::endl;
        QCoreApplication::exit(1);
    });
    timeout.start();

    SyncManager sync(&client, &store);
    sync.setSettings(settings);
    QList<FolderInfo> syncFolders;
    QObject::connect(&sync, &SyncManager::progress, &app, [&timeout](const QString &folder, int done, int total) {
        timeout.start();
        if (done == 0 || done == total || done % 20 == 0) {
            print(u"  %1: %2 / %3"_s.arg(folder).arg(done).arg(total));
        }
    });
    QObject::connect(&sync, &SyncManager::folderSynced, &app, [&timeout](const QString &folder) {
        timeout.start();
        print(u"  %1: done"_s.arg(folder));
    });
    QObject::connect(&sync, &SyncManager::errorOccurred, &app, [](const QString &message) {
        std::cerr << "ERROR: " << message.toStdString() << std::endl;
        QCoreApplication::exit(1);
    });
    QObject::connect(&sync, &SyncManager::syncFinished, &app, [&] {
        print(u"\nStored in %1:"_s.arg(storeDir));
        for (const FolderInfo &folder : std::as_const(syncFolders)) {
            if (SyncManager::isChosen(folder)) {
                print(u"  %1: %2 messages"_s.arg(folder.path).arg(store.messages(folder.path, -1).size()));
            }
        }
        print(u"Total: %1 MB"_s.arg(megabytes(store.totalSize())));
        print(u"OK"_s);
        QCoreApplication::exit(0);
    });

    QObject::connect(&client, &ImapClient::foldersListed, &app, [&](const QList<FolderInfo> &folders) {
        print(u"\nFolders (%1):"_s.arg(folders.size()));
        for (const FolderInfo &folder : folders) {
            print(u"  %1%2%3"_s.arg(QString(folder.depth * 2, u' '), folder.path, folder.role.isEmpty() ? QString() : u"  [%1]"_s.arg(folder.role)));
            if (folder.role == "inbox"_L1) {
                inbox = folder.path;
            }
        }
        if (syncMode) {
            syncFolders = folders;
            print(u"\nSyncing (since %1, limit %2):"_s.arg(settings.importSince.isValid() ? settings.importSince.toString(Qt::ISODate) : u"all mail"_s,
                                                       settings.maxBytes() > 0 ? u"%1 MB"_s.arg(settings.sizeValue) : u"none"_s));
            sync.syncAll(folders);
            return;
        }
        if (inbox.isEmpty()) {
            std::cerr << "ERROR: no inbox in the folder list" << std::endl;
            QCoreApplication::exit(1);
            return;
        }
        client.fetchHeaders(inbox, headerCount);
    });

    QObject::connect(&client, &ImapClient::headersFetched, &app, [&](const QString &folder, ImapClient::HeaderFilter, const QList<MessageSummary> &messages) {
        print(u"\nNewest %1 messages in %2:"_s.arg(messages.size()).arg(folder));
        for (const MessageSummary &msg : messages) {
            print(u"  %1 %2 %3 | %4 | %5"_s.arg(msg.unread ? u"*"_s : u" "_s)
                      .arg(msg.uid, 7)
                      .arg(msg.date.toLocalTime().toString(u"yyyy-MM-dd hh:mm"_s), msg.from.left(30), msg.subject.left(60)));
        }
        if (messages.isEmpty()) {
            print(u"\nInbox is empty; nothing to fetch. OK"_s);
            QCoreApplication::exit(0);
            return;
        }
        client.fetchMessage(folder, messages.first().uid);
    });

    QObject::connect(&client, &ImapClient::messageFetched, &app, [](const QString &, qint64 uid, const std::shared_ptr<KMime::Message> &message) {
        const auto *subject = message->subject();
        print(u"\nFetched newest message (UID %1): \"%2\", %3 bytes"_s.arg(uid)
                  .arg(subject ? subject->asUnicodeString() : QString())
                  .arg(message->encodedContent().size()));
        print(u"OK"_s);
        QCoreApplication::exit(0);
    });

    client.listFolders();
    return app.exec();
}
