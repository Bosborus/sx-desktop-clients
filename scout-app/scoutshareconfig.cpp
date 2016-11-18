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

#include "scoutshareconfig.h"
#include "scoutclusterconfig.h"
#include <QVariant>

namespace ConfigKeys {
    static const char *LINK_EXP_TIME {"linkExpiration"};
    static const char *NOTIFY_EMAIL {"notifyEmail"};
}


void ScoutShareConfig::saveConfig()
{
    mSettings.sync();
}

QString ScoutShareConfig::clusterToken() const
{
    return mClusterConfig->token();
}

QString ScoutShareConfig::volumePath(const QString &) const
{
    return QString();
}

QStringList ScoutShareConfig::volumes() const
{
    return QStringList();
}

qint64 ScoutShareConfig::expirationTime() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(QString("share/")+ConfigKeys::LINK_EXP_TIME, 0).toLongLong();
}

void ScoutShareConfig::setExpirationTime(qint64 expTime)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(QString("share/")+ConfigKeys::NOTIFY_EMAIL, expTime);
}

QString ScoutShareConfig::notifyEmail() const
{
    QMutexLocker locker(&mMutex);
    return mSettings.value(QString("share/")+ConfigKeys::LINK_EXP_TIME, "").toString();
}

void ScoutShareConfig::setNotifyEmail(const QString &email)
{
    QMutexLocker locker(&mMutex);
    mSettings.setValue(QString("share/")+ConfigKeys::LINK_EXP_TIME, email);
}

QByteArray ScoutShareConfig::sxwebCertFp() const
{
    return mClusterConfig->sxwebCertFp();
}

void ScoutShareConfig::setSxwebCertFp(const QByteArray &certFp)
{
    mClusterConfig->setClusterCertFp(certFp);
}

ScoutShareConfig::ScoutShareConfig(ScoutClusterConfig *clusterConfig, QSettings &settings, QMutex &mutex)
    : mSettings(settings), mMutex(mutex)
{
    mClusterConfig = clusterConfig;
}
