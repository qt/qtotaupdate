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
import QtQuick 2.7
import QtQuick.Window 2.2
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.3
import QtOtaUpdate 1.0

Window {
    visible:true

    function log(message) { logFormatted(message, "black") }
    function logError(message) { logFormatted(message, "red") }
    function logWithCondition(message, condition) {
       var color = condition ? "black" : "red"
       var suffix = condition ? " finished" : " failed"
       logFormatted(message + suffix, color)
    }
    function logFormatted(message, color) {
        logRecords.append({
            "record" : "<font color=\"black\">" + (logRecords.count + 1) + "</font> " + message,
            "textcolor" : color
        })
        logView.positionViewAtEnd()
    }

    function otaReady() {
        if (!OTAClient.otaEnabled) {
            log("OTA Update functionality is not enabled on this device")
            return false;
        }
        if (!OTAClient.initialized) {
            log("Initialization is not ready")
            return false;
        }
        return true;
    }

    function configureRemote(config) {
        if (!OTAClient.removeRepositoryConfig()) {
            logError("Failed to remove repository configuration")
            return;
        }
        if (!OTAClient.setRepositoryConfig(config)) {
            logError("Failed to update repository configuration")
            return;
        }
    }

    function updateConfigView(config) {
        repoUrl.text = "<b>URL:</b> " + (config ? config.url : "not set")
        repoGpgVerify.text = "<b>GPG Verify:</b> " + (config ? config.gpgVerify : "not set")
        repoClientCert.text = "<b>TLS Client Cert:</b> " + (config ? config.tlsClientCertPath : "not set")
        repoClientKey.text = "<b>TLS Client Key:</b> " + (config ? config.tlsClientKeyPath : "not set")
        repoPermissive.text = "<b>TLS Permissive:</b> " + (config ? config.tlsPermissive : "not set")
        repoCa.text = "<b>TLS CA:</b> " + (config ? config.tlsCaPath : "not set")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.topMargin: 10

        Label { text: "BOOTED"; Layout.bottomMargin: 14; font.underline: true; }
        Label { text: "<b>Version:</b> " + OTAClient.bootedVersion }
        Label { text: "<b>Description:</b> " + OTAClient.bootedDescription }
        Label { text: "<b>Revision:</b> " + OTAClient.bootedRevision }

        Label { text: "REMOTE"; Layout.bottomMargin: 14; Layout.topMargin: 14; font.underline: true }
        Label { text: "<b>Version:</b> " + OTAClient.remoteVersion }
        Label { text: "<b>Description:</b> " + OTAClient.remoteDescription }
        Label { text: "<b>Revision:</b> " + OTAClient.remoteRevision; Layout.bottomMargin: 10; }

        Label { id: repoUrl; }
        Label { id: repoGpgVerify; }
        Label { id: repoClientCert; }
        Label { id: repoClientKey; }
        Label { id: repoPermissive; }
        Label { id: repoCa; }

        Label { text: "ROLLBACK"; Layout.bottomMargin: 14; Layout.topMargin: 14; font.underline: true }
        Label { text: "<b>Version:</b> " + OTAClient.rollbackVersion }
        Label { text: "<b>Description:</b> " + OTAClient.rollbackDescription }
        Label { text: "<b>Revision:</b> " + OTAClient.rollbackRevision }

        RowLayout {
            Layout.topMargin: 20
            Layout.bottomMargin: 10
            Button {
                id: fetchButton
                text: "Fetch OTA info"
                onClicked: {
                    if (!otaReady())
                        return;
                    log("Fetcing OTA info...")
                    OTAClient.fetchRemoteInfo()
                }
            }
            Button {
                visible: OTAClient.rollbackAvailable
                text: "Rollback"
                onClicked: {
                    if (!otaReady())
                        return;
                    log("Roolback...")
                    OTAClient.rollback()
                }
            }
            Button {
                visible: OTAClient.updateAvailable
                text: "Update"
                onClicked: {
                    if (!otaReady())
                        return;
                    log("Updating...")
                    OTAClient.update()
                }
            }
            Button {
                visible: OTAClient.restartRequired
                text: "Restart"
                onClicked: {
                    if (!otaReady())
                        return;
                    log("Restarting (unimplemented) ...")
                }
            }
        }

        Frame {
            Layout.preferredHeight: 200
            Layout.preferredWidth: Screen.width * 0.5
            padding: 2
            ListView {
                id: logView
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: ListModel { id: logRecords }
                delegate: Label { text: record ; color: textcolor }
            }
        }

        Item { Layout.fillHeight: true }
    }

    OtaRepositoryConfig {
        id: repoConfig
        gpgVerify: true
        url: "http://www.b2qtupdate.com/ostree-repo"
        tlsClientCertPath: "/usr/share/ostree/certs/clientcert.pem"
        tlsClientKeyPath: "/usr/share/ostree/certs/clientkey.pem"
        tlsPermissive: false
        tlsCaPath: "/usr/share/ostree/certs/servercert.pem"
    }

    Connections {
        target: OTAClient
        onErrorChanged: logError(error)
        onStatusChanged: log(status)
        onInitializationFinished: {
            logWithCondition("Initialization", OTAClient.initialized)
            configureRemote(repoConfig)
        }
        onFetchRemoteInfoFinished: {
            logWithCondition("Fetching info from a remote server", success)
            if (success)
                log("Update available: " + OTAClient.updateAvailable)
        }
        onRollbackFinished: logWithCondition("Rollback", success)
        onUpdateFinished: logWithCondition("Update", success)
        onRepositoryConfigChanged: updateConfigView(config)
    }

    Component.onCompleted: {
        if (!OTAClient.otaEnabled)
            log("OTA Update functionality is not enabled on this device")
        updateConfigView(0)
    }
}
