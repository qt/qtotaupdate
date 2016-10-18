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

Q_LOGGING_CATEGORY(qota, "b2qt.ota")

QT_BEGIN_NAMESPACE

QOTAClientPrivate::QOTAClientPrivate(QOTAClient *client) :
    q_ptr(client),
    m_initialized(false),
    m_updateAvailable(false),
    m_rollbackAvailable(false),
    m_restartRequired(false)
{
    // https://github.com/ostreedev/ostree/issues/480
    m_otaEnabled = QFile().exists(QStringLiteral("/ostree/deploy"));
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
                qCWarning(qota) << "Timed out waiting for worker thread to exit.";
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

void QOTAClientPrivate::updateRemoteInfo(const QString &remoteRev, const QJsonDocument &remoteInfo)
{
    Q_Q(QOTAClient);
    if (m_remoteRev == remoteRev)
        return;

    m_remoteRev = remoteRev;
    updateInfoMembers(remoteInfo, &m_remoteInfo, &m_remoteVersion, &m_remoteDescription);
    emit q->remoteInfoChanged();
}

bool QOTAClientPrivate::isReady() const
{
    if (!m_otaEnabled) {
        qCWarning(qota) << "over-the-air update functionality is not enabled for this device";
        return false;
    }
    if (!m_initialized) {
        qCWarning(qota) << "initialization is not ready";
        return false;
    }
    return true;
}

void QOTAClientPrivate::refreshState()
{
    Q_Q(QOTAClient);

    bool updateAvailable = m_defaultRev != m_remoteRev && m_remoteRev != m_bootedRev;
    if (m_updateAvailable != updateAvailable) {
        m_updateAvailable = updateAvailable;
        emit q->updateAvailableChanged(m_updateAvailable);
    }

    bool restartRequired = m_bootedRev != m_defaultRev;
    if (m_restartRequired != restartRequired) {
        m_restartRequired = restartRequired;
        emit q->restartRequiredChanged(m_restartRequired);
    }
}

void QOTAClientPrivate::initializeFinished(const QString &defaultRev,
                                           const QString &bootedRev, const QJsonDocument &bootedInfo,
                                           const QString &remoteRev, const QJsonDocument &remoteInfo)
{
    Q_Q(QOTAClient);
    m_defaultRev = defaultRev;
    m_bootedRev = bootedRev;
    updateInfoMembers(bootedInfo, &m_bootedInfo, &m_bootedVersion, &m_bootedDescription);
    updateRemoteInfo(remoteRev, remoteInfo);
    refreshState();
    m_initialized = true;
    emit q->initializationFinished();
}

void QOTAClientPrivate::fetchRemoteInfoFinished(const QString &remoteRev, const QJsonDocument &remoteInfo, bool success)
{
    Q_Q(QOTAClient);
    if (success) {
        updateRemoteInfo(remoteRev, remoteInfo);
        refreshState();
    }
    emit q->fetchRemoteInfoFinished(success);
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

void QOTAClientPrivate::rollbackChanged(const QString &rollbackRev, const QJsonDocument &rollbackInfo, int treeCount)
{
    Q_Q(QOTAClient);
    if (!m_rollbackAvailable && treeCount > 1) {
        m_rollbackAvailable = true;
        emit q->rollbackAvailableChanged();
    }
    m_rollbackRev = rollbackRev;
    updateInfoMembers(rollbackInfo, &m_rollbackInfo, &m_rollbackVersion, &m_rollbackDescription);
    q->rollbackInfoChanged();
}

QString QOTAClientPrivate::version(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QString();

    switch (target) {
    case QueryTarget::Booted:
        return m_bootedVersion.isEmpty() ? QStringLiteral("unknown") : m_bootedVersion;
    case QueryTarget::Remote:
        return m_remoteVersion.isEmpty() ? QStringLiteral("unknown") : m_remoteVersion;
    case QueryTarget::Rollback:
        return m_rollbackVersion.isEmpty() ? QStringLiteral("unknown") : m_rollbackVersion;
    default:
        Q_UNREACHABLE();
    }
}

QByteArray QOTAClientPrivate::info(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QByteArray();

    switch (target) {
    case QueryTarget::Booted:
        return m_bootedInfo;
    case QueryTarget::Remote:
        return m_remoteInfo;
    case QueryTarget::Rollback:
        return m_rollbackInfo;
    default:
        Q_UNREACHABLE();
    }
}

QString QOTAClientPrivate::description(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QString();

    switch (target) {
    case QueryTarget::Booted:
        return m_bootedDescription.isEmpty() ? QStringLiteral("unknown") : m_bootedDescription;
    case QueryTarget::Remote:
        return m_remoteDescription.isEmpty() ? QStringLiteral("unknown") : m_remoteDescription;
    case QueryTarget::Rollback:
        return m_rollbackDescription.isEmpty() ? QStringLiteral("unknown") : m_rollbackDescription;
    default:
        Q_UNREACHABLE();
    }
}

QString QOTAClientPrivate::revision(QueryTarget target) const
{
    if (!m_otaEnabled)
        return QString();

    switch (target) {
    case QueryTarget::Booted:
        return m_bootedRev.isEmpty() ? QStringLiteral("unknown") : m_bootedRev;
    case QueryTarget::Remote:
        return m_remoteRev.isEmpty() ? QStringLiteral("unknown") : m_remoteRev;
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
    \fn void QOTAClient::fetchRemoteInfoFinished(bool success)

    A notifier signal for fetchRemoteInfo(). The \a success argument indicates
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
    \fn void QOTAClient::remoteInfoChanged()
//! [remoteinfochanged-description]
    Remote info can change when calling fetchRemoteInfo(). If OTA metadata on
    the remote server is different from the local cache, the local cache is updated
    and this signal is emitted.
//! [remoteinfochanged-description]
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
    \fn void QOTAClient::rollbackAvailableChanged()

    This signal is emitted when the value of rollbackAvailable changes. A rollback
    system becomes available when a device has performed at least one system update.
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
        connect(async, &QOTAClientAsync::fetchRemoteInfoFinished, d, &QOTAClientPrivate::fetchRemoteInfoFinished);
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
//! [fetchremoteinfo-description]
    Fetches OTA metadata from a remote server and updates the local cache. This
    metadata contains information on what system version is available on a
    server. The cache is persistent as it is stored on the disk.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.
//! [fetchremoteinfo-description]

    \sa fetchRemoteInfoFinished(), updateAvailable, remoteInfo
*/
bool QOTAClient::fetchRemoteInfo() const
{
    Q_D(const QOTAClient);
    if (!d->isReady())
        return false;

    d->m_otaAsync->fetchRemoteInfo();
    return true;
}

/*!
//! [update-description]
    Fetches an OTA update from a remote and performs the system update.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.
//! [update-description]

    \sa updateFinished(), fetchRemoteInfo(), restartRequired
*/
bool QOTAClient::update() const
{
    Q_D(const QOTAClient);
    if (!d->isReady() || !updateAvailable())
        return false;

    d->m_otaAsync->update(d->m_remoteRev);
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
    if (!d->isReady())
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
    fetchRemoteInfo().
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
    fetchRemoteInfo().
*/
bool QOTAClient::updateAvailable() const
{
    Q_D(const QOTAClient);
    if (d->m_updateAvailable)
        Q_ASSERT(!d->m_remoteRev.isEmpty());
    return d->m_updateAvailable;
}

/*!
    \property QOTAClient::rollbackAvailable
    \brief whether a rollback system is available.

    \sa rollbackAvailableChanged()
*/
bool QOTAClient::rollbackAvailable() const
{
    Q_D(const QOTAClient);
    return d->m_rollbackAvailable;
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
    return d->m_restartRequired;
}

/*!
    \property QOTAClient::bootedVersion
    \brief a QString containing the booted system's version.

    This is a convenience method.

    \sa bootedInfo
*/
QString QOTAClient::bootedVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Booted);
}

/*!
    \property QOTAClient::bootedDescription
    \brief a QString containing the booted system's description.

    This is a convenience method.

    \sa bootedInfo
*/
QString QOTAClient::bootedDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Booted);
}

/*!
    \property QOTAClient::bootedRevision
    \brief a QString containing the booted system's revision.

    A booted revision is a checksum in the OSTree repository.
*/
QString QOTAClient::bootedRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Booted);
}

/*!
    \property QOTAClient::bootedInfo
    \brief a QByteArray containing the booted system's OTA metadata.

    Returns a JSON-formatted QByteArray containing OTA metadata for the booted
    system. Metadata is bundled with each system's version.
*/
QByteArray QOTAClient::bootedInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Booted);
}

/*!
    \property QOTAClient::remoteVersion
    \brief a QString containing the system's version on a server.

    This is a convenience method.

    \sa remoteInfo
*/
QString QOTAClient::remoteVersion() const
{
    return d_func()->version(QOTAClientPrivate::QueryTarget::Remote);
}

/*!
    \property QOTAClient::remoteDescription
    \brief a QString containing the system's description on a server.

    This is a convenience method.

    \sa remoteInfo
*/
QString QOTAClient::remoteDescription() const
{
    return d_func()->description(QOTAClientPrivate::QueryTarget::Remote);
}

/*!
    \property QOTAClient::remoteRevision
    \brief a QString containing the system's revision on a server.

    A checksum in the OSTree repository.
*/
QString QOTAClient::remoteRevision() const
{
    return d_func()->revision(QOTAClientPrivate::QueryTarget::Remote);
}

/*!
    \property QOTAClient::remoteInfo
    \brief a QByteArray containing the system's OTA metadata on a server.

    Returns a JSON-formatted QByteArray containing OTA metadata for the system
    on a server. Metadata is bundled with each system's version.

    \sa fetchRemoteInfo()
*/
QByteArray QOTAClient::remoteInfo() const
{
    return d_func()->info(QOTAClientPrivate::QueryTarget::Remote);
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
