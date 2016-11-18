/*
 *  Copyright (C) 2012-2016 Skylable Ltd. <info-copyright@skylable.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Special exception for linking this software with OpenSSL:
 *
 *  In addition, as a special exception, Skylable Ltd. gives permission to
 *  link the code of this program with the OpenSSL library and distribute
 *  linked combinations including the two. You must obey the GNU General
 *  Public License in all respects for all of the code used other than
 *  OpenSSL. You may extend this exception to your version of the program,
 *  but you are not obligated to do so. If you do not wish to do so, delete
 *  this exception statement from your version.
 */

#include "clusterconfig.h"
#include "sxauth.h"

namespace configKeys
{
    static const char *SX_CLUSTER { "sxCluster" };
    static const char *SX_AUTH { "sxAuth" };
    static const char *SX_CLUSTID { "sxClusterUUID" };
    static const char *SX_USERNAME { "sxUsername" };
    static const char *USE_SSL { "useSSL" };
    static const char *SSL_PORT { "sslPort" };
    static const char *CERT_FPRINT { "sslCertFingerprint" };
    static const char *SECONDARY_CERT_FPRINT { "sslSecondaryCertFingerprint" };
    static const char *SXWEB_CERT_FPRINT { "sxwebCertFingerprint" };
    static const char *SX_ADDRESS { "sxAddress" };
    static const char *VCLUSTER {"vCluster"};
};

QString ClusterConfig::cluster() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::SX_CLUSTER).toString();
}

QByteArray ClusterConfig::uuid() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(configKeys::SX_CLUSTID).toByteArray();
}

QString ClusterConfig::address() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::SX_ADDRESS).toString();
}

QString ClusterConfig::token() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::SX_AUTH).toString();
}

QString ClusterConfig::username() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::SX_USERNAME).toString();
}

QString ClusterConfig::vcluster() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::VCLUSTER).toString();
}

void ClusterConfig::setVcluster(const QString &vcluster)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(configKeys::VCLUSTER, vcluster);
}

QByteArray ClusterConfig::clusterCertFp() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::CERT_FPRINT).toByteArray();
}

void ClusterConfig::setClusterCertFp(const QByteArray &certFp)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(configKeys::CERT_FPRINT, certFp);
}

QByteArray ClusterConfig::secondaryClusterCertFp() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::SECONDARY_CERT_FPRINT).toByteArray();
}

void ClusterConfig::setSecondaryClusterCertFp(const QByteArray &certFp)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(configKeys::SECONDARY_CERT_FPRINT, certFp);
}

QByteArray ClusterConfig::sxwebCertFp() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::SXWEB_CERT_FPRINT).toByteArray();
}

bool ClusterConfig::ssl() const
{
    QMutexLocker locker(&mMutex);
    return  mSettings.value(configKeys::USE_SSL).toBool();
}

int ClusterConfig::port() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(configKeys::SSL_PORT).toInt();
}

SxAuth ClusterConfig::sxAuth() const
{
    QString sxCluster = cluster();
    QString sxAddress = address();
    bool sxSsl = ssl();
    int sxPort = port();
    QString sxToken = token();
    return SxAuth(sxCluster, sxAddress, sxSsl, sxPort, sxToken);
}

void ClusterConfig::setConfig(const QString &cluster, const QString &uuid, const QString &token, const QString &initialAddress, bool useSsl, int port, const QByteArray &certFp)
{
    QMutexLocker locker(&mMutex);
    mSettings.remove("delete");
    mSettings.setValue(configKeys::SX_CLUSTER, cluster);
    mSettings.setValue(configKeys::SX_CLUSTID, uuid);
    mSettings.setValue(configKeys::SX_AUTH, token);
    mSettings.setValue(configKeys::SX_ADDRESS, initialAddress);
    mSettings.setValue(configKeys::USE_SSL, useSsl);
    mSettings.setValue(configKeys::SSL_PORT, port);
    mSettings.setValue(configKeys::CERT_FPRINT, certFp);
    mValid = validate();
}

void ClusterConfig::setUsername(const QString &username)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(configKeys::SX_USERNAME, username);
}

bool ClusterConfig::isValid()
{
    return mValid;
}

ClusterConfig::ClusterConfig(QSettings &settings, QMutex &mutex)
    : mSettings(settings), mMutex(mutex)
{
    mValid = validate();
}

void ClusterConfig::setSxwebCertFp(const QByteArray &certFp)
{
    if (certFp != sxwebCertFp()) {
        mSettings.setValue(configKeys::SXWEB_CERT_FPRINT, certFp);
        mSettings.sync();
    }
}

bool ClusterConfig::validate()
{
    QVariant value;
    value = mSettings.value(configKeys::SX_CLUSTER);
    if (value.type() != QVariant::String)
        return false;
    value = mSettings.value(configKeys::SX_AUTH);
    if (value.type() != QVariant::String)
        return false;
    value = mSettings.value(configKeys::SX_CLUSTID);
    if (value.type() != QVariant::String)
        return false;
    value = mSettings.value(configKeys::USE_SSL);
    if (value.toString()!="true" && value.toString()!="false")
        return false;
    value = mSettings.value(configKeys::SSL_PORT);
    bool ok;
    int port = value.toInt(&ok);
    if (!ok || port <= 0)
        return false;
    value = mSettings.value(configKeys::CERT_FPRINT);
    if (value.type() != QVariant::Invalid && value.type() != QVariant::ByteArray)
        return false;
    value = mSettings.value(configKeys::SXWEB_CERT_FPRINT);
    if (value.type() != QVariant::Invalid && value.type() != QVariant::ByteArray)
        return false;
    value = mSettings.value(configKeys::SX_ADDRESS);
    if (value.type() != QVariant::Invalid && value.type() != QVariant::String)
        return false;
    value = mSettings.value(configKeys::SX_USERNAME);
    if (value.type() != QVariant::Invalid && value.type() != QVariant::String)
        return false;
    value = mSettings.value(configKeys::VCLUSTER);
    if (value.type() != QVariant::Invalid && value.type() != QVariant::String)
        return false;
    return true;
}
