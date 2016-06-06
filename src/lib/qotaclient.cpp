/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt OTA Update module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qotaclientasync_p.h"
#include "qotaclient_p.h"
#include "qotaclient.h"

#include <QtCore/QFile>
#include <QtCore/QJsonObject>

Q_LOGGING_CATEGORY(otaLog, "qt.ota")

QT_BEGIN_NAMESPACE

QOTAClientPrivate::QOTAClientPrivate(QOTAClient *client) :
    q_ptr(client),
    m_initialized(false),
    m_updateAvailable(false),
    m_restartRequired(false)
{
    m_otaEnabled = QFile().exists(QStringLiteral("/run/ostree-booted"));
    if (m_otaEnabled) {
        m_otaAsyncThread = new QThread();
        m_otaAsyncThread->start();
        m_otaAsync.reset(new QOTAClientAsync());
        m_otaAsync->moveToThread(m_otaAsyncThread);
    }
}

QOTAClientPrivate::~QOTAClientPrivate()
{
    if (m_otaEnabled) {
        if (m_otaAsyncThread->isRunning()) {
            m_otaAsyncThread->quit();
            if (Q_UNLIKELY(m_otaAsyncThread->wait(4000)))
                qCWarning(otaLog) << "Timed out waiting for worker thread to exit.";
        }
        delete m_otaAsyncThread;
    }
}

static void updateInfoMembers(const QJsonDocument &json, QByteArray *info, QString *version, QString *description)
{
    if (json.isNull())
        return;

    *info = json.toJson();
    QJsonObject root = json.object();
    *version = root.value(QStringLiteral("version")).toString(QStringLiteral("unknown"));
    *description = root.value(QStringLiteral("description")).toString(QStringLiteral("unknown"));
}

void QOTAClientPrivate::updateServerInfo(const QString &serverRev, const QJsonDocument &serverInfo)
{
    Q_Q(QOTAClient);
    if (m_serverRev == serverRev)
        return;

    m_serverRev = serverRev;
    updateInfoMembers(serverInfo, &m_serverInfo, &m_serverVersion, &m_serverDescription);
    emit q->serverInfoChanged();
}

void QOTAClientPrivate::refreshState()
{
    Q_Q(QOTAClient);

    bool updateAvailable = m_defaultRev != m_serverRev && m_serverRev != m_clientRev;
    if (m_updateAvailable != updateAvailable) {
        m_updateAvailable = updateAvailable;
        emit q->updateAvailableChanged(m_updateAvailable);
    }

    bool restartRequired = m_clientRev != m_defaultRev;
    if (m_restartRequired != restartRequired) {
        m_restartRequired = restartRequired;
        emit q->restartRequiredChanged(m_restartRequired);
    }
}

void QOTAClientPrivate::initializeFinished(const QString &defaultRev,
                                           const QString &clientRev, const QJsonDocument &clientInfo,
                                           const QString &serverRev, const QJsonDocument &serverInfo)
{
    Q_Q(QOTAClient);
    m_defaultRev = defaultRev;
    m_clientRev = clientRev;
    updateInfoMembers(clientInfo, &m_clientInfo, &m_clientVersion, &m_clientDescription);
    updateServerInfo(serverRev, serverInfo);
    refreshState();
    m_initialized = true;
    emit q->initializationFinished();
}

void QOTAClientPrivate::fetchServerInfoFinished(const QString &serverRev, const QJsonDocument &serverInfo, bool success)
{
    Q_Q(QOTAClient);
    if (success) {
        updateServerInfo(serverRev, serverInfo);
        refreshState();
    }
    emit q->fetchServerInfoFinished(success);
}

void QOTAClientPrivate::updateFinished(const QString &defaultRev, bool success)
{
    Q_Q(QOTAClient);
    if (success) {
        m_defaultRev = defaultRev;
        refreshState();
    }
    emit q->updateFinished(success);
}

void QOTAClientPrivate::rollbackFinished(const QString &defaultRev, bool success)
{
    Q_Q(QOTAClient);
    if (success) {
        m_defaultRev = defaultRev;
        refreshState();
    }
    emit q->rollbackFinished(success);
}

void QOTAClientPrivate::errorOccurred(const QString &error)
{
    Q_Q(QOTAClient);
    m_error = error;
    emit q->errorOccurred(m_error);
}

void QOTAClientPrivate::rollbackChanged(const QString &rollbackRev, const QJsonDocument &rollbackInfo)
{
    Q_Q(QOTAClient);
    if (m_rollbackRev == rollbackRev)
        return;

    m_rollbackRev = rollbackRev;
    updateInfoMembers(rollbackInfo, &m_rollbackInfo, &m_rollbackVersion, &m_rollbackDescription);
    q->rollbackInfoChanged();
}

QString QOTAClientPrivate::version(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QStringLiteral("The OTA update capability is not enabled.");

    switch (target) {
    case QueryTarget::Client:
        return m_clientVersion.isEmpty() ? QStringLiteral("unknown") : m_clientVersion;
    case QueryTarget::Server:
        return m_serverVersion.isEmpty() ? QStringLiteral("unknown") : m_serverVersion;
    case QueryTarget::Rollback:
        return m_rollbackVersion.isEmpty() ? QStringLiteral("unknown") : m_rollbackVersion;
    default:
        Q_UNREACHABLE();
    }
}

QByteArray QOTAClientPrivate::info(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QByteArray("unknown");

    switch (target) {
    case QueryTarget::Client:
        return m_clientInfo;
    case QueryTarget::Server:
        return m_serverInfo;
    case QueryTarget::Rollback:
        return m_rollbackInfo;
    default:
        Q_UNREACHABLE();
    }
}

QString QOTAClientPrivate::description(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QStringLiteral("The OTA update capability is not enabled.");

    switch (target) {
    case QueryTarget::Client:
        return m_clientDescription.isEmpty() ? QStringLiteral("unknown") : m_clientDescription;
    case QueryTarget::Server:
        return m_serverDescription.isEmpty() ? QStringLiteral("unknown") : m_serverDescription;
    case QueryTarget::Rollback:
        return m_rollbackDescription.isEmpty() ? QStringLiteral("unknown") : m_rollbackDescription;
    default:
        Q_UNREACHABLE();
    }
}

QString QOTAClientPrivate::revision(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QStringLiteral("The OTA update capability is not enabled.");

    switch (target) {
    case QueryTarget::Client:
        return m_clientRev.isEmpty() ? QStringLiteral("unknown") : m_clientRev;
    case QueryTarget::Server:
        return m_serverRev.isEmpty() ? QStringLiteral("unknown") : m_serverRev;
    case QueryTarget::Rollback:
        return m_rollbackRev.isEmpty() ? QStringLiteral("unknown") : m_rollbackRev;
    default:
        Q_UNREACHABLE();
    }
}

QOTAClient::QOTAClient(QObject *parent) : QObject(parent),
    d_ptr(new QOTAClientPrivate(this))
{
    Q_D(QOTAClient);
    if (d->m_otaEnabled) {
        QOTAClientAsync *async = d->m_otaAsync.data();
        // async finished handlers
        connect(async, &QOTAClientAsync::initializeFinished, d, &QOTAClientPrivate::initializeFinished);
        connect(async, &QOTAClientAsync::fetchServerInfoFinished, d, &QOTAClientPrivate::fetchServerInfoFinished);
        connect(async, &QOTAClientAsync::updateFinished, d, &QOTAClientPrivate::updateFinished);
        connect(async, &QOTAClientAsync::rollbackFinished, d, &QOTAClientPrivate::rollbackFinished);
        connect(async, &QOTAClientAsync::errorOccurred, d, &QOTAClientPrivate::errorOccurred);
        connect(async, &QOTAClientAsync::rollbackChanged, d, &QOTAClientPrivate::rollbackChanged);
        d->m_otaAsync->initialize();
    }
}

QOTAClient::~QOTAClient()
{
    delete d_ptr;
}

bool QOTAClient::fetchServerInfo() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled)
        return false;

    d->m_otaAsync->fetchServerInfo();
    return true;
}

bool QOTAClient::update() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled || !updateAvailable())
        return false;

    d->m_otaAsync->update(d->m_serverRev);
    return true;
}

bool QOTAClient::rollback() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled || !d->m_initialized)
        return false;

    d->m_otaAsync->rollback();
    return true;
}

bool QOTAClient::otaEnabled() const
{
    Q_D(const QOTAClient);
    return d->m_otaEnabled;
}

bool QOTAClient::initialized() const
{
    Q_D(const QOTAClient);
    return d->m_initialized;
}

QString QOTAClient::errorString() const
{
    Q_D(const QOTAClient);
    return d->m_error;
}

bool QOTAClient::updateAvailable() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled || d->m_serverRev.isEmpty())
        return false;

    return d->m_updateAvailable;
}

bool QOTAClient::restartRequired() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled)
        return false;

    return d->m_restartRequired;
}

QString QOTAClient::clientVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Client);
}

QString QOTAClient::clientDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Client);
}

QString QOTAClient::clientRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Client);
}

QByteArray QOTAClient::clientInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Client);
}

QString QOTAClient::serverVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Server);
}

QString QOTAClient::serverDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Server);
}

QString QOTAClient::serverRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Server);
}

QByteArray QOTAClient::serverInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Server);
}

QString QOTAClient::rollbackVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Rollback);
}

QString QOTAClient::rollbackDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Rollback);
}

QString QOTAClient::rollbackRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Rollback);
}

QByteArray QOTAClient::rollbackInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Rollback);
}


QT_END_NAMESPACE
