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
#include "qotarepositoryconfig_p.h"
#include "qotarepositoryconfig.h"
#include "qotaclient.h"

#include <QtCore/QFile>
#include <QtCore/QJsonObject>
#include <QtCore/QDir>
#include <QtCore/QThread>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qota, "b2qt.ota", QtWarningMsg)

const QString repoConfigPath(QStringLiteral("/etc/ostree/remotes.d/qt-os.conf"));

QOtaClientPrivate::QOtaClientPrivate(QOtaClient *client) :
    q_ptr(client),
    m_updateAvailable(false),
    m_rollbackAvailable(false),
    m_restartRequired(false)
{
    // https://github.com/ostreedev/ostree/issues/480
    m_otaEnabled = QFile().exists(QStringLiteral("/ostree/deploy"));
    if (m_otaEnabled) {
        m_otaAsyncThread = new QThread();
        m_otaAsyncThread->start();
        m_otaAsync.reset(new QOtaClientAsync());
        m_otaAsync->moveToThread(m_otaAsyncThread);
    }
}

QOtaClientPrivate::~QOtaClientPrivate()
{
    if (m_otaEnabled) {
        if (m_otaAsyncThread->isRunning()) {
            m_otaAsyncThread->quit();
            if (!Q_UNLIKELY(m_otaAsyncThread->wait(4000)))
                qCWarning(qota) << "Timed out waiting for worker thread to exit.";
        }
        delete m_otaAsyncThread;
    }
}

void QOtaClientPrivate::handleStateChanges()
{
    Q_Q(QOtaClient);

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

void QOtaClientPrivate::setBootedMetadata(const QString &bootedRev, const QString &bootedMetadata)
{
    Q_Q(QOtaClient);
    m_bootedRev = bootedRev;
    m_bootedMetadata = bootedMetadata;
}

void QOtaClientPrivate::statusStringChanged(const QString &status)
{
    Q_Q(QOtaClient);
    m_status = status;
    emit q->statusStringChanged(m_status);
}

void QOtaClientPrivate::errorOccurred(const QString &error)
{
    Q_Q(QOtaClient);
    m_error = error;
    emit q->errorOccurred(m_error);
}

bool QOtaClientPrivate::verifyPathExist(const QString &path)
{
    if (!QDir().exists(path)) {
        errorOccurred(path + QLatin1String(" does not exist"));
        return false;
    }
    return true;
}

void QOtaClientPrivate::rollbackMetadataChanged(const QString &rollbackRev, const QString &rollbackMetadata, int treeCount)
{
    Q_Q(QOtaClient);
    if (m_rollbackRev == rollbackRev)
        return;

    if (!m_rollbackAvailable && treeCount > 1) {
        m_rollbackAvailable = true;
        emit q->rollbackAvailableChanged();
    }

    m_rollbackRev = rollbackRev;
    m_rollbackMetadata = rollbackMetadata;

    q->rollbackMetadataChanged();
}

void QOtaClientPrivate::remoteMetadataChanged(const QString &remoteRev, const QString &remoteMetadata)
{
    Q_Q(QOtaClient);
    if (m_remoteRev == remoteRev)
        return;

    m_remoteRev = remoteRev;
    m_remoteMetadata = remoteMetadata;
    handleStateChanges();

    emit q->remoteMetadataChanged();
}

void QOtaClientPrivate::defaultRevisionChanged(const QString &defaultRevision)
{
    if (m_defaultRev == defaultRevision)
        return;

    m_defaultRev = defaultRevision;
    handleStateChanges();
}

/*!
    \class QOtaClient
    \inmodule qtotaupdate
    \brief Main interface to the OTA functionality.

    QOtaClient
//! [client-description]
    provides an API to execute Over-the-Air update tasks. Offline
    operations include querying the booted and rollback system version details,
    and atomically performing rollbacks. Online operations include fetching a
    new system version from a remote server, and atomically performing system
    updates. Using this API is safe and won't leave the system in an inconsistent
    state, even if the power fails half-way through.

    A remote needs to be configured for a device to be able to locate a server
    that is hosting an OTA update, see setRepositoryConfig().

    When utilizing this API from several processes, precautions need to be taken
    to ensure that the processes' view of the system state and metadata is up to date.
    This can be achieved by using any IPC mechanism of choice to ensure that this
    information is being modified by only a single process at a time.

    Methods that modify the system's state and/or metadata are marked as such. In a
    multi-process scenario, refreshMetadata() updates the processes' view of the system state
    and metadata. A typical example would be a daemon that periodically checks a remote
    server (with fetchRemoteMetadata()) for system updates, and then uses IPC (such as a push
    notification) to let the system's main GUI know when a new version is available.

//! [client-description]
*/

/*!
    \fn void QOtaClient::fetchRemoteMetadataFinished(bool success)

    A notifier signal for fetchRemoteMetadata(). The \a success argument indicates
    whether the operation was successful.
*/

/*!
    \fn void QOtaClient::updateFinished(bool success)

    A notifier signal for update(). The \a success argument indicates whether
    the operation was successful.
*/

/*!
    \fn void QOtaClient::rollbackFinished(bool success)

    This is a notifier signal for rollback(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOtaClient::updateOfflineFinished(bool success)

    This is a notifier signal for updateOffline(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOtaClient::updateRemoteMetadataOfflineFinished(bool success)

    This is a notifier signal for updateRemoteMetadataOffline(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOtaClient::remoteMetadataChanged()
//! [remotemetadatachanged-description]
    Remote metadata can change when calling fetchRemoteMetadata() or updateRemoteMetadataOffline().
    If OTA metadata on the remote server is different from the local cache, the local
    cache is updated and this signal is emitted.
//! [remotemetadatachanged-description]
*/

/*!
    \fn void QOtaClient::rollbackMetadataChanged()

    This signal is emitted when rollback metadata changes. Rollback metadata changes
    when calling rollback().
*/

/*!
    \fn void QOtaClient::updateAvailableChanged(bool available)

    This signal is emitted when the value of updateAvailable changes. The
    \a available argument holds whether a system update is available for
    the default system.
*/

/*!
    \fn void QOtaClient::rollbackAvailableChanged()

    This signal is emitted when the value of rollbackAvailable changes. A rollback
    system becomes available when a device has performed at least one system update.
*/

/*!
    \fn void QOtaClient::restartRequiredChanged(bool required)

    This signal is emitted when the value of restartRequired changes. The
    \a required argument holds whether a reboot is required.
*/

/*!
    \fn void QOtaClient::statusStringChanged(const QString &status)
//! [statusstringchanged-description]
    This signal is emitted when an additional status information is available
    (for example, when a program is busy performing a long operation). The
    \a status argument holds the status message.
//! [statusstringchanged-description]
*/

/*!
    \fn void QOtaClient::errorOccurred(const QString &error)

    This signal is emitted when an error occurs. The \a error argument holds
    the error message.
*/

/*!
    \fn void QOtaClient::repositoryConfigChanged(QOtaRepositoryConfig *repository)

    This signal is emitted when the configuration file was updated (\a repository
    holds a pointer to the new configuration) or removed (\a repository holds the
    \c nullptr value).
*/

QOtaClient::QOtaClient() :
    d_ptr(new QOtaClientPrivate(this))
{
    Q_D(QOtaClient);
    if (d->m_otaEnabled) {
        QOtaClientAsync *async = d->m_otaAsync.data();
        connect(async, &QOtaClientAsync::fetchRemoteMetadataFinished, this, &QOtaClient::fetchRemoteMetadataFinished);
        connect(async, &QOtaClientAsync::updateFinished, this, &QOtaClient::updateFinished);
        connect(async, &QOtaClientAsync::rollbackFinished, this, &QOtaClient::rollbackFinished);
        connect(async, &QOtaClientAsync::updateOfflineFinished, this, &QOtaClient::updateOfflineFinished);
        connect(async, &QOtaClientAsync::updateRemoteMetadataOfflineFinished, this, &QOtaClient::updateRemoteMetadataOfflineFinished);
        connect(async, &QOtaClientAsync::errorOccurred, d, &QOtaClientPrivate::errorOccurred);
        connect(async, &QOtaClientAsync::statusStringChanged, d, &QOtaClientPrivate::statusStringChanged);
        connect(async, &QOtaClientAsync::rollbackMetadataChanged, d, &QOtaClientPrivate::rollbackMetadataChanged);
        connect(async, &QOtaClientAsync::remoteMetadataChanged, d, &QOtaClientPrivate::remoteMetadataChanged);
        connect(async, &QOtaClientAsync::defaultRevisionChanged, d, &QOtaClientPrivate::defaultRevisionChanged);
        d->m_otaAsync->refreshMetadata(d);
    }
}

QOtaClient::~QOtaClient()
{
    delete d_ptr;
}

/*!
    Returns a singleton instance of QOtaClient.
*/
QOtaClient& QOtaClient::instance()
{
    static QOtaClient otaClient;
    return otaClient;
}

/*!
//! [fetchremotemetadata-description]
    Fetches OTA metadata from a remote server and updates the local metadata
    cache. This metadata contains information on what system version is available
    on a server. The cache is persistent as it is stored on the disk.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \note This method mutates system's state/metadata.
//! [fetchremotemetadata-description]

    \sa fetchRemoteMetadataFinished(), updateAvailable, remoteMetadata
*/
bool QOtaClient::fetchRemoteMetadata()
{
    Q_D(QOtaClient);
    if (!d->m_otaEnabled)
        return false;

    d->m_otaAsync->fetchRemoteMetadata();
    return true;
}

/*!
//! [update-description]
    Fetches an OTA update from a remote and performs the system update.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \note This method mutates system's state/metadata.
//! [update-description]

    \sa updateFinished(), fetchRemoteMetadata, restartRequired, setRepositoryConfig
*/
bool QOtaClient::update()
{

    Q_D(const QOtaClient);
    if (!d->m_otaEnabled || !updateAvailable())
        return false;

    d->m_otaAsync->update(d->m_remoteRev);
    return true;
}

/*!
    Rollback to the previous system version.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \note This method mutates system's state/metadata.
    \sa rollbackFinished(), restartRequired
*/
bool QOtaClient::rollback()
{
    Q_D(QOtaClient);
    if (!d->m_otaEnabled)
        return false;

    d->m_otaAsync->rollback();
    return true;
}

/*!
//! [update-offline]
    Uses the provided self-contained update package to update the system.
    Updates the local metadata cache, if it has not been already updated
    by calling updateRemoteMetadataOffline().

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully. The \a packagePath
    argument holds a path to the update package.

    \note This method mutates system's state/metadata.
//! [update-offline]

    \sa updateOfflineFinished()
*/
bool QOtaClient::updateOffline(const QString &packagePath)
{
    Q_D(QOtaClient);
    if (!d->m_otaEnabled)
        return false;

    QString package = QFileInfo(packagePath).absoluteFilePath();
    if (!d->verifyPathExist(package))
        return false;

    d->m_otaAsync->updateOffline(package);
    return true;
}

/*!
//! [update-remote-offline]
    Uses the provided self-contained update package to update local metadata cache.
    This metadata contains information on what system version is available on a server.
    The cache is persistent as it is stored on the disk. This method is an offline
    counterpart for fetchRemoteMetadata().

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully. The \a packagePath
    argument holds a path to the update package.

    \note This method mutates system's state/metadata.
//! [update-remote-offline]

    \sa remoteMetadataChanged
*/
bool QOtaClient::updateRemoteMetadataOffline(const QString &packagePath)
{
    Q_D(QOtaClient);
    if (!d->m_otaEnabled)
        return false;

    QFileInfo package(packagePath);
    if (!package.exists()) {
        d->errorOccurred(QString(QStringLiteral("The package %1 does not exist"))
                         .arg(package.absoluteFilePath()));
        return false;
    }

    d->m_otaAsync->updateRemoteMetadataOffline(package.absoluteFilePath());
    return true;
}

/*!
//! [refresh-metadata]
    Refreshes the instances view of the system's state from the local metadata cache.
    Notifier signals are emitted for properties that depend on changed metadata.
    Returns \c true if metadata is refreshed successfully; otherwise returns \c false.

    Using this method is not required when only one process is responsible for all OTA tasks.

//! [refresh-metadata]
*/
bool QOtaClient::refreshMetadata()
{
    Q_D(QOtaClient);
    if (!d->m_otaEnabled)
        return false;

    return d->m_otaAsync->refreshMetadata();
}

/*!
//! [remove-repository-config]
    Remove a configuration file for the repository.

    The repository configuration is stored on a file system in \c {/etc/ostree/remotes.d/*.conf}

    If the configuration file does not exist, this function returns \c true.
    If the configuration file exists, this function returns \c true if the file
    is removed successfully; otherwise returns \c false.
//! [remove-repository-config]

    \sa setRepositoryConfig(), repositoryConfigChanged
*/
bool QOtaClient::removeRepositoryConfig()
{
    Q_D(QOtaClient);
    if (!otaEnabled() || !QDir().exists(repoConfigPath))
        return true;

    bool removed = QDir().remove(repoConfigPath);
    if (removed)
        emit repositoryConfigChanged(nullptr);
    else
        d->errorOccurred(QStringLiteral("Failed to remove repository configuration"));

    return removed;
}

/*!
//! [is-repository-config-set]

    Returns \c true if the configuration \a config is already set; otherwise returns \c false.

//! [is-repository-config-set]
*/
bool QOtaClient::isRepositoryConfigSet(QOtaRepositoryConfig *config) const
{
    QOtaRepositoryConfig *currentConfig = repositoryConfig();

    bool isSet = currentConfig && config && currentConfig->url() == config->url() &&
                 currentConfig->gpgVerify() == config->gpgVerify() &&
                 currentConfig->tlsPermissive() == config->tlsPermissive() &&
                 currentConfig->tlsClientCertPath() == config->tlsClientCertPath() &&
                 currentConfig->tlsClientKeyPath() == config->tlsClientKeyPath() &&
                 currentConfig->tlsCaPath() == config->tlsCaPath();

    return isSet;
}

/*!
//! [set-repository-config]
    Change the configuration for the repository. The repository configuration
    is stored on a file system in \c {/etc/ostree/remotes.d/*.conf}

    Returns \c true if the configuration file is changed successfully;
    otherwise returns \c false.
//! [set-repository-config]

    The \a config argument is documented in QOtaRepositoryConfig.

    \sa removeRepositoryConfig(), repositoryConfigChanged
*/
bool QOtaClient::setRepositoryConfig(QOtaRepositoryConfig *config)
{
    Q_D(QOtaClient);
    if (!d->m_otaEnabled || !config)
        return false;

    if (QDir().exists(repoConfigPath)) {
        d->errorOccurred(QStringLiteral("Repository configuration already exists"));
        return false;
    }
    // URL
    if (config->url().isEmpty()) {
        d->errorOccurred(QStringLiteral("Repository URL can not be empty"));
        return false;
    }

    // TLS client certs
    int tlsClientArgs = 0;
    if (!config->tlsClientCertPath().isEmpty()) {
        if (!d->verifyPathExist(config->tlsClientCertPath()))
            return false;
        ++tlsClientArgs;
    }
    if (!config->tlsClientKeyPath().isEmpty()) {
        if (!d->verifyPathExist(config->tlsClientKeyPath()))
            return false;
        ++tlsClientArgs;
    }
    if (tlsClientArgs == 1) {
        d->errorOccurred(QStringLiteral("Both tlsClientCertPath and tlsClientKeyPath are required"
                                        " for TLS client authentication functionality"));
        return false;
    }

    // FORMAT: ostree remote add [OPTION...] NAME URL [BRANCH...]
    QString cmd(QStringLiteral("ostree remote add"));
    // GPG
    cmd.append(QStringLiteral(" --set=gpg-verify="));
    config->gpgVerify() ? cmd.append(QStringLiteral("true")) : cmd.append(QStringLiteral("false"));
    // TLS client authentication
    if (!config->tlsClientCertPath().isEmpty()) {
        cmd.append(QStringLiteral(" --set=tls-client-cert-path="));
        cmd.append(config->tlsClientCertPath());
        cmd.append(QStringLiteral(" --set=tls-client-key-path="));
        cmd.append(config->tlsClientKeyPath());
    }
    // TLS server authentication
    cmd.append(QStringLiteral(" --set=tls-permissive="));
    config->tlsPermissive() ? cmd.append(QStringLiteral("true")) : cmd.append(QStringLiteral("false"));
    if (!config->tlsCaPath().isEmpty()) {
        if (!d->verifyPathExist(config->tlsCaPath()))
            return false;
        cmd.append(QStringLiteral(" --set=tls-ca-path="));
        cmd.append(config->tlsCaPath());
    }
    // NAME URL [BRANCH...]
    cmd.append(QString(QStringLiteral(" qt-os %1 linux/qt")).arg(config->url()));

    bool ok = true;
    d->m_otaAsync->ostree(cmd, &ok);
    if (ok)
        emit repositoryConfigChanged(config);

    return ok;
}

/*!
    Returns a configuration object for the repository or \c nullptr if the
    configuration file does not exist or could not be read. The caller is
    responsible for deleting the returned object.

    \sa setRepositoryConfig(), removeRepositoryConfig()
*/
QOtaRepositoryConfig *QOtaClient::repositoryConfig() const
{
    if (!otaEnabled())
        return nullptr;
    return QOtaRepositoryConfig().d_func()->repositoryConfigFromFile(repoConfigPath);
}

/*!
    \property QOtaClient::otaEnabled
    \brief whether a device supports OTA updates.
*/
bool QOtaClient::otaEnabled() const
{
    Q_D(const QOtaClient);
    return d->m_otaEnabled;
}

/*!
    \property QOtaClient::error
    \brief a string containing the last error occurred.
*/
QString QOtaClient::errorString() const
{
    Q_D(const QOtaClient);
    return d->m_error;
}

/*!
    \property QOtaClient::status
    \brief a string containing the last status message.

    Only selected operations update this property.
*/
QString QOtaClient::statusString() const
{
    Q_D(const QOtaClient);
    return d->m_status;
}

/*!
    \property QOtaClient::updateAvailable
    \brief whether a system update is available.

    This information is cached; to update the local cache, call
    fetchRemoteMetadata().
*/
bool QOtaClient::updateAvailable() const
{
    Q_D(const QOtaClient);
    if (d->m_updateAvailable)
        Q_ASSERT(!d->m_remoteRev.isEmpty());
    return d->m_updateAvailable;
}

/*!
    \property QOtaClient::rollbackAvailable
    \brief whether a rollback system is available.

    \sa rollbackAvailableChanged()
*/
bool QOtaClient::rollbackAvailable() const
{
    Q_D(const QOtaClient);
    return d->m_rollbackAvailable;
}

/*!
    \property QOtaClient::restartRequired
    \brief whether a reboot is required.

    Reboot is required after update(), updateOffline() and rollback(),
    to boot into the new default system.

*/
bool QOtaClient::restartRequired() const
{
    Q_D(const QOtaClient);
    return d->m_restartRequired;
}

/*!
    \property QOtaClient::bootedRevision
    \brief a QString containing the booted system's revision.

    A booted revision is a checksum in the OSTree repository.
*/
QString QOtaClient::bootedRevision() const
{
    return d_func()->m_bootedRev;
}

/*!
    \property QOtaClient::bootedMetadata
    \brief a QString containing the booted system's OTA metadata.

    Returns a JSON-formatted QString containing OTA metadata for the booted
    system. Metadata is bundled with each system's version.
*/
QString QOtaClient::bootedMetadata() const
{
    return d_func()->m_bootedMetadata;
}

/*!
    \property QOtaClient::remoteRevision
    \brief a QString containing the system's revision on a server.

    A checksum in the OSTree repository.
*/
QString QOtaClient::remoteRevision() const
{
    return d_func()->m_remoteRev;
}

/*!
    \property QOtaClient::remoteMetadata
    \brief a QString containing the system's OTA metadata on a server.

    Returns a JSON-formatted QString containing OTA metadata for the system
    on a server. Metadata is bundled with each system's version.

    \sa fetchRemoteMetadata()
*/
QString QOtaClient::remoteMetadata() const
{
    return d_func()->m_remoteMetadata;
}

/*!
    \property QOtaClient::rollbackRevision
    \brief a QString containing the rollback system's revision.

    A checksum in the OSTree repository.
*/
QString QOtaClient::rollbackRevision() const
{
    return d_func()->m_rollbackRev;
}

/*!
    \property QOtaClient::rollbackMetadata
    \brief a QString containing the rollback system's OTA metadata.

    Returns a JSON-formatted QString containing OTA metadata for the rollback
    system. Metadata is bundled with each system's version.

    \sa rollback()
*/
QString QOtaClient::rollbackMetadata() const
{
    return d_func()->m_rollbackMetadata;
}

QT_END_NAMESPACE
