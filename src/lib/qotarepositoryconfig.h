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
#ifndef QOTAREPOSITORYCONFIG_H
#define QOTAREPOSITORYCONFIG_H

#include <QtCore/QObject>
#include <QtCore/QString>

QT_BEGIN_NAMESPACE

class QOtaClient;
class QOtaRepositoryConfigPrivate;

class Q_DECL_EXPORT QOtaRepositoryConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(bool gpgVerify READ gpgVerify WRITE setGpgVerify NOTIFY gpgVerifyChanged)
    Q_PROPERTY(QString tlsClientCertPath READ tlsClientCertPath WRITE setTlsClientCertPath NOTIFY tlsClientCertPathChanged)
    Q_PROPERTY(QString tlsClientKeyPath READ tlsClientKeyPath WRITE setTlsClientKeyPath NOTIFY tlsClientKeyPathChanged)
    Q_PROPERTY(bool tlsPermissive READ tlsPermissive WRITE setTlsPermissive NOTIFY tlsPermissiveChanged)
    Q_PROPERTY(QString tlsCaPath READ tlsCaPath WRITE setTlsCaPath NOTIFY tlsCaPathChanged)
public:
    explicit QOtaRepositoryConfig(QObject *parent = nullptr);
    virtual ~QOtaRepositoryConfig();

    void setUrl(const QString &url);
    QString url() const;

    void setGpgVerify(bool verify);
    bool gpgVerify() const;

    void setTlsClientCertPath(const QString &certPath);
    QString tlsClientCertPath() const;

    void setTlsClientKeyPath(const QString &keyPath);
    QString tlsClientKeyPath() const;

    void setTlsPermissive(bool permissive);
    bool tlsPermissive() const;

    void setTlsCaPath(const QString &caPath);
    QString tlsCaPath() const;

Q_SIGNALS:
    void urlChanged();
    void gpgVerifyChanged();
    void tlsClientCertPathChanged();
    void tlsClientKeyPathChanged();
    void tlsPermissiveChanged();
    void tlsCaPathChanged();

private:
    Q_DISABLE_COPY(QOtaRepositoryConfig)
    Q_DECLARE_PRIVATE(QOtaRepositoryConfig)
    QOtaRepositoryConfigPrivate *const d_ptr;
    friend class QOtaClient;
};

QT_END_NAMESPACE

#endif // QOTAREPOSITORYCONFIG_H
