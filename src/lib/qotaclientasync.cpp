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

#include <QtCore/QJsonDocument>

QT_BEGIN_NAMESPACE

#define OSTREE_STATIC_DELTA_META_ENTRY_FORMAT "(uayttay)"
#define OSTREE_STATIC_DELTA_FALLBACK_FORMAT "(yaytt)"
#define OSTREE_STATIC_DELTA_SUPERBLOCK_FORMAT "(a{sv}tayay" OSTREE_COMMIT_GVARIANT_STRING "aya" OSTREE_STATIC_DELTA_META_ENTRY_FORMAT "a" OSTREE_STATIC_DELTA_FALLBACK_FORMAT ")"

// from libglnx
#define GLNX_DEFINE_CLEANUP_FUNCTION0(Type, name, func) \
  static inline void name (void *v) \
  { \
    if (*(Type*)v) \
      func (*(Type*)v); \
  }
#define glnx_unref_object __attribute__ ((cleanup(glnx_local_obj_unref)))
GLNX_DEFINE_CLEANUP_FUNCTION0(GObject*, glnx_local_obj_unref, g_object_unref)

QOtaClientAsync::QOtaClientAsync()
{
    // async mapper
    connect(this, &QOtaClientAsync::fetchRemoteMetadata, this, &QOtaClientAsync::_fetchRemoteMetadata);
    connect(this, &QOtaClientAsync::update, this, &QOtaClientAsync::_update);
    connect(this, &QOtaClientAsync::rollback, this, &QOtaClientAsync::_rollback);
    connect(this, &QOtaClientAsync::updateOffline, this, &QOtaClientAsync::_updateOffline);
    connect(this, &QOtaClientAsync::updateRemoteMetadataOffline, this, &QOtaClientAsync::_updateRemoteMetadataOffline);
}

QOtaClientAsync::~QOtaClientAsync()
{
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

OstreeSysroot* QOtaClientAsync::defaultSysroot()
{
    GError *error = nullptr;
    OstreeSysroot *sysroot = ostree_sysroot_new_default();
    if (!ostree_sysroot_load (sysroot, 0, &error))
        emitGError(error);
    return sysroot;
}

QString QOtaClientAsync::metadataFromRev(const QString &rev, bool *ok)
{
    QString jsonData;
    jsonData = ostree(QString(QStringLiteral("ostree cat %1 /usr/etc/qt-ota.json")).arg(rev), ok);
    if (jsonData.isEmpty())
        return jsonData;

    QJsonParseError parseError;
    QJsonDocument jsonMetadata = QJsonDocument::fromJson(jsonData.toUtf8(), &parseError);
    if (jsonMetadata.isNull()) {
        *ok = false;
        QString error = QString(QStringLiteral("failed to parse JSON file, error: %1, data: %2"))
                                .arg(parseError.errorString()).arg(jsonData);
        emit errorOccurred(error);
    }

    QByteArray ba = jsonMetadata.toJson();
    return QString::fromUtf8(ba);
}

bool QOtaClientAsync::refreshMetadata(QOtaClientPrivate *d)
{
    glnx_unref_object OstreeSysroot *sysroot = defaultSysroot();
    if (!sysroot)
        return false;

    bool ok = true;
    if (d) {
        // This is non-nullptr only when called from QOtaClient's constructor.
        // Booted revision can change only when a device is rebooted.
        OstreeDeployment *bootedDeployment = (OstreeDeployment*)ostree_sysroot_get_booted_deployment (sysroot);
        QString bootedRev = QLatin1String(ostree_deployment_get_csum (bootedDeployment));
        QString bootedMetadata = metadataFromRev(bootedRev, &ok);
        if (!ok)
            return false;
        d->setBootedMetadata(bootedRev, bootedMetadata);
    }

    // prepopulate with what we think is on the remote server (head of the local repo)
    QString remoteRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    QString remoteMetadata;
    if (ok) remoteMetadata = metadataFromRev(remoteRev, &ok);
    if (!ok)
        return false;
    emit remoteMetadataChanged(remoteRev, remoteMetadata);

    ok = handleRevisionChanges(sysroot);
    return ok;
}

void QOtaClientAsync::_fetchRemoteMetadata()
{
    QString remoteRev;
    QString remoteMetadata;
    bool ok = true;
    ostree(QStringLiteral("ostree pull --commit-metadata-only --disable-static-deltas qt-os linux/qt"), &ok);
    if (ok) remoteRev = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    if (ok) ostree(QString(QStringLiteral("ostree pull --subpath=/usr/etc/qt-ota.json qt-os %1")).arg(remoteRev), &ok);
    if (ok) remoteMetadata = metadataFromRev(remoteRev, &ok);
    if (ok) emit remoteMetadataChanged(remoteRev, remoteMetadata);
    emit fetchRemoteMetadataFinished(ok);
}

bool QOtaClientAsync::deployCommit(const QString &commit, OstreeSysroot *sysroot)
{
    bool ok = true;
    QString kernelArgs;
    GError *error = nullptr;
    g_autoptr(GFile) root = nullptr;
    OstreeRepo *repo = nullptr;

    // read kernel args for rev
    if (!ostree_sysroot_get_repo (sysroot, &repo, 0, &error) ||
        !ostree_repo_read_commit (repo, commit.toLatin1().constData(), &root, nullptr, nullptr, &error)) {
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
    glnx_unref_object OstreeSysroot *sysroot = defaultSysroot();
    if (!sysroot) {
        emit updateFinished(false);
        return;
    }

    bool ok = true;
    emit statusStringChanged(QStringLiteral("Checking for missing objects..."));
    ostree(QString(QStringLiteral("ostree pull qt-os:%1")).arg(updateToRev), &ok, true);
    if (!ok || !deployCommit(updateToRev, sysroot)) {
        emit updateFinished(false);
        return;
    }

    ok = handleRevisionChanges(sysroot, true);
    emit updateFinished(ok);
}

int QOtaClientAsync::rollbackIndex(OstreeSysroot *sysroot)
{
    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (sysroot);
    if (deployments->len < 2)
        return -1;

    // 1) if we're not in the default boot index (0), it plans to prepend the
    //    booted index (1, since we can't have more than two trees) so that it
    //    becomes index 0 (default) and the current default becomes index 1.
    // 2) if we're booted into the default boot index (0), let's roll back to the previous (1)
    return 1;
}

bool QOtaClientAsync::handleRevisionChanges(OstreeSysroot *sysroot, bool reloadSysroot)
{
    if (reloadSysroot) {
        GError *error = nullptr;
        if (!ostree_sysroot_load (sysroot, 0, &error)) {
            emitGError(error);
            return false;
        }
    }

    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (sysroot);
    OstreeDeployment *firstDeployment = (OstreeDeployment*)deployments->pdata[0];
    QString defaultRev(QLatin1String(ostree_deployment_get_csum (firstDeployment)));
    emit defaultRevisionChanged(defaultRev);

    int index = rollbackIndex(sysroot);
    if (index != -1) {
        OstreeDeployment *rollbackDeployment = (OstreeDeployment*)deployments->pdata[index];
        QString rollbackRev(QLatin1String(ostree_deployment_get_csum (rollbackDeployment)));
        bool ok = true;
        QString rollbackMetadata = metadataFromRev(rollbackRev, &ok);
        if (!ok)
            return false;
        emit rollbackMetadataChanged(rollbackRev, rollbackMetadata, deployments->len);
    }

    return true;
}

void QOtaClientAsync::emitGError(GError *error)
{
    if (!error)
        return;

    emit errorOccurred(QString::fromLatin1((error->message)));
    g_error_free (error);
}

void QOtaClientAsync::_rollback()
{
    glnx_unref_object OstreeSysroot *sysroot = defaultSysroot();
    if (!sysroot) {
        emit rollbackFinished(false);
        return;
    }

    int index = rollbackIndex(sysroot);
    if (index == -1) {
        emit errorOccurred(QStringLiteral("At least 2 system versions required for rollback"));
        emit rollbackFinished(false);
        return;
    }

    g_autoptr(GPtrArray) deployments = ostree_sysroot_get_deployments (sysroot);
    g_autoptr(GPtrArray) newDeployments = g_ptr_array_new_with_free_func (g_object_unref);
    g_ptr_array_add (newDeployments, g_object_ref (deployments->pdata[index]));
    for (uint i = 0; i < deployments->len; i++) {
        if (i == (uint)index)
          continue;
        g_ptr_array_add (newDeployments, g_object_ref (deployments->pdata[i]));
    }

    // atomically update bootloader configuration
    GError *error = nullptr;
    if (!ostree_sysroot_write_deployments (sysroot, newDeployments, 0, &error)) {
        emitGError(error);
        emit rollbackFinished(false);
        return;
    }

    bool ok = handleRevisionChanges(sysroot, true);
    emit rollbackFinished(ok);
}

bool QOtaClientAsync::extractPackage(const QString &packagePath, OstreeSysroot *sysroot, QString *updateToRev)
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
    OstreeRepo *repo = nullptr;
    if (!ostree_sysroot_get_repo (sysroot, &repo, 0, &error)) {
        emitGError(error);
        return false;
    }
    QString currentCommit = ostree(QStringLiteral("ostree rev-parse linux/qt"), &ok);
    if (!ok || !ostree_repo_load_commit (repo, currentCommit.toLatin1().constData(),
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

    QString remoteMetadata;
    ostree(QString(QStringLiteral("ostree reset qt-os:linux/qt %1")).arg(*updateToRev), &ok);
    if (ok) remoteMetadata = metadataFromRev(*updateToRev, &ok);
    if (ok) emit remoteMetadataChanged(*updateToRev, remoteMetadata);
    return ok;
}

void QOtaClientAsync::_updateRemoteMetadataOffline(const QString &packagePath)
{
    QString rev;
    glnx_unref_object OstreeSysroot *sysroot = defaultSysroot();
    bool ok = sysroot && extractPackage(packagePath, sysroot, &rev);
    emit updateRemoteMetadataOfflineFinished(ok);
}

void QOtaClientAsync::_updateOffline(const QString &packagePath)
{
    QString rev;
    glnx_unref_object OstreeSysroot *sysroot = defaultSysroot();
    bool ok = sysroot && extractPackage(packagePath, sysroot, &rev) &&
            deployCommit(rev, sysroot) && handleRevisionChanges(sysroot, true);

    emit updateOfflineFinished(ok);
}

QT_END_NAMESPACE
