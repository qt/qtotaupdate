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

    // The default system is used as the configuration merge source
    // when performing a system update.
    bool updateAvailable = m_defaultRev != m_remoteRev;
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

void QOtaClientPrivate::defaultRevisionChanged(const QString &defaultRevision, const QString &defaultMetadata)
{
    Q_Q(QOtaClient);
    if (m_defaultRev == defaultRevision)
        return;

    m_defaultRev = defaultRevision;
    m_defaultMetadata = defaultMetadata;
    handleStateChanges();

    emit q->defaultMetadataChanged();
}

/*!
    \inqmlmodule QtOtaUpdate
    \qmltype OtaClient
    \instantiates QOtaClient
    \brief Main interface to the OTA functionality.

    OtaClient
    \include qotaclient.cpp client-description
*/

/*!
    \class QOtaClient
    \inmodule qtotaupdate
    \brief Main interface to the OTA functionality.

    QOtaClient
//! [client-description]
    provides an API to administer the system's update routines. Offline operations
    include querying the booted, the default and the rollback system metadata,
    atomically updating the system from self-contained update packages and atomically
    performing system rollbacks. Online operations include fetching metadata
    for the system on a remote server and atomically performing system updates
    by fetching the update via http(s).

    Using this API is safe and won't leave the system in an inconsistent state,
    even if the power fails half-way through. A device needs to be configured for
    it to be able to locate a server that is hosting a system update, see setRepositoryConfig().

    When utilizing this API from several processes, precautions need to be taken
    to ensure that the processes' view of the system's state is up to date, see
    refreshMetadata(). A typical example would be a daemon that periodically
    checks a remote server (with fetchRemoteMetadata()) for system updates, and
    then uses IPC (such as a push notification) to let the system's main GUI know
    when a new version is available.

    Any IPC mechanism of choice can be used to ensure that system's state is being
    modified by only a single process at a time. Methods that modify the system's
    state are marked as such.

//! [client-description]
*/

/*!
    \qmlsignal OtaClient::fetchRemoteMetadataFinished(bool success)

    A notifier signal for fetchRemoteMetadata(). The \a success argument indicates
    whether the operation was successful.
*/

/*!
    \fn void QOtaClient::fetchRemoteMetadataFinished(bool success)

    A notifier signal for fetchRemoteMetadata(). The \a success argument indicates
    whether the operation was successful.
*/

/*!
    \qmlsignal OtaClient::updateFinished(bool success)

    A notifier signal for update(). The \a success argument indicates whether
    the operation was successful.
*/

/*!
    \fn void QOtaClient::updateFinished(bool success)

    A notifier signal for update(). The \a success argument indicates whether
    the operation was successful.
*/

/*!
    \qmlsignal OtaClient::rollbackFinished(bool success)

    A notifier signal for rollback(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOtaClient::rollbackFinished(bool success)

    A notifier signal for rollback(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlsignal OtaClient::updateOfflineFinished(bool success)

    A notifier signal for updateOffline(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOtaClient::updateOfflineFinished(bool success)

    A notifier signal for updateOffline(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlsignal OtaClient::updateRemoteMetadataOfflineFinished(bool success)

    A notifier signal for updateRemoteMetadataOffline(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \fn void QOtaClient::updateRemoteMetadataOfflineFinished(bool success)

    A notifier signal for updateRemoteMetadataOffline(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlsignal OtaClient::remoteMetadataChanged()

     This signal is emitted when remoteMetadata changes.
    \include qotaclient.cpp remotemetadatachanged-description
*/

/*!
    \fn void QOtaClient::remoteMetadataChanged()
    This signal is emitted when remoteMetadata() changes.
//! [remotemetadatachanged-description]
    This may happen when calling fetchRemoteMetadata() or updateRemoteMetadataOffline().
    This signal may also be emitted when calling updateOffline(), if updateRemoteMetadataOffline()
    haven't been called for the same package as used in updateOffline().
//! [remotemetadatachanged-description]
*/

/*!
    \qmlsignal OtaClient::rollbackMetadataChanged()

    This signal is emitted when rollbackMetadata changes. This metadata changes
    after system updates or rollbacks.
*/

/*!
    \fn void QOtaClient::rollbackMetadataChanged()

    This signal is emitted when rollbackMetadata() changes. This metadata changes
    after system updates or rollbacks.
*/

/*!
    \qmlsignal OtaClient::defaultMetadataChanged()

    This signal is emitted when defaultMetadata changes. This metadata changes
    after system updates or rollbacks.
*/

/*!
    \fn void QOtaClient::defaultMetadataChanged()

    This signal is emitted when defaultMetadata() changes. This metadata changes
    after system updates or rollbacks.
*/

/*!
    \qmlsignal OtaClient::updateAvailableChanged(bool available)

    This signal is emitted when the value of updateAvailable changes. The
    \a available argument holds whether a system update is available for
    the default system.
*/

/*!
    \fn void QOtaClient::updateAvailableChanged(bool available)

    This signal is emitted when the value of updateAvailable changes. The
    \a available argument holds whether a system update is available for
    the default system.
*/

/*!
    \qmlsignal OtaClient::rollbackAvailableChanged()

    This signal is emitted when the value of rollbackAvailable changes. The rollback
    system becomes available when a device has performed at least one system update.
*/

/*!
    \fn void QOtaClient::rollbackAvailableChanged()

    This signal is emitted when the value of rollbackAvailable changes. The rollback
    system becomes available when a device has performed at least one system update.
*/

/*!
    \qmlsignal OtaClient::restartRequiredChanged(bool required)

    This signal is emitted when the value of restartRequired changes. The
    \a required argument holds whether a reboot is required.
*/

/*!
    \fn void QOtaClient::restartRequiredChanged(bool required)

    This signal is emitted when the value of restartRequired changes. The
    \a required argument holds whether a reboot is required.
*/

/*!
    \qmlsignal OtaClient::statusChanged(string status);

    \include qotaclient.cpp statusstringchanged-description
*/

/*!
    \fn void QOtaClient::statusStringChanged(const QString &status)
//! [statusstringchanged-description]
    This signal is emitted when new status information is available. The
    \a status argument holds the status message.
//! [statusstringchanged-description]
*/

/*!
    \qmlsignal OtaClient::errorOccurred(string error);

    This signal is emitted when an error occurs. The \a error argument holds
    the error message.
*/

/*!
    \fn void QOtaClient::errorOccurred(const QString &error)

    This signal is emitted when an error occurs. The \a error argument holds
    the error message.
*/

/*!
    \qmlsignal OtaClient::repositoryConfigChanged(OtaRepositoryConfig config)

    This signal is emitted when the configuration file was successfully changed
    (\a config holds the new configuration) or removed (\a config holds the \c
    null value).
*/

/*!
    \fn void QOtaClient::repositoryConfigChanged(QOtaRepositoryConfig *config)

    This signal is emitted when the configuration file was successfully changed
    (\a config holds a pointer to the new configuration) or removed (\a config
    holds the \c nullptr value).
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
    \qmlmethod bool OtaClient::fetchRemoteMetadata()

    Fetches metadata from a remote server and updates remoteMetadata.

    \include qotaclient.cpp is-async-and-mutating
    \sa remoteMetadataChanged(), fetchRemoteMetadataFinished(), updateAvailable
*/

/*!
    Fetches metadata from a remote server and updates remoteMetadata().

//! [is-async-and-mutating]
    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \note This method mutates system's state.
//! [is-async-and-mutating]

    \sa remoteMetadataChanged(), fetchRemoteMetadataFinished(), updateAvailable()
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
    \qmlmethod bool OtaClient::update()
    \include qotaclient.cpp update-description

    \sa updateFinished(), fetchRemoteMetadata(), setRepositoryConfig(), restartRequired
*/

/*!
//! [update-description]
    Fetches an OTA update from a remote server and performs the system update.

    \include qotaclient.cpp is-async-and-mutating
//! [update-description]

    \sa updateFinished(), fetchRemoteMetadata(), setRepositoryConfig(), restartRequired()
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
    \qmlmethod bool OtaClient::rollback()
    \include qotaclient.cpp rollback-description

    \sa rollbackFinished(), restartRequired
*/

/*!
//! [rollback-description]
    Rollback to the previous snapshot of the system. The currently booted system
    becomes the new rollback system.

    \include qotaclient.cpp is-async-and-mutating
//! [rollback-description]

    \sa rollbackFinished(), restartRequired()
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
    \qmlmethod bool OtaClient::updateOffline(string packagePath)
    \include qotaclient.cpp update-offline
*/

/*!
//! [update-offline]
    Uses the provided self-contained update package to update the system.
    This method is an offline counterpart for update(). The \a packagePath
    argument holds a path to the update package.

    \include qotaclient.cpp is-async-and-mutating

    \sa updateOfflineFinished(), updateRemoteMetadataOffline()
//! [update-offline]
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
    \qmlmethod bool OtaClient::updateRemoteMetadataOffline(string packagePath)
    Uses the provided self-contained update package to update remoteMetadata.
    \include qotaclient.cpp update-remote-offline
*/

/*!
    Uses the provided self-contained update package to update remoteMetadata().
//! [update-remote-offline]
    This method is an offline counterpart for fetchRemoteMetadata(). The \a packagePath
    argument holds a path to the update package.

    \include qotaclient.cpp is-async-and-mutating
    \sa remoteMetadataChanged(), updateRemoteMetadataOfflineFinished(), updateOffline()
//! [update-remote-offline]
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
    \qmlmethod bool OtaClient::refreshMetadata()
    \include qotaclient.cpp refresh-metadata
*/

/*!
//! [refresh-metadata]
    In a multi-process scenario, refreshMetadata() updates the instances' view of the system.
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
    \qmlmethod bool OtaClient::removeRepositoryConfig()
    \include qotaclient.cpp remove-repository-config
*/

/*!
//! [remove-repository-config]
    Remove a configuration file for the repository.

    The repository configuration is stored on a file system in
    \c {/etc/ostree/remotes.d/}\unicode {0x002A}\c{.conf}

    If the configuration file does not exist, this function returns \c true.
    If the configuration file exists, this function returns \c true if the file
    is removed successfully; otherwise returns \c false.

    \sa setRepositoryConfig(), repositoryConfigChanged()
//! [remove-repository-config]
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
    \qmlmethod bool OtaClient::isRepositoryConfigSet(OtaRepositoryConfig config)
    \include qotaclient.cpp is-repository-config-set
*/

/*!
//! [is-repository-config-set]
    Returns \c true if the configuration \a config is already set; otherwise returns \c false.
    Configurations are compared by comparing all properties.
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
    \qmlmethod bool OtaClient::setRepositoryConfig(OtaRepositoryConfig config)
    \include qotaclient.cpp set-repository-config

    The \a config argument is documented in OtaRepositoryConfig.

    \sa isRepositoryConfigSet(), removeRepositoryConfig(), repositoryConfigChanged
*/

/*!
//! [set-repository-config]
    Set the configuration for the repository. The repository configuration
    is stored on a file system in \c {/etc/ostree/remotes.d/}\unicode {0x002A}\c{.conf}.
    If a configuration already exists, it needs to be removed before a new configuration
    can be set.

    Returns \c true if the configuration file is set successfully; otherwise returns \c false.
//! [set-repository-config]

    The \a config argument is documented in QOtaRepositoryConfig.

    \sa isRepositoryConfigSet(), removeRepositoryConfig(), repositoryConfigChanged
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
    \qmlmethod OtaRepositoryConfig OtaClient::repositoryConfig()

    Returns a configuration object for the repository or \c null if the
    configuration file does not exist or could not be read.

    \sa setRepositoryConfig(), removeRepositoryConfig()
*/

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
    \qmlproperty bool OtaClient::otaEnabled
    \readonly

    Holds whether a device supports OTA updates.
*/

/*!
    \property QOtaClient::otaEnabled
    Holds whether a device supports OTA updates.
*/
bool QOtaClient::otaEnabled() const
{
    Q_D(const QOtaClient);
    return d->m_otaEnabled;
}

/*!
    \qmlproperty string OtaClient::error
    \readonly

    Holds the last error occurred.
    \sa errorOccurred()
*/

/*!
    \property QOtaClient::error

    Holds the last error occurred.
    \sa errorOccurred()
*/
QString QOtaClient::errorString() const
{
    Q_D(const QOtaClient);
    return d->m_error;
}

/*!
    \qmlproperty string OtaClient::status
    \readonly

    Holds the last status message. Lengthy asynchronous operations use this property
    to notify of status changes.

    \sa statusChanged()
*/

/*!
    \property QOtaClient::status

    Holds the last status message. Lengthy asynchronous operations use this property
    to notify of status changes.

    \sa statusStringChanged()
*/
QString QOtaClient::statusString() const
{
    Q_D(const QOtaClient);
    return d->m_status;
}

/*!
    \qmlproperty bool OtaClient::updateAvailable
    \readonly

    \include qotaclient.cpp update-available-description
*/

/*!
    \property QOtaClient::updateAvailable

//! [update-available-description]
    Holds whether a system update is available for the default system. An update is
    available when the default system differ from the remote system.

    \sa fetchRemoteMetadata(), updateAvailableChanged(), update()
//! [update-available-description]
*/
bool QOtaClient::updateAvailable() const
{
    Q_D(const QOtaClient);
    if (d->m_updateAvailable)
        Q_ASSERT(!d->m_remoteRev.isEmpty());
    return d->m_updateAvailable;
}

/*!
    \qmlproperty bool OtaClient::rollbackAvailable
    \readonly

    \include qotaclient.cpp rollback-available-description
*/

/*!
    \property QOtaClient::rollbackAvailable

//! [rollback-available-description]
    Holds whether the rollback system is available.

    \sa rollbackAvailableChanged()
//! [rollback-available-description]
*/
bool QOtaClient::rollbackAvailable() const
{
    Q_D(const QOtaClient);
    return d->m_rollbackAvailable;
}

/*!
    \qmlproperty bool OtaClient::restartRequired
    \readonly

    \include qotaclient.cpp restart-required-description
*/

/*!
    \property QOtaClient::restartRequired

//! [restart-required-description]
    Holds whether a reboot is required. Reboot is required when the default system
    differ from the booted system.

    \sa restartRequiredChanged()
//! [restart-required-description]
*/
bool QOtaClient::restartRequired() const
{
    Q_D(const QOtaClient);
    return d->m_restartRequired;
}

/*!
    \qmlproperty string OtaClient::bootedRevision
    \readonly

    Holds the booted system's revision
//! [what-is-revision]
    (a checksum in the OSTree repository representing a snapshot of the system).
//! [what-is-revision]

    \sa bootedMetadata
*/

/*!
    \property QOtaClient::bootedRevision

    Holds the booted system's revision
    \include qotaclient.cpp what-is-revision

    \sa bootedMetadata()
*/

QString QOtaClient::bootedRevision() const
{
    return d_func()->m_bootedRev;
}

/*!
    \qmlproperty string OtaClient::bootedMetadata
    \readonly

    Holds JSON-formatted metadata for the booted system.
*/

/*!
    \property QOtaClient::bootedMetadata

    Holds JSON-formatted metadata for the snapshot of the booted system.
*/
QString QOtaClient::bootedMetadata() const
{
    return d_func()->m_bootedMetadata;
}

/*!
    \qmlproperty string OtaClient::remoteRevision
    \readonly

    Holds the system's revision on a server
    \include qotaclient.cpp what-is-revision

    \sa remoteMetadata
*/

/*!
    \property QOtaClient::remoteRevision

    Holds the system's revision on a server
    \include qotaclient.cpp what-is-revision

    \sa remoteMetadata()
*/
QString QOtaClient::remoteRevision() const
{
    return d_func()->m_remoteRev;
}

/*!
    \qmlproperty string OtaClient::remoteMetadata
    \readonly
    \include qotaclient.cpp remote-metadata
*/

/*!
    \property QOtaClient::remoteMetadata
//! [remote-metadata]
    Holds JSON-formatted metadata for the latest snapshot of the system on a server.

    \sa fetchRemoteMetadata(), remoteMetadataChanged()
//! [remote-metadata]
*/
QString QOtaClient::remoteMetadata() const
{
    return d_func()->m_remoteMetadata;
}

/*!
    \qmlproperty string OtaClient::rollbackRevision
    \readonly

    Holds the rollback system's revision
    \include qotaclient.cpp what-is-revision

    \sa rollbackMetadata
*/

/*!
    \property QOtaClient::rollbackRevision

    Holds the rollback system's revision
    \include qotaclient.cpp what-is-revision

    \sa rollbackMetadata()
*/
QString QOtaClient::rollbackRevision() const
{
    return d_func()->m_rollbackRev;
}

/*!
    \qmlproperty string OtaClient::rollbackMetadata
    \readonly
    \include qotaclient.cpp rollback-metadata
*/

/*!
    \property QOtaClient::rollbackMetadata
//! [rollback-metadata]
    Holds JSON-formatted metadata for the snapshot of the rollback system.

    \sa rollback(), rollbackMetadataChanged()
//! [rollback-metadata]
*/
QString QOtaClient::rollbackMetadata() const
{
    return d_func()->m_rollbackMetadata;
}

/*!
    \qmlproperty string OtaClient::defaultRevision
    \readonly

    Holds the default system's revision
    \include qotaclient.cpp what-is-revision

    \sa defaultMetadata
*/

/*!
    \property QOtaClient::defaultRevision

    Holds the default system's revision
    \include qotaclient.cpp what-is-revision

    \sa defaultMetadata()
*/
QString QOtaClient::defaultRevision() const
{
    return d_func()->m_defaultRev;
}

/*!
    \qmlproperty string OtaClient::defaultMetadata
    \readonly

    \include qotaclient.cpp default-metadata
*/

/*!
    \property QOtaClient::defaultMetadata
//! [default-metadata]
    Holds JSON-formatted metadata for the snapshot of the default system.
    A default system represents what the host will boot into the next time system is rebooted.

    \sa defaultMetadataChanged()
//! [default-metadata]
*/
QString QOtaClient::defaultMetadata() const
{
    return d_func()->m_defaultMetadata;
}

QT_END_NAMESPACE
