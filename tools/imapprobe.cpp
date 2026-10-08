// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

// imap-probe: manual check of the engine's IMAP code against a real account.
//
//   PLASMAMAIL_EMAIL=you@yahoo.com build/bin/imap-probe
//
// Environment: PLASMAMAIL_PROVIDER (yahoo, gmail, outlook; default yahoo), PLASMAMAIL_EMAIL,
// PLASMAMAIL_PASSWORD (optional: if unset, the password is asked for on the terminal without
// echo, so it never ends up in shell history). The password is never printed.

#include "account.h"
#include "imapclient.h"

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

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

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

    print(u"Connecting to %1:%2 as %3"_s.arg(account.imapHost).arg(account.imapPort).arg(account.email));

    ImapClient client;
    client.setAccount(account, password);
    QString inbox;

    // The first error ends the run; later ones are consequences of it.
    QObject::connect(&client, &ImapClient::errorOccurred, &app, [](const QString &message) {
        std::cerr << "ERROR: " << message.toStdString() << std::endl;
        QCoreApplication::exit(1);
    });
    QTimer::singleShot(timeoutMs, &app, [] {
        std::cerr << "ERROR: no answer within " << timeoutMs / 1000 << " s" << std::endl;
        QCoreApplication::exit(1);
    });

    QObject::connect(&client, &ImapClient::foldersListed, &app, [&](const QList<FolderInfo> &folders) {
        print(u"\nFolders (%1):"_s.arg(folders.size()));
        for (const FolderInfo &folder : folders) {
            print(u"  %1%2%3"_s.arg(QString(folder.depth * 2, u' '), folder.path, folder.role.isEmpty() ? QString() : u"  [%1]"_s.arg(folder.role)));
            if (folder.role == "inbox"_L1) {
                inbox = folder.path;
            }
        }
        if (inbox.isEmpty()) {
            std::cerr << "ERROR: no inbox in the folder list" << std::endl;
            QCoreApplication::exit(1);
            return;
        }
        client.fetchHeaders(inbox, headerCount);
    });

    QObject::connect(&client, &ImapClient::headersFetched, &app, [&](const QString &folder, const QList<MessageSummary> &messages) {
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
