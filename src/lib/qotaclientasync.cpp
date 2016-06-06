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
    connect(this, &QOTAClientAsync::fetchServerInfo, this, &QOTAClientAsync::_fetchServerInfo);
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

QString QOTAClientAsync::ostree(const QString &command, bool *ok)
{
    qCDebug(otaLog) << command;
    if (!m_ostree) {
        m_ostree = new QProcess;
        m_ostree->setProcessChannelMode(QProcess::MergedChannels);
    }
    m_ostree->start(command);
    m_ostree->waitForStarted();

    QByteArray output;
    do {
        m_ostree->waitForReadyRead();
        QByteArray bytesRead = m_ostree->readAll().trimmed();
        if (!bytesRead.isEmpty()) {
            qCDebug(otaLog) << bytesRead;
            if (bytesRead.startsWith("error:")) {
                *ok = false;
                emit errorOccurred(QString::fromUtf8(bytesRead));
            }
            output.append(bytesRead);
        }
    } while (m_ostree->state() != QProcess::NotRunning);

    return QString::fromUtf8(output);
}

QJsonDocument QOTAClientAsync::info(QOTAClientPrivate::QueryTarget target, bool *ok, const QString &rev)
{
    QString jsonData;
    switch (target) {
    case QOTAClientPrivate::QueryTarget::Client: {
        QFile metadata(QStringLiteral("/usr/etc/qt-ota.json"));
        if (metadata.open(QFile::ReadOnly))
            jsonData = QString::fromLatin1(metadata.readAll());
        break;
    }
    case QOTAClientPrivate::QueryTarget::Server:
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
    qCDebug(otaLog) << QTime::currentTime() << method << "- waiting for lock...";
    ostree_sysroot_lock (m_sysroot, 0);
    qCDebug(otaLog) << QTime::currentTime() << " lock acquired";
}

void QOTAClientAsync::multiprocessUnlock()
{
    ostree_sysroot_unlock (m_sysroot);
    qCDebug(otaLog) << QTime::currentTime() << "lock released";
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
    QString clientRev = QLatin1String(ostree_deployment_get_csum (bootedDeployment));
    bool ok = true;
    QJsonDocument clientInfo = info(QOTAClientPrivate::QueryTarget::Client, &ok);
    QString defaultRev = defaultRevision();
    // prepopulate with what we think is on the server (head of the local repo)
    QString serverRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    QJsonDocument serverInfo = info(QOTAClientPrivate::QueryTarget::Server, &ok, serverRev);

    refreshRollbackState();
    emit initializeFinished(defaultRev, clientRev, clientInfo, serverRev, serverInfo);
    multiprocessUnlock();
}

void QOTAClientAsync::_fetchServerInfo()
{
    multiprocessLock(QStringLiteral("_fetchServerInfo"));
    QString serverRev;
    QJsonDocument serverInfo;
    bool ok = true;
    ostree(QStringLiteral("ostree pull --commit-metadata-only qt-os linux/qt"), &ok);
    if (ok) ostree(QStringLiteral("ostree pull --subpath=/usr/etc/qt-ota.json qt-os linux/qt"), &ok);
    if (ok) serverRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    if (ok) serverInfo = info(QOTAClientPrivate::QueryTarget::Server, &ok, serverRev);
    emit fetchServerInfoFinished(serverRev, serverInfo, ok);
    multiprocessUnlock();
}

void QOTAClientAsync::_update(const QString &updateToRev)
{
    bool ok = true;
    ostree(QString(QStringLiteral("ostree admin upgrade --override-commit=%1")).arg(updateToRev), &ok);
    if (!ok) {
        emit updateFinished(QStringLiteral(""), ok);
        return;
    }

    multiprocessLock(QStringLiteral("_update"));
    ostree_sysroot_load (m_sysroot, 0, 0);

    refreshRollbackState();
    QString defaultRev = defaultRevision();
    emit updateFinished(defaultRev, ok);
    multiprocessUnlock();
}

int QOTAClientAsync::rollbackIndex()
{
    OstreeDeployment *bootedDeployment = ostree_sysroot_get_booted_deployment (m_sysroot);
    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    if (deployments->len < 2)
        return -1;

    uint bootedIndex = 0;
    for (bootedIndex = 0; bootedIndex < deployments->len; bootedIndex++) {
        if (deployments->pdata[bootedIndex] == bootedDeployment)
          break;
    }

    int rollbackIndex;
    if (bootedIndex != 0) {
         // what this does is, if we're not in the default boot index, it plans to prepend
         // the booted index (1, since we can't have more than two trees) so that it becomes
         // index 0 (default) and the current default becomes index 1
        rollbackIndex = bootedIndex;
    } else {
        // we're booted into the first, let's roll back to the previous
        rollbackIndex = 1;
    }

    return rollbackIndex;
}

void QOTAClientAsync::refreshRollbackState(int index)
{
    if (index == -2)
        // index not provided
        index = rollbackIndex();
    if (index == -1)
        // no rollback available
        return;

    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    OstreeDeployment *rollbackDeployment = (OstreeDeployment*)deployments->pdata[index];
    QString rollbackRev = QLatin1String(ostree_deployment_get_csum (rollbackDeployment));
    bool ok = true;
    QJsonDocument rollbackInfo = info(QOTAClientPrivate::QueryTarget::Rollback, &ok, rollbackRev);
    emit rollbackChanged(rollbackRev, rollbackInfo);
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

    refreshRollbackState(index);
    QString defaultRev = defaultRevision();
    emit rollbackFinished(defaultRev, true);
    multiprocessUnlock();
}

QT_END_NAMESPACE
