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

/*!
    \class QOTAClient
    \inmodule qtotaupdate
    \brief Main interface to the OTA functionality.

    This class provides API to perform OTA tasks. Offline operations include
    querying the booted and rollback system version details and atomically
    performing the rollbacks. Online operations include fetching a new system
    version from a remote server and atomically performing system updates.

    Using this API is safe and won't leave the system in an inconsistent state,
    even if the power fails half-way through.
*/

/*!
    \fn void QOTAClient::fetchServerInfoFinished(bool success)

    This is a notifier signal for fetchServerInfo(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOTAClient::updateFinished(bool success)

    This is a notifier signal for update(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOTAClient::rollbackFinished(bool success)

    This is a notifier signal for rollback(). The \a success argument
    indicates whether the operation was successful.
*/

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

/*!
    Fetch OTA metadata from a server and update the local cache. This
    metadata contains information on what system version is available on a
    server. The cache is persistent as it is stored on the disk.

    This method is asynchronous and returns immediately, see fetchServerInfoFinished().

    \sa updateAvailable(), serverInfo()
*/
bool QOTAClient::fetchServerInfo() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled)
        return false;

    d->m_otaAsync->fetchServerInfo();
    return true;
}

/*!
    Fetch an OTA update from a server and perform the system update.

    This method is asynchronous and returns immediately, see updateFinished().

    \sa fetchServerInfo(), updateAvailable(), restartRequired()
*/
bool QOTAClient::update() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled || !updateAvailable())
        return false;

    d->m_otaAsync->update(d->m_serverRev);
    return true;
}

/*!
    Rollback to the previous system version.

    This method is asynchronous and returns immediately, see rollbackFinished().

    \sa restartRequired()
*/
bool QOTAClient::rollback() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled || !d->m_initialized)
        return false;

    d->m_otaAsync->rollback();
    return true;
}

/*!
    \property QOTAClient::otaEnabled
    \brief whether a device supports OTA updates.
*/
bool QOTAClient::otaEnabled() const
{
    Q_D(const QOTAClient);
    return d->m_otaEnabled;
}

/*!
    \property QOTAClient::initialized
    \brief whether the object has completed the initialization.

    When an object of this class is created, it asynchronously (from a non-GUI thread)
    pre-populates the internal state and sets the initialized property accordingly, see
    initializationFinished().

    Initialization is fast if there are no other processes locking access to the OSTree
    repository on a device. This could happen if there is some other process currently
    writing to the OSTree repository, for example a daemon calling fetchServerInfo().

    \sa initializationFinished()
*/
bool QOTAClient::initialized() const
{
    Q_D(const QOTAClient);
    return d->m_initialized;
}

/*!
    \property QOTAClient::error
    \brief a string containing the last error occurred.
*/
QString QOTAClient::errorString() const
{
    Q_D(const QOTAClient);
    return d->m_error;
}

/*!
    \property QOTAClient::updateAvailable
    \brief whether a system update is available.

    Holds a bool indicating the availability of a system update. This information
    is cached - to update the local cache call fetchServerInfo().
*/
bool QOTAClient::updateAvailable() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled || d->m_serverRev.isEmpty())
        return false;

    return d->m_updateAvailable;
}

/*!
    \property QOTAClient::restartRequired
    \brief whether a reboot is required.

    Holds a bool indicating whether a reboot is required. A reboot is required
    after update() and rollback() to boot into the new default system.

*/
bool QOTAClient::restartRequired() const
{
    Q_D(const QOTAClient);
    if (!d->m_otaEnabled)
        return false;

    return d->m_restartRequired;
}

/*!
    \property QOTAClient::clientVersion
    \brief a QString containing the booted system's version.

    This is a convenience method.
    \sa clientInfo()
*/
QString QOTAClient::clientVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::clientDescription
    \brief a QString containing the booted system's description.

    This is a convenience method.
    \sa clientInfo()
*/
QString QOTAClient::clientDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::clientRevision
    \brief a QString containing the booted system's revision.

    A checksum in the OSTree repository.
*/
QString QOTAClient::clientRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::clientInfo
    \brief a QByteArray containing the booted system's OTA metadata.

    Returns JSON formatted QByteArray containing OTA metadata for the booted
    system. This metadata is bundled with each system's version.
*/
QByteArray QOTAClient::clientInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::serverVersion
    \brief a QString containing the system's version on a server.

    This is a convenience method.
    \sa serverInfo()
*/
QString QOTAClient::serverVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Server);
}

/*!
    \property QOTAClient::serverDescription
    \brief a QString containing the system's description on a server.

    This is a convenience method.
    \sa serverInfo()
*/
QString QOTAClient::serverDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Server);
}

/*!
    \property QOTAClient::serverRevision
    \brief a QString containing the system's revision on a server.

    A checksum in the OSTree repository.
*/
QString QOTAClient::serverRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Server);
}

/*!
    \property QOTAClient::serverInfo
    \brief a QByteArray containing the system's OTA metadata on a server.

    Returns JSON formatted QByteArray containing OTA metadata for the system
    on a server. This metadata is bundled with each system's version.
*/
QByteArray QOTAClient::serverInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Server);
}

/*!
    \property QOTAClient::rollbackVersion
    \brief a QString containing the rollback system's version.

    This is a convenience method.
    \sa rollbackInfo()
*/
QString QOTAClient::rollbackVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Rollback);
}

/*!
    \property QOTAClient::rollbackDescription
    \brief a QString containing the rollback system's description.

    This is a convenience method.
    \sa rollbackInfo()
*/
QString QOTAClient::rollbackDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Rollback);
}

/*!
    \property QOTAClient::rollbackRevision
    \brief a QString containing the rollback system's revision.

    A checksum in the OSTree repository.
*/
QString QOTAClient::rollbackRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Rollback);
}

/*!
    \property QOTAClient::rollbackInfo
    \brief a QByteArray containing the rollback system's OTA metadata.

    Returns JSON formatted QByteArray containing OTA metadata for the roolback
    system. This metadata is bundled with each system's version.
*/
QByteArray QOTAClient::rollbackInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Rollback);
}

QT_END_NAMESPACE
