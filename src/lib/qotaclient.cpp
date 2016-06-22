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

void QOTAClientPrivate::statusStringChanged(const QString &status)
{
    Q_Q(QOTAClient);
    m_status = status;
    emit q->statusStringChanged(m_status);
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

    QOTAClient
//! [client-description]
    provides an API to execute Over-the-Air update tasks. Offline
    operations include  querying the booted and rollback system version details,
    and atomically performing rollbacks. Online operations include fetching a
    new system version from a remote server, and atomically performing system
    updates.

    Using this API is safe and won't leave the system in an inconsistent state,
    even if the power fails half-way through.

    \b {Remote Configuration}

    A remote needs to be configured for a device to be able to locate a server
    that is hosting an OTA update. A Tech Preview release does not provide Qt
    API to configure remotes. To configure a remote, it is necessary to use the
    ostree command line tool. Examples for remote configurations:

    No Security:
    \badcode
    ostree remote add --no-gpg-verify qt-os http://${SERVER_ADDRESS}:${PORT}/ostree-repo linux/qt
    \endcode

    Using GPG Signing:
    \badcode
    ostree remote add --set=gpg-verify=true qt-os http://${SERVER_ADDRESS}:${PORT}/ostree-repo linux/qt
    \endcode

    Using TLS Authentication:
    \badcode
    ostree remote add \
    --tls-client-cert-path /path/client.crt \
    --tls-client-key-path /path/client.key \
    --tls-ca-path /trusted/server.crt qt-os https://${SERVER_ADDRESS}:${PORT}/ostree-repo linux/qt
    \endcode

    Above, \c ${SERVER_ADDRESS} is the server where you have exported the
    OSTree repository, and \c ${PORT} is the port number.
//! [client-description]
*/

/*!
    \fn void QOTAClient::fetchServerInfoFinished(bool success)

    A notifier signal for fetchServerInfo(). The \a success argument indicates
    whether the operation was successful.
*/

/*!
    \fn void QOTAClient::updateFinished(bool success)

    A notifier signal for update(). The \a success argument indicates whether
    the operation was successful.
*/

/*!
    \fn void QOTAClient::rollbackFinished(bool success)

    This is a notifier signal for rollback(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOTAClient::serverInfoChanged()
//! [serverinfochanged-description]
    Server info can change when calling fetchServerInfo(). If OTA metadata on
    the server is different from the local cache, the local cache is updated
    and this signal is emitted.
//! [serverinfochanged-description]
*/

/*!
    \fn void QOTAClient::rollbackInfoChanged()

    This signal is emitted when rollback info changes. Rollback info changes
    when calling rollback().
*/

/*!
    \fn void QOTAClient::updateAvailableChanged(bool available)

    This signal is emitted when the value of updateAvailable changes. The
    \a available argument holds whether a system update is available for
    the default system.
*/

/*!
    \fn void QOTAClient::restartRequiredChanged(bool required)

    This signal is emitted when the value of restartRequired changes. The
    \a required argument holds whether a reboot is required.
*/

/*!
    \fn void QOTAClient::statusStringChanged(const QString &status)
//! [statusstringchanged-description]
    This signal is emitted when an additional status information is available
    (for example, when a program is busy performing a long operation). The
    \a status argument holds the status message.
//! [statusstringchanged-description]
*/

/*!
    \fn void QOTAClient::errorOccurred(const QString &error)

    This signal is emitted when an error occurs. The \a error argument holds
    the error message.
*/

/*!
    \fn void QOTAClient::initializationFinished()

    This signal is emitted when the object has finished initialization. The
    object is not ready for use until this signal is received.
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
        connect(async, &QOTAClientAsync::statusStringChanged, d, &QOTAClientPrivate::statusStringChanged);
        connect(async, &QOTAClientAsync::rollbackChanged, d, &QOTAClientPrivate::rollbackChanged);
        d->m_otaAsync->initialize();
    }
}

QOTAClient::~QOTAClient()
{
    delete d_ptr;
}

/*!
//! [fetchserverinfo-description]
    Fetches OTA metadata from a server and updates the local cache. This
    metadata contains information on what system version is available on a
    server. The cache is persistent as it is stored on the disk.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.
//! [fetchserverinfo-description]

    \sa fetchServerInfoFinished(), updateAvailable, serverInfo
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
//! [update-description]
    Fetches an OTA update from a server and performs the system update.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.
//! [update-description]

    \sa updateFinished(), fetchServerInfo(), restartRequired
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

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \sa rollbackFinished(), restartRequired
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
//! [initialized-description]
    \brief whether the object has completed the initialization.

    When an object of this class is created, it asynchronously (from a non-GUI
    thread) pre-populates the internal state, sets this property accordingly,
    and signals initializationFinished().

    Initialization is fast unless there is another process locking access to
    the OSTree repository on a device, for example, a daemon process calling
    fetchServerInfo().
//! [initialized-description]

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
    \property QOTAClient::status
    \brief a string containing the last status message.

    Only selected operations update this property.
*/
QString QOTAClient::statusString() const
{
    Q_D(const QOTAClient);
    return d->m_status;
}

/*!
    \property QOTAClient::updateAvailable
    \brief whether a system update is available.

    This information is cached; to update the local cache, call
    fetchServerInfo().
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

    Reboot is required after update() and rollback(), to boot into the new
    default system.

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

    \sa clientInfo
*/
QString QOTAClient::clientVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::clientDescription
    \brief a QString containing the booted system's description.

    This is a convenience method.

    \sa clientInfo
*/
QString QOTAClient::clientDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::clientRevision
    \brief a QString containing the booted system's revision.

    A client revision is a checksum in the OSTree repository.
*/
QString QOTAClient::clientRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::clientInfo
    \brief a QByteArray containing the booted system's OTA metadata.

    Returns a JSON-formatted QByteArray containing OTA metadata for the booted
    system. Metadata is bundled with each system's version.
*/
QByteArray QOTAClient::clientInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Client);
}

/*!
    \property QOTAClient::serverVersion
    \brief a QString containing the system's version on a server.

    This is a convenience method.

    \sa serverInfo
*/
QString QOTAClient::serverVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Server);
}

/*!
    \property QOTAClient::serverDescription
    \brief a QString containing the system's description on a server.

    This is a convenience method.

    \sa serverInfo
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

    Returns a JSON-formatted QByteArray containing OTA metadata for the system
    on a server. Metadata is bundled with each system's version.

    \sa fetchServerInfo()
*/
QByteArray QOTAClient::serverInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Server);
}

/*!
    \property QOTAClient::rollbackVersion
    \brief a QString containing the rollback system's version.

    This is a convenience method.

    \sa rollbackInfo
*/
QString QOTAClient::rollbackVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Rollback);
}

/*!
    \property QOTAClient::rollbackDescription
    \brief a QString containing the rollback system's description.

    This is a convenience method.

    \sa rollbackInfo
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

    Returns a JSON-formatted QByteArray containing OTA metadata for the rollback
    system. Metadata is bundled with each system's version.

    \sa rollback()
*/
QByteArray QOTAClient::rollbackInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Rollback);
}

QT_END_NAMESPACE
