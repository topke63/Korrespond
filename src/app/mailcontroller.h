// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "account.h"
#include "folderutils.h"
#include "imapclient.h"
#include "storagesettings.h"

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

#include <memory>

namespace KMime
{
class Message;
}
class FolderModel;
class MessageListModel;
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
    Q_PROPERTY(qint64 currentUid READ currentUid NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageSubject READ messageSubject NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageFrom READ messageFrom NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageTo READ messageTo NOTIFY currentMessageChanged)
    Q_PROPERTY(QDateTime messageDate READ messageDate NOTIFY currentMessageChanged)
    Q_PROPERTY(QString messageHtml READ messageHtml NOTIFY currentMessageChanged)
    Q_PROPERTY(QStringList messageAttachments READ messageAttachments NOTIFY currentMessageChanged)
    // Local storage settings. importSince is an ISO date ("yyyy-MM-dd"), empty = all mail; the
    // units are StorageSettings::KeepUnit / SizeUnit as ints.
    Q_PROPERTY(bool storageEnabled READ storageEnabled NOTIFY storageSettingsChanged)
    Q_PROPERTY(QString importSince READ importSince NOTIFY storageSettingsChanged)
    Q_PROPERTY(int keepValue READ keepValue NOTIFY storageSettingsChanged)
    Q_PROPERTY(int keepUnit READ keepUnit NOTIFY storageSettingsChanged)
    Q_PROPERTY(int sizeValue READ sizeValue NOTIFY storageSettingsChanged)
    Q_PROPERTY(int sizeUnit READ sizeUnit NOTIFY storageSettingsChanged)

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
    [[nodiscard]] qint64 currentUid() const;
    [[nodiscard]] QString messageSubject() const;
    [[nodiscard]] QString messageFrom() const;
    [[nodiscard]] QString messageTo() const;
    [[nodiscard]] QDateTime messageDate() const;
    [[nodiscard]] QString messageHtml() const;
    [[nodiscard]] QStringList messageAttachments() const;
    [[nodiscard]] bool storageEnabled() const;
    [[nodiscard]] QString importSince() const;
    [[nodiscard]] int keepValue() const;
    [[nodiscard]] int keepUnit() const;
    [[nodiscard]] int sizeValue() const;
    [[nodiscard]] int sizeUnit() const;

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
    // Fetches message `uid` of the current folder and shows it; marks it read if it was unread.
    // The message properties stay empty until it has arrived.
    Q_INVOKABLE void openMessage(qint64 uid);
    // Saves the local storage settings (arguments as the properties). Only saves for now; the
    // store, retention and sync arrive in T39.
    Q_INVOKABLE void saveStorageSettings(bool enabled, const QString &importSince, int keepValue, int keepUnit, int sizeValue, int sizeUnit);
    // The values storage starts with when first turned on, as a map with the property names.
    Q_INVOKABLE QVariantMap defaultStorageSettings() const;

    // How many of the newest messages a folder shows.
    static constexpr int messageLimit = 50;

Q_SIGNALS:
    void accountChanged();
    void busyChanged();
    void currentFolderChanged();
    void currentMessageChanged();
    void storageSettingsChanged();
    void errorOccurred(const QString &message);

private:
    // Reads the saved account's password from the keychain and connects.
    void connectSavedAccount();
    void connectAccount(const Account &account, const QString &password);
    void beginRequest();
    void endRequest();
    void resetRequests();
    void onFoldersListed(const QList<FolderInfo> &folders);
    void onHeadersFetched(const QString &folder, ImapClient::HeaderFilter filter, const QList<MessageSummary> &messages);
    void onMessageFetched(const QString &folder, qint64 uid, const std::shared_ptr<KMime::Message> &message);
    // Empties the message properties; with clearUid also forgets which message is open.
    void clearMessage(bool clearUid);

    Account m_account; // the saved account; invalid when there is none
    Account m_pendingAccount; // being verified by setupAccount()
    QString m_pendingPassword;
    ImapClient *m_client;
    SecretStore *m_secrets;
    FolderModel *m_folderModel;
    MessageListModel *m_messageModel;
    QString m_currentFolder;
    qint64 m_currentUid = 0; // 0 when no message is open
    QString m_messageFolder; // folder of the open message
    QString m_messageSubject;
    QString m_messageFrom;
    QString m_messageTo;
    QDateTime m_messageDate;
    QString m_messageHtml;
    QStringList m_messageAttachments;
    StorageSettings m_storage;
    int m_requests = 0;
};
