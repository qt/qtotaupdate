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

#include "qotaclient.h"

#include <QtCore/QObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QByteArray>
#include <QtCore/QThread>
#include <QtCore/QScopedPointer>
#include <QtCore/QJsonDocument>

Q_DECLARE_LOGGING_CATEGORY(qota)

class QOTAClientAsync;

class QOTAClientPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QOTAClient)
public:
    enum class QueryTarget
    {
        Client,
        Server,
        Rollback
    };
    Q_ENUM(QueryTarget)

    QOTAClientPrivate(QOTAClient *client);
    virtual ~QOTAClientPrivate();

    void refreshState();
    void initializeFinished(const QString &defaultRev,
                            const QString &clientRev, const QJsonDocument &clientInfo,
                            const QString &serverRev, const QJsonDocument &serverInfo);
    void fetchServerInfoFinished(const QString &serverRev, const QJsonDocument &serverInfo, bool success);
    void updateFinished(const QString &defaultRev, bool success);
    void rollbackFinished(const QString &defaultRev, bool success);
    void statusStringChanged(const QString &status);
    void errorOccurred(const QString &error);
    void rollbackChanged(const QString &rollbackRev, const QJsonDocument &rollbackInfo, int treeCount);

    QString version(QueryTarget target) const;
    QString description(QueryTarget target) const;
    QString revision(QueryTarget target) const;
    QByteArray info(QueryTarget target) const;

    void updateServerInfo(const QString &serverRev, const QJsonDocument &serverInfo);
    bool isReady() const;

    // members
    QOTAClient *const q_ptr;
    bool m_initialized;
    bool m_updateAvailable;
    bool m_rollbackAvailable;
    bool m_restartRequired;
    bool m_otaEnabled;
    QString m_status;
    QString m_error;
    QThread *m_otaAsyncThread;
    QScopedPointer<QOTAClientAsync> m_otaAsync;
    QString m_defaultRev;

    QString m_clientVersion;
    QString m_clientDescription;
    QString m_clientRev;
    QByteArray m_clientInfo;

    QString m_serverVersion;
    QString m_serverDescription;
    QString m_serverRev;
    QByteArray m_serverInfo;

    QString m_rollbackVersion;
    QString m_rollbackDescription;
    QString m_rollbackRev;
    QByteArray m_rollbackInfo;
};

#endif // QOTACLIENT_P_H
