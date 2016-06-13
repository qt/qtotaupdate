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
#include <QtQml>

QT_BEGIN_NAMESPACE

/*!
    \inqmlmodule QtOTAUpdate
    \qmltype OTAClient
    \instantiates QOTAClient
    \brief Main interface to the OTA functionality.

    OTAClient provides an API to execute OTA tasks. Offline operations include
    querying the booted and rollback system version details and atomically
    performing the rollbacks. Online operations include fetching a new system
    version from a remote server and atomically performing system updates.

    Using this API is safe and won't leave the system in an inconsistent state,
    even if the power fails half-way through.
*/

/*!
    \qmlproperty string OTAClient::clientVersion
    \readonly

    This is a convenience property that holds a string containing the booted system's version.
    \sa clientInfo
*/

/*!
    \qmlproperty string OTAClient::clientDescription
    \readonly

    This is a convenience property that holds a string containing the booted system's description.
    \sa clientInfo
*/

/*!
    \qmlproperty string OTAClient::clientRevision
    \readonly

    This property holds a string containing the booted system's revision (a checksum in the OSTree
    repository).
*/

/*!
    \qmlproperty string OTAClient::clientInfo
    \readonly

    This property holds a JSON-formatted string containing the booted system's OTA metadata.
    Metadata is bundled with each system's version.
*/

/*!
    \qmlproperty string OTAClient::serverVersion
    \readonly

    This is a convenience property that holds a string containing the system's version on a server.
    \sa serverInfo
*/

/*!
    \qmlproperty string OTAClient::serverDescription
    \readonly

    This is a convenience property that holds a string containing the system's description on a server.
    \sa serverInfo
*/

/*!
    \qmlproperty string OTAClient::serverRevision
    \readonly

    This property holds a string containing the system's revision on a server (a checksum in the
    OSTree repository).
*/

/*!
    \qmlproperty string OTAClient::serverInfo
    \readonly

    This property holds a JSON-formatted string containing OTA metadata for the system on a server.
    Metadata is bundled with each system's version.
*/

/*!
    \qmlproperty string OTAClient::rollbackVersion
    \readonly

    This is a convenience property that holds a string containing the rollback system's version.
    \sa rollbackInfo
*/

/*!
    \qmlproperty string OTAClient::rollbackDescription
    \readonly

    This is a convenience property that holds a string containing the rollback system's description.
    \sa rollbackInfo
*/

/*!
    \qmlproperty string OTAClient::rollbackRevision
    \readonly

    This property holds a string containing the rollback system's revision (a checksum in the OSTree
    repository).
*/

/*!
    \qmlproperty string OTAClient::rollbackInfo
    \readonly

    This property holds a JSON-formatted string containing the rollback system's OTA metadata.
    Metadata is bundled with each system's version.

    \sa rollback()
*/

/*!
    \qmlsignal OTAClient::rollbackInfoChanged()

    This signal is emitted when rollback info changes. Rollback info changes
    when calling rollback().
*/

/*!
    \qmlmethod bool OTAClient::fetchServerInfo()

    Fetch OTA metadata from a server and update the local cache. This
    metadata contains information on what system version is available on a
    server. The cache is persistent as it is stored on the disk.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \sa fetchServerInfoFinished(), updateAvailable, serverInfo
*/

/*!
    \qmlsignal OTAClient::fetchServerInfoFinished(bool success)

    This is a notifier signal for fetchServerInfo(). The \a success argument
    indicates whether the operation was successful.
*/

/*!
    \qmlsignal OTAClient::serverInfoChanged()

    Server info can change when calling fetchServerInfo(). If OTA metadata on
    the server is different from the local cache, the local cache is updated and
    this signal is emitted.
*/

/*!
    \qmlmethod bool OTAClient::update()

    Fetch an OTA update from a server and perform the system update.

    This method is asynchronous and returns immediately. The return value
    holds whether the operation was started successfully.

    \sa updateFinished(), fetchServerInfo, restartRequired
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

    This property holds whether the object has completed the initialization.

    When an object of this class is created, it asynchronously (from a non-GUI thread)
    pre-populates the internal state and sets this property accordingly, and signals
    initializationFinished().

    Initialization is fast if there are no other processes locking access to the OSTree
    repository on a device. This could happen if there is some other process currently
    writing to the OSTree repository, for example, a daemon calling fetchServerInfo().

    \sa initializationFinished()
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

    Holds a bool indicating the availability of a system update. This information
    is cached - to update the local cache, call fetchServerInfo().

    \sa update()
*/

/*!
    \qmlsignal OTAClient::updateAvailableChanged(bool available)

    This signal is emitted when the value of updateAvailable changes. The
    \a available argument holds whether a system update is available for
    default system.
*/

/*!
    \qmlproperty bool OTAClient::restartRequired
    \readonly

    Holds a bool indicating whether reboot is required. Reboot is required
    after update() and rollback() to boot into the new default system.
*/

/*!
    \qmlsignal OTAClient::restartRequiredChanged(bool required)

    This signal is emitted when the value of restartRequired changes. The
    \a required argument holds whether reboot is required.
*/

/*!
    \qmlsignal OTAClient::initializationFinished()

    This signal is emitted when the object has finished initialization. Only after
    this signal has arrived, the object is ready for use.
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
    }
};

QT_END_NAMESPACE

#include "pluginmain.moc"
