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
        if (!OtaClient.otaEnabled) {
            log("OTA Update functionality is not enabled on this device")
            return false;
        }
        if (!OtaClient.initialized) {
            log("Initialization is not ready")
            return false;
        }
        return true;
    }

    function configureRepository(config, silent) {
        var currentConfig = OtaClient.repositoryConfig()
        if (currentConfig && OtaClient.repositoryConfigsEqual(currentConfig, config)) {
            if (!silent)
                log("The configuration is already set")
            return false;
        } else {
            if (!OtaClient.removeRepositoryConfig()) {
                logError("Failed to remove repository configuration")
                return false;
            }
        }
        if (!OtaClient.setRepositoryConfig(config)) {
            logError("Failed to update repository configuration")
            return false;
        }
        if (!silent)
            log("Successfully updated repository configuration")
        return true;
    }

    function updateConfigView(config) {
        repoConfigLabel.text = "<b>URL:</b> " + (config ? config.url : "not set")
        repoConfigLabel.text += "<br><b>GPG Verify:</b> " + (config ? config.gpgVerify : "not set")
        repoConfigLabel.text += "<br><b>TLS Client Cert:</b> " + (config ? config.tlsClientCertPath : "not set")
        repoConfigLabel.text += "<br><b>TLS Client Key:</b> " + (config ? config.tlsClientKeyPath : "not set")
        repoConfigLabel.text += "<br><b>TLS Permissive:</b> " + (config ? config.tlsPermissive : "not set")
        repoConfigLabel.text += "<br><b>TLS CA:</b> " + (config ? config.tlsCaPath : "not set")
    }

    function updateMetadataLabel(label, metadata, rev) {
        // QByteArray to JS string.
        var metadataString = metadata.toString()
        if (metadataString.length === 0) {
            label.text = "<b>No metadata available</b>"
            return
        }
        var metadataObj = JSON.parse(metadataString)
        label.text = ""
        for (var property in metadataObj)
            label.text += "<b>" + property.charAt(0).toUpperCase()
                        + property.slice(1) + ": </b>" + metadataObj[property] + "<br>"
        label.text += "<b>Revision: </b>" + rev
    }
    function updateBootedMetadataLabel() {
        updateMetadataLabel(bootedMetadataLabel, OtaClient.bootedInfo, OtaClient.bootedRevision)
    }
    function updateRemoteMetadataLabel() {
        updateMetadataLabel(remoteMetadataLabel, OtaClient.remoteInfo, OtaClient.remoteRevision)
    }
    function updateRollbackMetadataLabel() {
        updateMetadataLabel(rollbackMetadataLabel, OtaClient.rollbackInfo, OtaClient.rollbackRevision)
    }

    Flickable {
        anchors.fill: parent
        contentHeight: topLayout.implicitHeight + topLayout.anchors.bottomMargin * 2
        flickableDirection: Flickable.VerticalFlick
        ScrollBar.vertical: ScrollBar { }

        ColumnLayout {
            id: topLayout
            anchors.fill: parent
            anchors.margins: 10

            Label { text: "BOOTED"; Layout.bottomMargin: 14; font.underline: true; }
            Label { id: bootedMetadataLabel; lineHeight : 1.3 }

            Label { text: "REMOTE"; Layout.bottomMargin: 14; Layout.topMargin: 14; font.underline: true }
            Label { id: remoteMetadataLabel; lineHeight : 1.3 }

            Label { text: "ROLLBACK"; Layout.bottomMargin: 14; Layout.topMargin: 14; font.underline: true }
            Label { id: rollbackMetadataLabel; lineHeight : 1.3 }

            Label { text: "REPOSITORY CONFIGURATION"; Layout.bottomMargin: 14; Layout.topMargin: 14; font.underline: true }
            Label { id: repoConfigLabel; lineHeight : 1.3 }

            RowLayout {
                Layout.topMargin: 20
                Layout.bottomMargin: 10

                Button {
                    text: "Use basic config"
                    onClicked: {
                        if (!otaReady())
                            return;
                        configureRepository(basicConfig, false)
                    }
                }
                Button {
                    text: "Use secure config"
                    onClicked: {
                        if (!otaReady())
                            return;
                        configureRepository(secureConfig, false)

                    }
                }
                Button {
                    text: "Fetch OTA info"
                    onClicked: {
                        if (!otaReady())
                            return;
                        log("Fetcing OTA info...")
                        OtaClient.fetchRemoteInfo()
                    }
                }
                Button {
                    visible: OtaClient.rollbackAvailable
                    text: "Rollback"
                    onClicked: {
                        if (!otaReady())
                            return;
                        log("Roolback...")
                        OtaClient.rollback()
                    }
                }
                Button {
                    visible: OtaClient.updateAvailable
                    text: "Update"
                    onClicked: {
                        if (!otaReady())
                            return;
                        log("Updating...")
                        OtaClient.update()
                    }
                }
                Button {
                    visible: OtaClient.restartRequired
                    text: "Restart"
                    onClicked: {
                        if (!otaReady())
                            return;
                        log("Restarting (unimplemented) ...")
                    }
                }
            }

            Frame {
                Layout.preferredHeight: bootedMetadataLabel.font.pixelSize * 12
                Layout.preferredWidth: {
                    Screen.width < 800 ? Screen.width - topLayout.anchors.leftMargin * 2
                                       : Screen.width * 0.5 > 800 ? 800 : Screen.width * 0.5
                }
                padding: 6
                ListView {
                    id: logView
                    anchors.fill: parent
                    clip: true
                    model: ListModel { id: logRecords }
                    delegate: Label {
                        text: record
                        color: textcolor
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    OtaRepositoryConfig {
        id: basicConfig
        url: "http://www.b2qtupdate.com/ostree-repo"
    }

    OtaRepositoryConfig {
        id: secureConfig
        gpgVerify: true
        url: "https://www.b2qtupdate.com/ostree-repo"
        tlsClientCertPath: "/usr/share/ostree/certs/clientcert.pem"
        tlsClientKeyPath: "/usr/share/ostree/certs/clientkey.pem"
        tlsPermissive: false
        tlsCaPath: "/usr/share/ostree/certs/servercert.pem"
    }

    Connections {
        target: OtaClient
        onErrorChanged: logError(error)
        onStatusChanged: log(status)
        onInitializationFinished: {
            logWithCondition("Initialization", OtaClient.initialized)
            if (!configureRepository(basicConfig, true))
                updateConfigView(OtaClient.repositoryConfig())
            updateBootedMetadataLabel()
            updateRemoteMetadataLabel()
            updateRollbackMetadataLabel()
        }
        onFetchRemoteInfoFinished: {
            logWithCondition("Fetching info from a remote server", success)
            if (success)
                log("Update available: " + OtaClient.updateAvailable)
        }
        onRollbackFinished: logWithCondition("Rollback", success)
        onUpdateFinished: logWithCondition("Update", success)
        onRepositoryConfigChanged: updateConfigView(config)
        onRemoteInfoChanged: updateRemoteMetadataLabel()
        onRollbackInfoChanged: updateRollbackMetadataLabel()
    }

    Component.onCompleted: {
        if (!OtaClient.otaEnabled)
            log("OTA Update functionality is not enabled on this device")
        updateConfigView(0)
    }
}
