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
#include <QtOTAUpdate/QOTAClient>
#include <QtOTAUpdate/QOtaRepositoryConfig>
#include <QtQml>

QT_BEGIN_NAMESPACE

/*!
    \inqmlmodule QtOTAUpdate
    \qmltype OTAClient
    \instantiates QOTAClient
    \brief Main interface to the OTA functionality.

    OTAClient
    \include qotaclient.cpp client-description
*/

/*!
    \qmlproperty string OTAClient::bootedVersion
    \readonly

    This is a convenience property that holds a string containing the booted
    system's version.

    \sa bootedInfo
*/

/*!
    \qmlproperty string OTAClient::bootedDescription
    \readonly

    This is a convenience property that holds a string containing the booted
    system's description.

    \sa bootedInfo
*/

/*!
    \qmlproperty string OTAClient::bootedRevision
    \readonly

    This property holds a string containing the booted system's revision
    (a checksum in the OSTree repository).
*/

/*!
    \qmlproperty string OTAClient::bootedInfo
    \readonly

    This property holds a JSON-formatted string containing the booted system's
    OTA metadata. Metadata is bundled with each system's version.
*/

/*!
    \qmlproperty string OTAClient::remoteVersion
    \readonly

    This is a convenience property that holds a string containing the system's
    version on a server.

    \sa remoteInfo
*/

/*!
    \qmlproperty string OTAClient::remoteDescription
    \readonly

    This is a convenience property that holds a string containing the system's
    description on a server.

    \sa remoteInfo
*/

/*!
    \qmlproperty string OTAClient::remoteRevision
    \readonly

    This property holds a string containing the system's revision on a server
    (a checksum in the OSTree repository).
*/

/*!
    \qmlproperty string OTAClient::remoteInfo
    \readonly

    This property holds a JSON-formatted string containing OTA metadata for the
    system on a server. Metadata is bundled with each system's version.
*/

/*!
    \qmlproperty string OTAClient::rollbackVersion
    \readonly

    This is a convenience property that holds a string containing the rollback
    system's version.

    \sa rollbackInfo
*/

/*!
    \qmlproperty string OTAClient::rollbackDescription
    \readonly

    This is a convenience property that holds a string containing the rollback
    system's description.

    \sa rollbackInfo
*/

/*!
    \qmlproperty string OTAClient::rollbackRevision
    \readonly

    This property holds a string containing the rollback system's revision (a
    checksum in the OSTree repository).
*/

/*!
    \qmlproperty string OTAClient::rollbackInfo
    \readonly

    This property holds a JSON-formatted string containing the rollback
    system's OTA metadata. Metadata is bundled with each system's version.

    \sa rollback()
*/

/*!
    \qmlsignal OTAClient::rollbackInfoChanged()

    This signal is emitted when the rollback info changes. Rollback info
    changes when calling rollback().
*/

/*!
    \qmlmethod bool OTAClient::fetchRemoteInfo()

    \include qotaclient.cpp fetchremoteinfo-description

    \sa fetchRemoteInfoFinished(), updateAvailable, remoteInfo
*/

/*!
    \qmlsignal OTAClient::fetchRemoteInfoFinished(bool success)

    This is a notifier signal for fetchRemoteInfo(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlsignal OTAClient::remoteInfoChanged()

    \include qotaclient.cpp remoteinfochanged-description
*/

/*!
    \qmlmethod bool OTAClient::update()

    \include qotaclient.cpp update-description

    \sa updateFinished(), fetchRemoteInfo, restartRequired
*/

/*!
    \qmlsignal OTAClient::updateFinished(bool success)

    This is a notifier signal for update(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlmethod bool OTAClient::rollback()

    Rollback to the previous system version.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \sa rollbackFinished(), restartRequired
*/

/*!
    \qmlsignal OTAClient::rollbackFinished(bool success)

    This is a notifier signal for rollback(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlproperty bool OTAClient::otaEnabled
    \readonly

    This property holds whether a device supports OTA updates.
*/

/*!
    \qmlproperty bool  OTAClient::initialized
    \readonly

    \include qotaclient.cpp initialized-description

    \sa initializationFinished()
*/

/*!
    \qmlproperty string OTAClient::status
    \readonly

    This property holds a string containing the last status message. Only
    selected operations update this property.
*/

/*!
    \qmlsignal OTAClient::statusChanged(string status);

    \include qotaclient.cpp statusstringchanged-description
*/

/*!
    \qmlproperty string OTAClient::error
    \readonly

    This property holds a string containing the last error occurred.
*/

/*!
    \qmlsignal OTAClient::errorOccurred(string error);

    This signal is emitted when an error occurs. The \a error argument holds
    the error message.
*/

/*!
    \qmlproperty bool OTAClient::updateAvailable
    \readonly

    Holds a bool indicating the availability of a system update. This
    information is cached; to update the local cache, call fetchRemoteInfo().

    \sa update()
*/

/*!
    \qmlsignal OTAClient::updateAvailableChanged(bool available)

    This signal is emitted when the value of updateAvailable changes. The
    \a available argument holds whether a system update is available for
    the default system.
*/

/*!
    \qmlproperty bool OTAClient::rollbackAvailable
    \readonly

    Holds a bool indicating the availability of a rollback system.

    \sa rollbackAvailableChanged()
*/

/*!
    \qmlsignal OTAClient::rollbackAvailableChanged()

    This signal is emitted when the value of rollbackAvailable changes. A rollback
    system becomes available when a device has performed at least one system update.
*/

/*!
    \qmlproperty bool OTAClient::restartRequired
    \readonly

    Holds a bool indicating whether a reboot is required. Reboot is required
    after update() and rollback(), to boot into the new default system.
*/

/*!
    \qmlsignal OTAClient::restartRequiredChanged(bool required)

    This signal is emitted when the value of restartRequired changes. The
    \a required argument holds whether a reboot is required.
*/

/*!
    \qmlsignal OTAClient::initializationFinished()

    This signal is emitted when the object has finished initialization. The
    object is not ready for use until this signal is received.
*/

/*!
    \qmlmethod bool OTAClient::setRepositoryConfig(OtaRepositoryConfig config)

    \include qotaclient.cpp set-repository-config

    The \a config argument is documented in OtaRepositoryConfig.

    \sa repositoryConfigChanged
*/

/*!
    \qmlmethod OtaRepositoryConfig OTAClient::repositoryConfig()

    Returns a configuration object for the repository or \c null if the
    configuration file does not exist or could not be read.

    \sa setRepositoryConfig(), removeRepositoryConfig()
*/

/*!
    \qmlmethod bool OTAClient::removeRepositoryConfig()

    \include qotaclient.cpp remove-repository-config

    \sa repositoryConfigChanged
*/

/*!
    \qmlsignal OTAClient::repositoryConfigChanged(OtaRepositoryConfig config)

    This signal is emitted when the configuration file was successfully updated
    (\a repository holds the new configuration) or removed (\a repository
    holds the \c null value).
*/

/*!
    \inqmlmodule QtOTAUpdate
    \qmltype OtaRepositoryConfig
    \instantiates QOtaRepositoryConfig
    \brief Used to configure the OSTree repository.

    OtaRepositoryConfig
    \include qotarepositoryconfig.cpp repository-config-description
*/

/*!
    \qmlsignal OtaRepositoryConfig::urlChanged()

    This signal is emitted when the value of url changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::gpgVerifyChanged()

    This signal is emitted when the value of gpgVerify changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsClientCertPathChanged()

    This signal is emitted when the value of tlsClientCertPath changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsClientKeyPathChanged()

    This signal is emitted when the value of tlsClientKeyPath changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsPermissiveChanged()

    This signal is emitted when the value of tlsPermissive changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsCaPathChanged()

    This signal is emitted when the value of tlsCaPath changes.
*/

/*!
    \qmlproperty bool OtaRepositoryConfig::gpgVerify

    Holds a bool indicating whether or not OSTree will require commits
    to be signed by a known GPG key.

    Default is \c false.
*/

/*!
    \qmlproperty bool OtaRepositoryConfig::tlsPermissive

    Holds a bool indicating whether to check the server's TLS certificates
    against the system's certificate store.

    Default is \c false.
*/

/*!
    \qmlproperty string OtaRepositoryConfig::url

    This property holds a URL for accessing remote OSTree repository.
    The supported schemes at the moment are \c http and \c https.
*/

/*!
    \qmlproperty string OtaRepositoryConfig::tlsClientCertPath

    This property holds a path to a file for the client-side certificate,
    to present when making requests to the remote repository.
*/

/*!
    \qmlproperty string OtaRepositoryConfig::tlsClientKeyPath

    This property holds a path to a file containing the client-side certificate key,
    to present when making requests to the remote repository.
*/

/*!
    \qmlproperty string OtaRepositoryConfig::tlsCaPath

    This property holds a path to a file containing trusted anchors instead of
    the system's CA database.
*/

static QObject *otaClientSingleton(QQmlEngine *qmlEngine, QJSEngine *jsEngine)
{
    Q_UNUSED(qmlEngine);
    Q_UNUSED(jsEngine);
    return new QOTAClient;
}
class QOTAUpdatePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QQmlExtensionInterface/1.0")

public:
    virtual void registerTypes(const char *uri)
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtOTAUpdate"));

        qmlRegisterSingletonType<QOTAClient>(uri, 1, 0, "OTAClient", otaClientSingleton);
        qmlRegisterType<QOtaRepositoryConfig>(uri, 1, 0, "OtaRepositoryConfig");
    }
};

QT_END_NAMESPACE

#include "pluginmain.moc"
