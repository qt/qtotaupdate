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

QT_BEGIN_NAMESPACE

class QOtaClientPrivate;
class QOtaRepositoryConfig;

class Q_DECL_EXPORT QOtaClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool otaEnabled READ otaEnabled CONSTANT)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool rollbackAvailable READ rollbackAvailable NOTIFY rollbackAvailableChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)
    Q_PROPERTY(QString error READ errorString NOTIFY errorOccurred)
    Q_PROPERTY(QString status READ statusString NOTIFY statusStringChanged)
    Q_PROPERTY(QString bootedRevision READ bootedRevision CONSTANT)
    Q_PROPERTY(QString bootedMetadata READ bootedMetadata CONSTANT)
    Q_PROPERTY(QString remoteRevision READ remoteRevision NOTIFY remoteMetadataChanged)
    Q_PROPERTY(QString remoteMetadata READ remoteMetadata NOTIFY remoteMetadataChanged)
    Q_PROPERTY(QString rollbackRevision READ rollbackRevision NOTIFY rollbackMetadataChanged)
    Q_PROPERTY(QString rollbackMetadata READ rollbackMetadata NOTIFY rollbackMetadataChanged)
    Q_PROPERTY(QString defaultRevision READ defaultRevision NOTIFY defaultMetadataChanged)
    Q_PROPERTY(QString defaultMetadata READ defaultMetadata NOTIFY defaultMetadataChanged)
public:
    static QOtaClient& instance();
    virtual ~QOtaClient();

    bool updateAvailable() const;
    bool rollbackAvailable() const;
    bool restartRequired() const;
    bool otaEnabled() const;
    QString errorString() const;
    QString statusString() const;

    Q_INVOKABLE bool fetchRemoteMetadata();
    Q_INVOKABLE bool update();
    Q_INVOKABLE bool rollback();
    Q_INVOKABLE bool updateOffline(const QString &packagePath);
    Q_INVOKABLE bool updateRemoteMetadataOffline(const QString &packagePath);
    Q_INVOKABLE bool refreshMetadata();
    Q_INVOKABLE bool setRepositoryConfig(QOtaRepositoryConfig *config);
    Q_INVOKABLE bool removeRepositoryConfig();
    Q_INVOKABLE bool isRepositoryConfigSet(QOtaRepositoryConfig *config) const;
    Q_INVOKABLE QOtaRepositoryConfig *repositoryConfig() const;

    QString bootedRevision() const;
    QString bootedMetadata() const;
    QString remoteRevision() const;
    QString remoteMetadata() const;
    QString rollbackRevision() const;
    QString rollbackMetadata() const;
    QString defaultRevision() const;
    QString defaultMetadata() const;

Q_SIGNALS:
    void remoteMetadataChanged();
    void rollbackMetadataChanged();
    void defaultMetadataChanged();
    void updateAvailableChanged(bool available);
    void rollbackAvailableChanged();
    void restartRequiredChanged(bool required);
    void statusStringChanged(const QString &status);
    void errorOccurred(const QString &error);
    void repositoryConfigChanged(QOtaRepositoryConfig *config);

    void fetchRemoteMetadataFinished(bool success);
    void updateFinished(bool success);
    void rollbackFinished(bool success);
    void updateOfflineFinished(bool success);
    void updateRemoteMetadataOfflineFinished(bool success);

private:
    QOtaClient();
    Q_DISABLE_COPY(QOtaClient)
    Q_DECLARE_PRIVATE(QOtaClient)
    QOtaClientPrivate *const d_ptr;
};

QT_END_NAMESPACE

#endif // QOTACLIENT_H
