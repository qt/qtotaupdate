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
import QtOTAUpdate 1.0

Window {
    visible:true

    function log(message) { logRecords.append({ "record" : (logRecords.count + 1) + " " + message }) }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.topMargin: 10

        Label { text: "CLIENT:"; Layout.bottomMargin: 14 }
        Label { text: "Version: " + OTAClient.clientVersion }
        Label { text: "Description: " + OTAClient.clientDescription }
        Label { text: "Revision: " + OTAClient.clientRevision }

        Label { text: "SERVER:"; Layout.bottomMargin: 14; Layout.topMargin: 14 }
        Label { text: "Version: " + OTAClient.serverVersion }
        Label { text: "Description: " + OTAClient.serverDescription }
        Label { text: "Revision: " + OTAClient.serverRevision }

        Label { text: "ROLLBACK:"; Layout.bottomMargin: 14; Layout.topMargin: 14 }
        Label { text: "Version: " + OTAClient.rollbackVersion }
        Label { text: "Description: " + OTAClient.rollbackDescription }
        Label { text: "Revision: " + OTAClient.rollbackRevision }

        RowLayout {
            Layout.topMargin: 20
            Layout.bottomMargin: 10
            Button {
                id: fetchButton
                text: "Fetch OTA info"
                onClicked: {
                    log("Fetcing OTA info...")
                    OTAClient.fetchServerInfo()
                }
            }
            Button {
                text: "Rollback"
                onClicked: {
                    log("Roolback...")
                    OTAClient.rollback()
                }
            }
            Button {
                visible: OTAClient.updateAvailable
                text: "Update"
                onClicked: {
                    log("updating...")
                    OTAClient.update()
                }
            }
            Button {
                visible: OTAClient.restartRequired
                text: "Restart"
                onClicked: {
                    log("restarting...")
                }
            }
        }

        Frame {
            Layout.preferredHeight: 200
            Layout.preferredWidth: Screen.width * 0.5
            padding: 2
            ListView {
                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: ListModel { id: logRecords }
                delegate: Label { text: record }
                onCountChanged: positionViewAtEnd()
            }
        }

        Item { Layout.fillHeight: true }
    }

    Connections {
        target: OTAClient
        onErrorChanged: log(OTAClient.error)
        onInitializationFinished: log("Initialization " + (OTAClient.initialized ? "finished" : "failed"))
        onFetchServerInfoFinished: {
            log("FetchServerInfo " + (success ? "finished" : "failed"))
            if (success)
                log("updateAvailable: " + OTAClient.updateAvailable)
        }
        onRollbackFinished: log("Rollback " + (success ? "finished" : "failed"))
        onUpdateFinished: log("Update " + (success ? "finished" : "failed"))
    }
}
