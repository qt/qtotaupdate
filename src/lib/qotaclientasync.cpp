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

#define OSTREE_STATIC_DELTA_META_ENTRY_FORMAT "(uayttay)"
#define OSTREE_STATIC_DELTA_FALLBACK_FORMAT "(yaytt)"
#define OSTREE_STATIC_DELTA_SUPERBLOCK_FORMAT "(a{sv}tayay" OSTREE_COMMIT_GVARIANT_STRING "aya" OSTREE_STATIC_DELTA_META_ENTRY_FORMAT "a" OSTREE_STATIC_DELTA_FALLBACK_FORMAT ")"

QOtaClientAsync::QOtaClientAsync() :
    m_sysroot(ostree_sysroot_new_default())
{
    // async mapper
    connect(this, &QOtaClientAsync::initialize, this, &QOtaClientAsync::_initialize);
    connect(this, &QOtaClientAsync::fetchRemoteInfo, this, &QOtaClientAsync::_fetchRemoteInfo);
    connect(this, &QOtaClientAsync::update, this, &QOtaClientAsync::_update);
    connect(this, &QOtaClientAsync::rollback, this, &QOtaClientAsync::_rollback);
    connect(this, &QOtaClientAsync::updateOffline, this, &QOtaClientAsync::_updateOffline);
    connect(this, &QOtaClientAsync::updateRemoteInfoOffline, this, &QOtaClientAsync::_updateRemoteInfoOffline);
}

QOtaClientAsync::~QOtaClientAsync()
{
    ostree_sysroot_unload (m_sysroot);
}

static void parseErrorString(QString *error)
{
    error->remove(0, qstrlen("error: "));
    if (error->startsWith(QLatin1String("Remote")) && error->endsWith(QLatin1String("not found")))
        *error = QLatin1String("Repository configuration not found");
}

QString QOtaClientAsync::ostree(const QString &command, bool *ok, bool updateStatus)
{
    qCDebug(qota) << command;
    QProcess ostree;
    ostree.setProcessChannelMode(QProcess::MergedChannels);
    ostree.start(command);
    if (!ostree.waitForStarted()) {
        *ok = false;
        emit errorOccurred(QLatin1String("Failed to start: ") + command
                         + QLatin1String(" : ") + ostree.errorString());
        return QString();
    }

    QString out;
    bool finished = false;
    do {
        finished = ostree.waitForFinished(200);
        if (!finished && ostree.error() != QProcess::Timedout) {
            *ok = false;
            emit errorOccurred(QLatin1String("Process failed: ") + command +
                               QLatin1String(" : ") + ostree.errorString());
            return QString();
        }
        while (ostree.canReadLine()) {
            QByteArray bytesRead = ostree.readLine().trimmed();
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

QJsonDocument QOtaClientAsync::info(QOtaClientPrivate::QueryTarget target, bool *ok, const QString &rev)
{
    QString jsonData;
    switch (target) {
    case QOtaClientPrivate::QueryTarget::Booted: {
        QFile metadata(QStringLiteral("/usr/etc/qt-ota.json"));
        if (metadata.open(QFile::ReadOnly))
            jsonData = QString::fromLatin1(metadata.readAll());
        break;
    }
    case QOtaClientPrivate::QueryTarget::Remote:
        jsonData = ostree(QString(QStringLiteral("ostree cat %1 /usr/etc/qt-ota.json")).arg(rev), ok);
        break;
    case QOtaClientPrivate::QueryTarget::Rollback:
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

void QOtaClientAsync::_initialize()
{
    GError *error = nullptr;
    if (!ostree_sysroot_load (m_sysroot, 0, &error) ||
        !ostree_sysroot_get_repo (m_sysroot, &m_repo, 0, &error)) {
        emitGError(error);
        emit initializeFinished(false);
        return;
    }

    handleRevisionChanges();

    bool ok = true;
    // prepopulate with what we think is on the remote server (head of the local repo)
    QString remoteRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    QJsonDocument remoteInfo;
    if (ok) remoteInfo = info(QOtaClientPrivate::QueryTarget::Remote, &ok, remoteRev);
    if (!ok) {
        emit initializeFinished(false);
        return;
    }
    emit remoteInfoChanged(remoteRev, remoteInfo);

    OstreeDeployment *bootedDeployment = (OstreeDeployment*)ostree_sysroot_get_booted_deployment (m_sysroot);
    QString bootedRev = QLatin1String(ostree_deployment_get_csum (bootedDeployment));
    QJsonDocument bootedInfo = info(QOtaClientPrivate::QueryTarget::Booted, &ok);
    emit initializeFinished(ok, bootedRev, bootedInfo);
}

void QOtaClientAsync::_fetchRemoteInfo()
{
    QString remoteRev;
    QJsonDocument remoteInfo;
    bool ok = true;
    ostree(QStringLiteral("ostree pull --commit-metadata-only --disable-static-deltas qt-os linux/qt"), &ok);
    if (ok) ostree(QStringLiteral("ostree pull --subpath=/usr/etc/qt-ota.json qt-os linux/qt"), &ok);
    if (ok) remoteRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    if (ok) remoteInfo = info(QOtaClientPrivate::QueryTarget::Remote, &ok, remoteRev);
    if (ok) emit remoteInfoChanged(remoteRev, remoteInfo);
    emit fetchRemoteInfoFinished(ok);
}

bool QOtaClientAsync::deployCommit(const QString &commit)
{
    bool ok = true;
    QString kernelArgs;
    GError *error = nullptr;
    g_autoptr(GFile) root = nullptr;
    if (!ostree_repo_read_commit (m_repo, commit.toLatin1().constData(),
                                  &root, nullptr, nullptr, &error)) {
        emitGError(error);
        return false;
    }
    g_autoptr(GFile) kargsInRev = g_file_resolve_relative_path (root, "/usr/lib/ostree-boot/kargs");
    g_autoptr(GInputStream) in = (GInputStream*)g_file_read (kargsInRev, 0, 0);
    if (in)
        kernelArgs = ostree(QString(QStringLiteral("ostree cat %1 /usr/lib/ostree-boot/kargs")).arg(commit), &ok);

    emit statusStringChanged(QStringLiteral("Deploying..."));
    if (ok) ostree(QString(QStringLiteral("ostree admin deploy --karg-none %1 %2"))
                   .arg(kernelArgs).arg(commit), &ok, true);
    return ok;
}

void QOtaClientAsync::_update(const QString &updateToRev)
{
    bool ok = true;
    GError *error = nullptr;
    emit statusStringChanged(QStringLiteral("Checking for missing objects..."));
    ostree(QString(QStringLiteral("ostree pull qt-os:%1")).arg(updateToRev), &ok, true);
    if (!ok || !deployCommit(updateToRev)) {
        emit updateFinished(false);
        return;
    }

    if (!ostree_sysroot_load (m_sysroot, 0, &error)) {
        emitGError(error);
        emit updateFinished(false);
        return;
    }

    handleRevisionChanges();
    emit updateFinished(ok);
}

int QOtaClientAsync::rollbackIndex()
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

void QOtaClientAsync::handleRevisionChanges()
{
    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    OstreeDeployment *firstDeployment = (OstreeDeployment*)deployments->pdata[0];
    QString defaultRev(QLatin1String(ostree_deployment_get_csum (firstDeployment)));
    emit defaultRevisionChanged(defaultRev);

    int index = rollbackIndex();
    if (index != -1) {
        OstreeDeployment *rollbackDeployment = (OstreeDeployment*)deployments->pdata[index];
        QString rollbackRev(QLatin1String(ostree_deployment_get_csum (rollbackDeployment)));
        bool ok = true;
        QJsonDocument rollbackInfo = info(QOtaClientPrivate::QueryTarget::Rollback, &ok, rollbackRev);
        emit rollbackInfoChanged(rollbackRev, rollbackInfo, deployments->len);
    }
}

bool QOtaClientAsync::emitGError(GError *error)
{
    if (!error)
        return false;

    emit errorOccurred(QString::fromLatin1((error->message)));
    g_error_free (error);
    return true;
}

void QOtaClientAsync::_rollback()
{
    GError *error = nullptr;
    if (!ostree_sysroot_load (m_sysroot, 0, &error)) {
        emitGError(error);
        emit rollbackFinished(false);
        return;
    }

    int index = rollbackIndex();
    if (index == -1) {
        emit errorOccurred(QStringLiteral("At least 2 system versions required for rollback"));
        emit rollbackFinished(false);
        return;
    }

    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (m_sysroot);
    g_autoptr(GPtrArray) newDeployments = g_ptr_array_new_with_free_func (g_object_unref);
    g_ptr_array_add (newDeployments, g_object_ref (deployments->pdata[index]));
    for (uint i = 0; i < deployments->len; i++) {
        if (i == (uint)index)
          continue;
        g_ptr_array_add (newDeployments, g_object_ref (deployments->pdata[i]));
    }

    // atomically update bootloader configuration
    if (!ostree_sysroot_write_deployments (m_sysroot, newDeployments, 0, &error)) {
        emitGError(error);
        emit rollbackFinished(false);
        return;
    }

    handleRevisionChanges();
    emit rollbackFinished(true);
}

bool QOtaClientAsync::extractPackage(const QString &packagePath, QString *updateToRev)
{
    GError *error = nullptr;
    // load delta superblock
    GMappedFile *mfile = g_mapped_file_new (packagePath.toLatin1().data(), FALSE, &error);
    if (!mfile) {
        emitGError(error);
        return false;
    }
    g_autoptr(GBytes) bytes = g_mapped_file_get_bytes (mfile);
    g_mapped_file_unref (mfile);
    g_autoptr(GVariant) deltaSuperblock = nullptr;
    deltaSuperblock = g_variant_new_from_bytes (G_VARIANT_TYPE (OSTREE_STATIC_DELTA_SUPERBLOCK_FORMAT),
                                                bytes, FALSE);
    g_variant_ref_sink (deltaSuperblock);

    // get a timestamp of the commit object from the superblock
    g_autoptr(GVariant) packageCommitV = g_variant_get_child_value (deltaSuperblock, 4);
    if (!ostree_validate_structureof_commit (packageCommitV, &error)) {
        emitGError(error);
        return false;
    }
    guint64 packageTimestamp = ostree_commit_get_timestamp (packageCommitV);
    // get timestamp of the head commit from the repository
    bool ok = true;
    g_autoptr(GVariant) currentCommitV = nullptr;
    QString currentCommit = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    if (!ok || !ostree_repo_load_commit (m_repo, currentCommit.toLatin1().constData(),
                                         &currentCommitV, nullptr, &error)) {
        emitGError(error);
        return false;
    }
    guint64 currentTimestamp = ostree_commit_get_timestamp (currentCommitV);
    qCDebug(qota) << "current timestamp:" << currentTimestamp;
    qCDebug(qota) << "package timestamp:" << packageTimestamp;
    if (packageTimestamp < currentTimestamp) {
        emit errorOccurred(QString(QStringLiteral("Not allowed to downgrade - current timestamp: %1,"
                           " package timestamp: %2")).arg(currentTimestamp).arg(packageTimestamp));
        return false;
    }

    emit statusStringChanged(QStringLiteral("Extracting the update package..."));
    ostree(QString(QStringLiteral("ostree static-delta apply-offline %1")).arg(packagePath), &ok);
    if (!ok) return false;

    g_autoptr(GVariant) toCsumV = g_variant_get_child_value (deltaSuperblock, 3);
    if (!ostree_validate_structureof_csum_v (toCsumV, &error)) {
        emitGError(error);
        return false;
    }

    g_autofree char *toCsum = ostree_checksum_from_bytes_v (toCsumV);
    *updateToRev = QString::fromLatin1(toCsum);

    QJsonDocument remoteInfo;
    ostree(QString(QStringLiteral("ostree reset qt-os:linux/qt %1")).arg(*updateToRev), &ok);
    if (ok) remoteInfo = infoFromRev(*updateToRev, &ok);
    if (ok) emit remoteInfoChanged(*updateToRev, remoteInfo);
    return ok;
}

void QOtaClientAsync::_updateRemoteInfoOffline(const QString &packagePath)
{
    QString rev;
    bool success = extractPackage(packagePath, &rev);
    emit updateRemoteInfoOfflineFinished(success);
}

void QOtaClientAsync::_updateOffline(const QString &packagePath)
{
    bool success = true;
    QString rev;
    if (!extractPackage(packagePath, &rev) || !deployCommit(rev))
        success = false;

    if (success)
        handleRevisionChanges();

    emit updateOfflineFinished(success);
}

QT_END_NAMESPACE
