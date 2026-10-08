// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "folderutils.h"

#include <QAbstractListModel>
#include <QObject>
#include <QQmlEngine>

class FolderModel;
class ImapClient;
class MessageListModel;
struct MessageSummary;
class SecretStore;

// The only object the QML UI talks to. Owns the engine objects for the single account.
class MailController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool hasAccount READ hasAccount NOTIFY accountChanged)
    Q_PROPERTY(QString accountEmail READ accountEmail NOTIFY accountChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QAbstractListModel *folderModel READ folderModel CONSTANT)
    Q_PROPERTY(QAbstractListModel *messageModel READ messageModel CONSTANT)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY currentFolderChanged)
    Q_PROPERTY(QString currentFolderName READ currentFolderName NOTIFY currentFolderChanged)

public:
    explicit MailController(QObject *parent = nullptr);
    ~MailController() override;

    [[nodiscard]] bool hasAccount() const;
    [[nodiscard]] QString accountEmail() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] QAbstractListModel *folderModel() const;
    [[nodiscard]] QAbstractListModel *messageModel() const;
    [[nodiscard]] QString currentFolder() const;
    [[nodiscard]] QString currentFolderName() const;

    // Checks the credentials by logging in and listing folders; only then stores the password
    // in the keychain, saves the account and sets hasAccount. On failure nothing is saved and
    // errorOccurred is emitted. provider: "yahoo", "gmail", "outlook" or "custom" (custom uses
    // the host/port arguments; the others use their presets).
    Q_INVOKABLE void setupAccount(const QString &provider,
                                  const QString &email,
                                  const QString &displayName,
                                  const QString &password,
                                  const QString &imapHost = {},
                                  int imapPort = 993,
                                  const QString &smtpHost = {},
                                  int smtpPort = 465);
    // Disconnects, deletes the password from the keychain and the account from the config.
    Q_INVOKABLE void removeAccount();
    // Shows the newest messages of `path` (clears the list first).
    Q_INVOKABLE void selectFolder(const QString &path);
    // Reloads the folder list and the current folder's messages.
    Q_INVOKABLE void refresh();

    // How many of the newest messages a folder shows.
    static constexpr int messageLimit = 50;

Q_SIGNALS:
    void accountChanged();
    void busyChanged();
    void currentFolderChanged();
    void errorOccurred(const QString &message);

private:
    // Reads the saved account's password from the keychain and connects.
    void connectSavedAccount();
    void connectAccount(const Account &account, const QString &password);
    void beginRequest();
    void endRequest();
    void resetRequests();
    void onFoldersListed(const QList<FolderInfo> &folders);
    void onHeadersFetched(const QString &folder, const QList<MessageSummary> &messages);

    Account m_account; // the saved account; invalid when there is none
    Account m_pendingAccount; // being verified by setupAccount()
    QString m_pendingPassword;
    ImapClient *m_client;
    SecretStore *m_secrets;
    FolderModel *m_folderModel;
    MessageListModel *m_messageModel;
    QString m_currentFolder;
    int m_requests = 0;
};
