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
#include "qotarepositoryconfig_p.h"
#include "qotarepositoryconfig.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtCore/QTextStream>

QT_BEGIN_NAMESPACE

const QString url(QStringLiteral("url="));
const QString clientCert(QStringLiteral("tls-client-cert-path="));
const QString clientKey(QStringLiteral("tls-client-key-path="));
const QString ca(QStringLiteral("tls-ca-path="));
// these 'bool' values by default are 'false' in QOtaRepositoryConfig
const QString gpg(QStringLiteral("gpg-verify=true"));
const QString tls(QStringLiteral("tls-permissive=true"));

QOtaRepositoryConfigPrivate::QOtaRepositoryConfigPrivate(QOtaRepositoryConfig *repo) :
    q_ptr(repo),
    m_gpgVerify(false),
    m_tlsPermissive(false)
{
}

QOtaRepositoryConfigPrivate::~QOtaRepositoryConfigPrivate()
{
}

QOtaRepositoryConfig *QOtaRepositoryConfigPrivate::repositoryConfigFromFile(const QString &configPath) const
{
    if (!QDir().exists(configPath))
        return nullptr;

    QFile config(configPath);
    if (!config.open(QFile::ReadOnly))
        return nullptr;

    QOtaRepositoryConfig *conf = new QOtaRepositoryConfig();
    QTextStream in(&config);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        // string(s)
        if (line.startsWith(url))
            conf->setUrl(line.mid(url.length()));
        else if (line.startsWith(clientCert))
            conf->setTlsClientCertPath(line.mid(clientCert.length()));
        else if (line.startsWith(clientKey))
            conf->setTlsClientKeyPath(line.mid(clientKey.length()));
        else if (line.startsWith(ca))
            conf->setTlsCaPath(line.mid(ca.length()));
        // bool(s)
        else if (line.startsWith(gpg))
            conf->setGpgVerify(true);
        else if (line.startsWith(tls))
            conf->setTlsPermissive(true);
    }

    return conf;
}

/*!
    \inqmlmodule QtOtaUpdate
    \qmltype OtaRepositoryConfig
    \instantiates QOtaRepositoryConfig
    \brief Used to configure the OSTree repository.

    OtaRepositoryConfig
    \include qotarepositoryconfig.cpp repository-config-description
*/

/*!
    \class QOtaRepositoryConfig
    \inmodule qtotaupdate
    \brief Used to configure the OSTree repository.

    QOtaRepositoryConfig
//! [repository-config-description]
    provides an API to configure the OSTree repository (located in \c {/ostree/repo}). The
    update process synchronizes the local repository with the remote repository (see \l url).
    The local repository keeps history for the current and the previous snapshot of the system.

    This class is used to configure TLS authentication and whether to utilize GPG for update
    integrity verification.

//! [repository-config-description]
*/

/*!
    \qmlsignal OtaRepositoryConfig::urlChanged()

    This signal is emitted when the value of \l url changes.
*/

/*!
    \fn void QOtaRepositoryConfig::urlChanged()

    This signal is emitted when the value of \l url changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::gpgVerifyChanged()

    This signal is emitted when the value of gpgVerify changes.
*/

/*!
    \fn void QOtaRepositoryConfig::gpgVerifyChanged()

    This signal is emitted when the value of gpgVerify changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsClientCertPathChanged()

    This signal is emitted when the value of tlsClientCertPath changes.
*/

/*!
    \fn void QOtaRepositoryConfig::tlsClientCertPathChanged()

    This signal is emitted when the value of tlsClientCertPath changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsClientKeyPathChanged()

    This signal is emitted when the value of tlsClientKeyPath changes.
*/

/*!
    \fn void QOtaRepositoryConfig::tlsClientKeyPathChanged()

    This signal is emitted when the value of tlsClientKeyPath changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsPermissiveChanged()

    This signal is emitted when the value of tlsPermissive changes.
*/

/*!
    \fn void QOtaRepositoryConfig::tlsPermissiveChanged()

    This signal is emitted when the value of tlsPermissive changes.
*/

/*!
    \qmlsignal OtaRepositoryConfig::tlsCaPathChanged()

    This signal is emitted when the value of tlsCaPath changes.
*/

/*!
    \fn void QOtaRepositoryConfig::tlsCaPathChanged()

    This signal is emitted when the value of tlsCaPath changes.
*/

QOtaRepositoryConfig::QOtaRepositoryConfig(QObject *parent) :
    QObject(parent),
    d_ptr(new QOtaRepositoryConfigPrivate(this))
{
}

QOtaRepositoryConfig::~QOtaRepositoryConfig()
{
    delete d_ptr;
}

void QOtaRepositoryConfig::setUrl(const QString &url)
{
    Q_D(QOtaRepositoryConfig);
    if (url.trimmed() == d->m_url)
        return;

    d->m_url = url.trimmed();
    emit urlChanged();
}

/*!
    \qmlproperty string OtaRepositoryConfig::url

    Holds a URL to a remote OSTree repository.
    The supported schemes are \c http and \c https.
*/

/*!
    \property QOtaRepositoryConfig::url

    Holds a URL to a remote OSTree repository.
    The supported schemes are \c http and \c https.
*/
QString QOtaRepositoryConfig::url() const
{
    return d_func()->m_url;
}

void QOtaRepositoryConfig::setGpgVerify(bool verify)
{
    Q_D(QOtaRepositoryConfig);
    if (verify == d->m_gpgVerify)
        return;

    d->m_gpgVerify = verify;
    emit gpgVerifyChanged();
}

/*!
    \qmlproperty bool OtaRepositoryConfig::gpgVerify

    Holds whether OSTree will require system updates to be signed by a known GPG key.

    Default is \c false.
*/

/*!
    \property QOtaRepositoryConfig::gpgVerify

    Holds whether OSTree will require system updates to be signed by a known GPG key.

    Default is \c false.
*/
bool QOtaRepositoryConfig::gpgVerify() const
{
    return d_func()->m_gpgVerify;
}

void QOtaRepositoryConfig::setTlsClientCertPath(const QString &certPath)
{
    Q_D(QOtaRepositoryConfig);
    if (certPath.trimmed() == d->m_clientCertPath)
        return;

    d->m_clientCertPath = certPath.trimmed();
    emit tlsClientCertPathChanged();
}

/*!
    \qmlproperty string OtaRepositoryConfig::tlsClientCertPath

    Holds a path to a client-side certificate, to present when making requests
    to the remote server.
*/

/*!
    \property QOtaRepositoryConfig::tlsClientCertPath

    Holds a path to a client-side certificate, to present when making requests
    to the remote server.
*/
QString QOtaRepositoryConfig::tlsClientCertPath() const
{
    return d_func()->m_clientCertPath;
}

void QOtaRepositoryConfig::setTlsClientKeyPath(const QString &keyPath)
{
    Q_D(QOtaRepositoryConfig);
    if (keyPath.trimmed() == d->m_clientKeyPath)
        return;

    d->m_clientKeyPath = keyPath.trimmed();
    emit tlsClientKeyPathChanged();
}

/*!
    \qmlproperty string OtaRepositoryConfig::tlsClientKeyPath

    Holds a path to a client-side certificate's key, to present when making requests
    to the remote server.
*/

/*!
    \property QOtaRepositoryConfig::tlsClientKeyPath

    Holds a path to a client-side certificate's key, to present when making requests
    to the remote server.
*/
QString QOtaRepositoryConfig::tlsClientKeyPath() const
{
    return d_func()->m_clientKeyPath;
}

void QOtaRepositoryConfig::setTlsPermissive(bool permissive)
{
    Q_D(QOtaRepositoryConfig);
    if (permissive == d->m_tlsPermissive)
        return;

    d->m_tlsPermissive = permissive;
    emit tlsPermissiveChanged();
}

/*!
    \qmlproperty bool OtaRepositoryConfig::tlsPermissive

    Holds whether to check the server's TLS certificates against the system's certificate store.

    Default is \c false.
*/

/*!
    \property QOtaRepositoryConfig::tlsPermissive

    Holds whether to check the server's TLS certificates against the system's certificate store.

    Default is \c false.
*/
bool QOtaRepositoryConfig::tlsPermissive() const
{
    return d_func()->m_tlsPermissive;
}

void QOtaRepositoryConfig::setTlsCaPath(const QString &caPath)
{
    Q_D(QOtaRepositoryConfig);
    if (caPath.trimmed() == d->m_tlsCaPath)
        return;

    d->m_tlsCaPath = caPath.trimmed();
    emit tlsCaPathChanged();
}

/*!
    \qmlproperty string OtaRepositoryConfig::tlsCaPath

    Holds a path to a file containing trusted anchors instead of the system's CA database.
*/

/*!
    \property QOtaRepositoryConfig::tlsCaPath

    Holds a path to a file containing trusted anchors instead of the system's CA database.
*/
QString QOtaRepositoryConfig::tlsCaPath() const
{
    return d_func()->m_tlsCaPath;
}

QT_END_NAMESPACE
