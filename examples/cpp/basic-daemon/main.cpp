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
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtOtaUpdate/QtOtaUpdate>

Q_LOGGING_CATEGORY(daemon, "ota.daemon.demo", QtDebugMsg)

class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    UpdateChecker(const QString &guiUpdater) : m_guiUpdaterPath(guiUpdater)
    {
        connect(&m_device, &QOtaClient::fetchRemoteInfoFinished, this, &UpdateChecker::fetchFinished);
        connect(&m_device, &QOtaClient::statusStringChanged, this, &UpdateChecker::log);
        connect(&m_device, &QOtaClient::errorOccurred, this, &UpdateChecker::logError);
        connect(&m_fetchTimer, &QTimer::timeout, this, &UpdateChecker::startFetch);

        m_repoConfig.setUrl(QStringLiteral("http://www.b2qtupdate.com/ostree-repo"));
        if (!m_device.isRepositoryConfigSet(&m_repoConfig))
            m_device.setRepositoryConfig(&m_repoConfig);

        m_fetchTimer.setSingleShot(true);
        m_fetchTimer.start();
    }

    void log(const QString &message) const { qCInfo(daemon) << message; }
    void logError(const QString &error) const {
        qCInfo(daemon) << QString(error).prepend(QStringLiteral("error: "));
    }

    void startFetch()
    {
        log(QStringLiteral("verifying remote server for system updates..."));
        m_device.fetchRemoteInfo();
    }

    void fetchFinished(bool success)
    {
        if (success && m_device.updateAvailable()) {
            log(QStringLiteral("update available"));
            // Any inter-process communication mechanism could be used here. In this demo we
            // simply launch a GUI that can be used to execute the update commands (such as examples/qml/basic/).
            // A more sophisticated approach would be to use IPC (such as a push notification) to let the
            // already running UI know that there is an system update available. Then this UI can open
            // OTA control view or call OtaClient::refreshInfo() if it is already at the OTA control view.
            QString cmd = QString(m_guiUpdaterPath).prepend(QStringLiteral("/usr/bin/appcontroller "));
            log(QString(cmd).prepend(QStringLiteral("starting GUI: ")));
            bool ok = QProcess::startDetached(cmd);
            if (!ok)
                logError(QString(cmd).prepend(QStringLiteral("failed to start updater GUI: ")));
            // Here we assume that the system restarts the daemon on the next reboot. Alternatively
            // GUI could use IPC to give further instructions to the daemon (based on users actions).
            qApp->quit();
        } else {
            log(QStringLiteral("no updates"));
            // Check again 2 seconds later.
            m_fetchTimer.start(2000);
        }
    }

private:
    QOtaClient m_device;
    QOtaRepositoryConfig m_repoConfig;
    QTimer m_fetchTimer;
    QString m_guiUpdaterPath;
};

#include "main.moc"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.setApplicationDescription(QStringLiteral("Qt OTA Update daemon demo."));
    QCommandLineOption verboseOption(QStringLiteral("v"),
                                     QStringLiteral("Print verbose debug messages from the Qt OTA Update library."));
    parser.addOption(verboseOption);
    QCommandLineOption guiUpdaterOption(QStringLiteral("gui-path"),
                QStringLiteral("A path to the GUI updater to start when a new system update is detected."),
                QStringLiteral("Path"));
    parser.addOption(guiUpdaterOption);

    parser.process(app);

    if (parser.isSet(verboseOption))
        QLoggingCategory::setFilterRules(QStringLiteral("b2qt.ota.debug=true"));
    QString guiAppPath = parser.value(guiUpdaterOption);
    if (guiAppPath.isEmpty()) {
        qWarning() << "--gui-path is required.";
        return EXIT_FAILURE;
    }

    UpdateChecker checker(guiAppPath);

    return app.exec();
}
