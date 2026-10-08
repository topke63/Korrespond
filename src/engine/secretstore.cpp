// SPDX-FileCopyrightText: 2026 topke <topke63@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "secretstore.h"

#include <qt6keychain/keychain.h>

using namespace Qt::StringLiterals;

static QString errorText(const QKeychain::Job *job)
{
    if (job->error() == QKeychain::NoError) {
        return {};
    }
    // errorString() can be empty for some backends; never report failure as an empty string.
    return job->errorString().isEmpty() ? u"Keychain error %1"_s.arg(int(job->error())) : job->errorString();
}

SecretStore::SecretStore(const QString &service, QObject *parent)
    : QObject(parent)
    , m_service(service)
{
}

QString SecretStore::service() const
{
    return m_service;
}

void SecretStore::writePassword(const QString &key, const QString &password)
{
    auto job = new QKeychain::WritePasswordJob(m_service, this);
    job->setInsecureFallback(false);
    job->setKey(key);
    job->setTextData(password);
    connect(job, &QKeychain::Job::finished, this, [this, key](QKeychain::Job *job) {
        Q_EMIT passwordWritten(key, errorText(job));
    });
    job->start();
}

void SecretStore::readPassword(const QString &key)
{
    auto job = new QKeychain::ReadPasswordJob(m_service, this);
    job->setInsecureFallback(false);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [this, key](QKeychain::Job *job) {
        const QString error = errorText(job);
        const QString password = error.isEmpty() ? static_cast<QKeychain::ReadPasswordJob *>(job)->textData() : QString();
        Q_EMIT passwordRead(key, password, error);
    });
    job->start();
}

void SecretStore::deletePassword(const QString &key)
{
    auto job = new QKeychain::DeletePasswordJob(m_service, this);
    job->setInsecureFallback(false);
    job->setKey(key);
    connect(job, &QKeychain::Job::finished, this, [this, key](QKeychain::Job *job) {
        Q_EMIT passwordDeleted(key, job->error() == QKeychain::EntryNotFound ? QString() : errorText(job));
    });
    job->start();
}
