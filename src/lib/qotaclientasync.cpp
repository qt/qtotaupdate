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
#include "ostree-1/ostree.h"
#include "glib-2.0/glib.h"

#include "qotaclientasync_p.h"
#include "qotaclient_p.h"

#include <QtCore/QFile>
#include <QtCore/QTime>

QT_BEGIN_NAMESPACE

QOTAClientAsync::QOTAClientAsync() :
    m_sysroot(ostree_sysroot_new(0)),
    m_ostree(0)
{
    // async mapper
    connect(this, &QOTAClientAsync::initialize, this, &QOTAClientAsync::_initialize);
    connect(this, &QOTAClientAsync::fetchRemoteInfo, this, &QOTAClientAsync::_fetchRemoteInfo);
    connect(this, &QOTAClientAsync::update, this, &QOTAClientAsync::_update);
    connect(this, &QOTAClientAsync::rollback, this, &QOTAClientAsync::_rollback);
}

QOTAClientAsync::~QOTAClientAsync()
{
    if (m_ostree) {
        m_ostree->waitForFinished();
        delete m_ostree;
    }
    g_object_unref (m_sysroot);
}

static void parseErrorString(QString *error)
{
    error->remove(0, qstrlen("error: "));

    if (error->startsWith(QLatin1String("Remote")) && error->endsWith(QLatin1String("not found")))
        *error = QLatin1String("Remote configuration not found.");
}

QString QOTAClientAsync::ostree(const QString &command, bool *ok, bool updateStatus)
{
    qCDebug(qota) << command;
    if (!m_ostree) {
        m_ostree = new QProcess;
        m_ostree->setProcessChannelMode(QProcess::MergedChannels);
    }
    m_ostree->start(command);
    if (!m_ostree->waitForStarted()) {
        *ok = false;
        emit errorOccurred(QLatin1String("Failed to start: ") + command
                         + QLatin1String(" : ") + m_ostree->errorString());
        return QString();
    }

    QString out;
    bool finished = false;
    do {
        finished = m_ostree->waitForFinished(200);
        if (!finished && m_ostree->error() != QProcess::Timedout) {
            *ok = false;
            emit errorOccurred(QLatin1String("Process failed: ") + command +
                               QLatin1String(" : ") + m_ostree->errorString());
            return QString();
        }
        while (m_ostree->canReadLine()) {
            QByteArray bytesRead = m_ostree->readLine().trimmed();
            if (bytesRead.isEmpty())
                continue;

            QString line = QString::fromUtf8(bytesRead);
            qCDebug(qota) << line;
            if (line.startsWith(QStringLiteral("error:"))) {
                *ok = false;
                parseErrorString(&line);
                emit errorOccurred(line);
            } else {
                if (updateStatus)
                    emit statusStringChanged(line);
            }
            out.append(line);
        }
    } while (!finished);

    return out;
}

QJsonDocument QOTAClientAsync::info(QOTAClientPrivate::QueryTarget target, bool *ok, const QString &rev)
{
    QString jsonData;
    switch (target) {
    case QOTAClientPrivate::QueryTarget::Booted: {
        QFile metadata(QStringLiteral("/usr/etc/qt-ota.json"));
        if (metadata.open(QFile::ReadOnly))
            jsonData = QString::fromLatin1(metadata.readAll());
        break;
    }
    case QOTAClientPrivate::QueryTarget::Remote:
        jsonData = ostree(QString(QStringLiteral("ostree cat %1 /usr/etc/qt-ota.json")).arg(rev), ok);
        break;
    case QOTAClientPrivate::QueryTarget::Rollback:
        jsonData = ostree(QString(QStringLiteral("ostree cat %1 /usr/etc/qt-ota.json")).arg(rev), ok);
        break;
    default:
        Q_UNREACHABLE();
    }
    if (jsonData.isEmpty())
        return QJsonDocument();

    QJsonParseError parseError;
    QJsonDocument jsonInfo = QJsonDocument::fromJson(jsonData.toLatin1(), &parseError);
    if (jsonInfo.isNull()) {
        *ok = false;
        QString error = QString(QStringLiteral("failed to parse JSON file, error: %1, data: %2"))
                                .arg(parseError.errorString()).arg(jsonData);
        emit errorOccurred(error);
    }

    return jsonInfo;
}

void QOTAClientAsync::multiprocessLock(const QString &method)
{
    qCDebug(qota) << QTime::currentTime().toString() << method << "- waiting for lock...";
    ostree_sysroot_lock (m_sysroot, 0);
    qCDebug(qota) << QTime::currentTime().toString() << " lock acquired";
}

void QOTAClientAsync::multiprocessUnlock()
{
    ostree_sysroot_unlock (m_sysroot);
    qCDebug(qota) << QTime::currentTime().toString() << "lock released";
}

QString QOTAClientAsync::defaultRevision()
{
    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    OstreeDeployment *firstDeployment = (OstreeDeployment*)deployments->pdata[0];
    return QLatin1String(ostree_deployment_get_csum (firstDeployment));
}

void QOTAClientAsync::_initialize()
{
    multiprocessLock(QStringLiteral("_initialize"));
    ostree_sysroot_load (m_sysroot, 0, 0);

    OstreeDeployment *bootedDeployment = (OstreeDeployment*)ostree_sysroot_get_booted_deployment (m_sysroot);
    QString bootedRev = QLatin1String(ostree_deployment_get_csum (bootedDeployment));
    bool ok = true;
    QJsonDocument bootedInfo = info(QOTAClientPrivate::QueryTarget::Booted, &ok);
    QString defaultRev = defaultRevision();
    // prepopulate with what we think is on the remote server (head of the local repo)
    QString remoteRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    QJsonDocument remoteInfo = info(QOTAClientPrivate::QueryTarget::Remote, &ok, remoteRev);

    resetRollbackState();
    emit initializeFinished(defaultRev, bootedRev, bootedInfo, remoteRev, remoteInfo);
    multiprocessUnlock();
}

void QOTAClientAsync::_fetchRemoteInfo()
{
    multiprocessLock(QStringLiteral("_fetchRemoteInfo"));
    QString remoteRev;
    QJsonDocument remoteInfo;
    bool ok = true;
    ostree(QStringLiteral("ostree pull --commit-metadata-only qt-os linux/qt"), &ok);
    if (ok) ostree(QStringLiteral("ostree pull --subpath=/usr/etc/qt-ota.json qt-os linux/qt"), &ok);
    if (ok) remoteRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    if (ok) remoteInfo = info(QOTAClientPrivate::QueryTarget::Remote, &ok, remoteRev);
    emit fetchRemoteInfoFinished(remoteRev, remoteInfo, ok);
    multiprocessUnlock();
}

void QOTAClientAsync::_update(const QString &updateToRev)
{
    bool ok = true;
    QString defaultRev;
    QString kernelArgs;

    multiprocessLock(QStringLiteral("_update"));
    emit statusStringChanged(QStringLiteral("Checking for missing objects..."));
    ostree(QString(QStringLiteral("ostree pull qt-os:%1")).arg(updateToRev), &ok, true);
    multiprocessUnlock();
    if (!ok) goto out;

    emit statusStringChanged(QStringLiteral("Deploying..."));
    kernelArgs = ostree(QString(QStringLiteral("ostree cat %1 /usr/lib/ostree-boot/kargs")).arg(updateToRev), &ok);
    if (ok) ostree(QString(QStringLiteral("ostree admin deploy --karg-none %1 linux/qt")).arg(kernelArgs), &ok, true);
    if (!ok) goto out;

    ostree_sysroot_load (m_sysroot, 0, 0);
    resetRollbackState();
    defaultRev = defaultRevision();

out:
    emit updateFinished(defaultRev, ok);
}

int QOTAClientAsync::rollbackIndex()
{
    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    if (deployments->len < 2)
        return -1;

    // 1) if we're not in the default boot index (0), it plans to prepend the
    //    booted index (1, since we can't have more than two trees) so that it
    //    becomes index 0 (default) and the current default becomes index 1.
    // 2) if we're booted into the default boot index (0), let's roll back to the previous (1)
    return 1;
}

void QOTAClientAsync::resetRollbackState()
{
    int index = rollbackIndex();
    if (index == -1)
        return;

    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    OstreeDeployment *rollbackDeployment = (OstreeDeployment*)deployments->pdata[index];
    QString rollbackRev = QLatin1String(ostree_deployment_get_csum (rollbackDeployment));
    bool ok = true;
    QJsonDocument rollbackInfo = info(QOTAClientPrivate::QueryTarget::Rollback, &ok, rollbackRev);
    emit rollbackChanged(rollbackRev, rollbackInfo, deployments->len);
}

void QOTAClientAsync::rollbackFailed(const QString &error)
{
    emit errorOccurred(error);
    emit rollbackFinished(QStringLiteral(""), false);
    multiprocessUnlock();
}

void QOTAClientAsync::_rollback()
{
    multiprocessLock(QStringLiteral("_rollback"));
    ostree_sysroot_load (m_sysroot, 0, 0);
    int index = rollbackIndex();
    if (index == -1)
        return rollbackFailed(QStringLiteral("At least 2 system versions required for rollback"));

    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    g_autoptr(GPtrArray) newDeployments = g_ptr_array_new_with_free_func (g_object_unref);
    g_ptr_array_add (newDeployments, g_object_ref (deployments->pdata[index]));
    for (uint i = 0; i < deployments->len; i++) {
        if (i == (uint)index)
          continue;
        g_ptr_array_add (newDeployments, g_object_ref (deployments->pdata[i]));
    }

    // atomically update bootloader configuration
    if (!ostree_sysroot_write_deployments (m_sysroot, newDeployments, 0, 0))
        return rollbackFailed(QStringLiteral("Failed to update bootloader configuration"));

    resetRollbackState();
    QString defaultRev = defaultRevision();
    emit rollbackFinished(defaultRev, true);
    multiprocessUnlock();
}

QT_END_NAMESPACE
