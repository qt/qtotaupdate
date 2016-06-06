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

class QOTAClientPrivate;

class Q_DECL_EXPORT QOTAClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool otaEnabled READ otaEnabled)
    Q_PROPERTY(bool initialized READ initialized NOTIFY initializationFinished)
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool restartRequired READ restartRequired NOTIFY restartRequiredChanged)
    Q_PROPERTY(QString error READ errorString NOTIFY errorOccurred)
    Q_PROPERTY(QString clientVersion READ clientVersion NOTIFY initializationFinished)
    Q_PROPERTY(QString clientDescription READ clientDescription NOTIFY initializationFinished)
    Q_PROPERTY(QString clientRevision READ clientRevision NOTIFY initializationFinished)
    Q_PROPERTY(QByteArray clientInfo READ clientInfo NOTIFY initializationFinished)
    Q_PROPERTY(QString serverVersion READ serverVersion NOTIFY serverInfoChanged)
    Q_PROPERTY(QString serverDescription READ serverDescription NOTIFY serverInfoChanged)
    Q_PROPERTY(QString serverRevision READ serverRevision NOTIFY serverInfoChanged)
    Q_PROPERTY(QByteArray serverInfo READ serverInfo NOTIFY serverInfoChanged)
    Q_PROPERTY(QString rollbackVersion READ rollbackVersion NOTIFY rollbackInfoChanged)
    Q_PROPERTY(QString rollbackDescription READ rollbackDescription NOTIFY rollbackInfoChanged)
    Q_PROPERTY(QString rollbackRevision READ rollbackRevision NOTIFY rollbackInfoChanged)
    Q_PROPERTY(QByteArray rollbackInfo READ rollbackInfo NOTIFY rollbackInfoChanged)
public:
    explicit QOTAClient(QObject *parent = Q_NULLPTR);
    ~QOTAClient();

    bool updateAvailable() const;
    bool restartRequired() const;
    bool otaEnabled() const;
    bool initialized() const;
    QString errorString() const;

    Q_INVOKABLE bool fetchServerInfo() const;
    Q_INVOKABLE bool update() const;
    Q_INVOKABLE bool rollback() const;

    QString clientVersion() const;
    QString clientDescription() const;
    QString clientRevision() const;
    QByteArray clientInfo() const;

    QString serverVersion() const;
    QString serverDescription() const;
    QString serverRevision() const;
    QByteArray serverInfo() const;

    QString rollbackVersion() const;
    QString rollbackDescription() const;
    QString rollbackRevision() const;
    QByteArray rollbackInfo() const;

Q_SIGNALS:
    void initializationFinished();
    void serverInfoChanged();
    void rollbackInfoChanged();
    void updateAvailableChanged(bool available);
    void restartRequiredChanged(bool required);
    void errorOccurred(const QString &error);

    void fetchServerInfoFinished(bool success);
    void updateFinished(bool success);
    void rollbackFinished(bool success);

private:
    Q_DISABLE_COPY(QOTAClient)
    Q_DECLARE_PRIVATE(QOTAClient)
    QOTAClientPrivate *const d_ptr;
};

QT_END_NAMESPACE

#endif // QOTACLIENT_H
