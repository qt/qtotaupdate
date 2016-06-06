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
import QtQuick 2.2
import QtQuick.Window 2.2
import QtQuick.Controls 1.4
import QtQuick.Layouts 1.3
import QtOTAUpdate 1.0

Window {
    id: screen
    visible:true
    width: Screen.width
    height: Screen.height

    Rectangle {
        anchors.fill: parent
        anchors.margins: 8
        anchors.leftMargin: 22

        ColumnLayout {
            Text { text: "CLIENT:\n"; }
            Text { text: "Version: " + OTAClient.clientVersion }
            Text { text: "Description: " + OTAClient.clientDescription }
            Text { text: "Revision: " + OTAClient.clientRevision }

            Text { text: "\nSERVER:\n"; }
            Text { text: "Version: " + OTAClient.serverVersion }
            Text { text: "Description: " + OTAClient.serverDescription }
            Text { text: "Revision: " + OTAClient.serverRevision }

            Text { text: "\nROLLBACK:\n"; }
            Text { text: "Version: " + OTAClient.rollbackVersion }
            Text { text: "Description: " + OTAClient.rollbackDescription }
            Text { text: "Revision: " + OTAClient.rollbackRevision }

            RowLayout {
                Button {
                    id: fetchButton
                    text: "Fetch OTA info"
                    onClicked: {
                        print("fetcing OTA info...")
                        OTAClient.fetchServerInfo()
                    }
                }

                Button {
                    text: "Rollback"
                    onClicked: {
                        print("roolback...")
                        OTAClient.rollback()
                    }
                }

                Button {
                    visible: OTAClient.updateAvailable
                    text: "Update"
                    onClicked: {
                        print("updating...")
                        OTAClient.update()
                    }
                }

                Button {
                    visible: OTAClient.restartRequired
                    text: "Restart"
                    onClicked: {
                        print("restarting...")
                    }
                }
            }

            Rectangle {
                height: fetchButton.height * 4
                width: screen.width * 0.5
                border.width: 2
                border.color: "red"
                ListView {
                    anchors.fill: parent
                    anchors.margins: 4
                    clip: true
                    model: ListModel { id: logRecords }
                    delegate: Text { text: record }
                }
            }
        }
    }

    Connections {
        target: OTAClient
        onInitializationFinished: print("onInitializationFinished")
        onFetchServerInfoFinished: print("onFetchServerInfoFinished: " + success)
        onRollbackFinished: print("onRollbackFinished: " + success)
        onErrorChanged: logRecords.append({ "record" : OTAClient.error })
        onUpdateAvailableChanged: print("onUpdateAvailableChanged: " + available)
        onUpdateFinished: {
            print("onUpdateFinished: " + success)
            if (success) {
                // .. notify user that restart is required for the new version to go live
            }
        }
    }
}
