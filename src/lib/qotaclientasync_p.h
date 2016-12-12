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
#ifndef QOTACLIENTASYNC_P_H
#define QOTACLIENTASYNC_P_H

#include "qotaclient.h"
#include "qotaclient_p.h"

#include <QtCore/QObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QProcess>

QT_BEGIN_NAMESPACE

struct OstreeSysroot;
struct OstreeRepo;
// from gerror.h
typedef struct _GError GError;

class QOtaClientAsync : public QObject
{
    Q_OBJECT
public:
    QOtaClientAsync();
    virtual ~QOtaClientAsync();
    QString ostree(const QString &command, bool *ok, bool updateStatus = false);

signals:
    void initialize();
    void initializeFinished(bool success, const QString &bootedRev = QString(),
                            const QJsonDocument &bootedInfo = QJsonDocument());
    void fetchRemoteInfo();
    void fetchRemoteInfoFinished(bool success);
    void update(const QString &updateToRev);
    void updateFinished(bool success);
    void rollback();
    void rollbackFinished(bool success);
    void updateOffline(const QString &packagePath);
    void updateOfflineFinished(bool success);
    void updateRemoteInfoOffline(const QString &packagePath);
    void updateRemoteInfoOfflineFinished(bool success);
    void rollbackInfoChanged(const QString &rollbackRev, const QJsonDocument &rollbackInfo, int treeCount);
    void errorOccurred(const QString &error);
    void statusStringChanged(const QString &status);
    void remoteInfoChanged(const QString &remoteRev, const QJsonDocument &remoteInfo);
    void defaultRevisionChanged(const QString &defaultRevision);

protected:
    QJsonDocument infoFromRev(const QString &rev, bool *ok);
    int rollbackIndex();
    void handleRevisionChanges();
    bool emitGError(GError *error);
    bool deployCommit(const QString &commit);
    bool extractPackage(const QString &packagePath, QString *updateToRev);

    void _initialize();
    void _fetchRemoteInfo();
    void _update(const QString &updateToRev);
    void _rollback();
    void _updateOffline(const QString &packagePath);
    void _updateRemoteInfoOffline(const QString &packagePath);

private:
    OstreeSysroot *m_sysroot;
    OstreeRepo *m_repo;
};

QT_END_NAMESPACE

#endif // QOTACLIENTASYNC_P_H
