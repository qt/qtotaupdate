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
#ifndef QOTACLIENT_P_H
#define QOTACLIENT_P_H

#include <QtCore/QObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QByteArray>
#include <QtCore/QScopedPointer>
#include <QtCore/QJsonDocument>

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(qota)

class QThread;
class QOtaClientAsync;
class QOtaRepositoryConfig;
class QOtaClient;

class QOtaClientPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QOtaClient)
public:
    QOtaClientPrivate(QOtaClient *client);
    virtual ~QOtaClientPrivate();

    void handleStateChanges();
    void statusStringChanged(const QString &status);
    void errorOccurred(const QString &error);
    bool verifyPathExist(const QString &path);
    void setBootedMetadata(QString &bootedRev, const QJsonDocument &bootedMetadata);
    void rollbackMetadataChanged(const QString &rollbackRev, const QJsonDocument &rollbackMetadata, int treeCount);
    void remoteMetadataChanged(const QString &remoteRev, const QJsonDocument &remoteMetadata);
    void defaultRevisionChanged(const QString &defaultRevision);

    // members
    QOtaClient *const q_ptr;
    bool m_updateAvailable;
    bool m_rollbackAvailable;
    bool m_restartRequired;
    bool m_otaEnabled;
    QString m_status;
    QString m_error;
    QThread *m_otaAsyncThread;
    QScopedPointer<QOtaClientAsync> m_otaAsync;
    QString m_defaultRev;

    QString m_bootedVersion;
    QString m_bootedDescription;
    QString m_bootedRev;
    QByteArray m_bootedMetadata;

    QString m_remoteVersion;
    QString m_remoteDescription;
    QString m_remoteRev;
    QByteArray m_remoteMetadata;

    QString m_rollbackVersion;
    QString m_rollbackDescription;
    QString m_rollbackRev;
    QByteArray m_rollbackMetadata;
};

QT_END_NAMESPACE

#endif // QOTACLIENT_P_H
