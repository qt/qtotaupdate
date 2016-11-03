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
#ifndef QOTACLIENT_H
#define QOTACLIENT_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QByteArray>

QT_BEGIN_NAMESPACE

class QOtaClientPrivate;
class QOtaRepositoryConfig;

class Q_DECL_EXPORT QOtaClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool otaEnabled READ otaEnabled)
    Q_PROPERTY(bool initialized READ initialized NOTIFY initializationFinished)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool rollbackAvailable READ rollbackAvailable NOTIFY rollbackAvailableChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)
    Q_PROPERTY(QString error READ errorString NOTIFY errorOccurred)
    Q_PROPERTY(QString status READ statusString NOTIFY statusStringChanged)
    Q_PROPERTY(QString bootedVersion READ bootedVersion NOTIFY initializationFinished)
    Q_PROPERTY(QString bootedDescription READ bootedDescription NOTIFY initializationFinished)
    Q_PROPERTY(QString bootedRevision READ bootedRevision NOTIFY initializationFinished)
    Q_PROPERTY(QByteArray bootedInfo READ bootedInfo NOTIFY initializationFinished)
    Q_PROPERTY(QString remoteVersion READ remoteVersion NOTIFY remoteInfoChanged)
    Q_PROPERTY(QString remoteDescription READ remoteDescription NOTIFY remoteInfoChanged)
    Q_PROPERTY(QString remoteRevision READ remoteRevision NOTIFY remoteInfoChanged)
    Q_PROPERTY(QByteArray remoteInfo READ remoteInfo NOTIFY remoteInfoChanged)
    Q_PROPERTY(QString rollbackVersion READ rollbackVersion NOTIFY rollbackInfoChanged)
    Q_PROPERTY(QString rollbackDescription READ rollbackDescription NOTIFY rollbackInfoChanged)
    Q_PROPERTY(QString rollbackRevision READ rollbackRevision NOTIFY rollbackInfoChanged)
    Q_PROPERTY(QByteArray rollbackInfo READ rollbackInfo NOTIFY rollbackInfoChanged)
public:
    explicit QOtaClient(QObject *parent = nullptr);
    virtual ~QOtaClient();

    bool updateAvailable() const;
    bool rollbackAvailable() const;
    bool restartRequired() const;
    bool otaEnabled() const;
    bool initialized() const;
    QString errorString() const;
    QString statusString() const;

    Q_INVOKABLE bool fetchRemoteInfo() const;
    Q_INVOKABLE bool update() const;
    Q_INVOKABLE bool rollback() const;

    Q_INVOKABLE bool setRepositoryConfig(QOtaRepositoryConfig *config);
    Q_INVOKABLE QOtaRepositoryConfig *repositoryConfig() const;
    Q_INVOKABLE bool removeRepositoryConfig();
    Q_INVOKABLE bool repositoryConfigsEqual(QOtaRepositoryConfig *a, QOtaRepositoryConfig *b);

    QString bootedVersion() const;
    QString bootedDescription() const;
    QString bootedRevision() const;
    QByteArray bootedInfo() const;

    QString remoteVersion() const;
    QString remoteDescription() const;
    QString remoteRevision() const;
    QByteArray remoteInfo() const;

    QString rollbackVersion() const;
    QString rollbackDescription() const;
    QString rollbackRevision() const;
    QByteArray rollbackInfo() const;

Q_SIGNALS:
    void remoteInfoChanged();
    void rollbackInfoChanged();
    void updateAvailableChanged(bool available);
    void rollbackAvailableChanged();
    void restartRequiredChanged(bool required);
    void statusStringChanged(const QString &status);
    void errorOccurred(const QString &error);
    void repositoryConfigChanged(QOtaRepositoryConfig *config);

    void initializationFinished();
    void fetchRemoteInfoFinished(bool success);
    void updateFinished(bool success);
    void rollbackFinished(bool success);

private:
    Q_DISABLE_COPY(QOtaClient)
    Q_DECLARE_PRIVATE(QOtaClient)
    QOtaClientPrivate *const d_ptr;
};

QT_END_NAMESPACE

#endif // QOTACLIENT_H
