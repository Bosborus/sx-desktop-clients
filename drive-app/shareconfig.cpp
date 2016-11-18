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

#include "shareconfig.h"

ShareConfig::ShareConfig(SxConfig *config)
{
    mConfig = config;
}

void ShareConfig::saveConfig()
{
    mConfig->syncConfig();
}

QString ShareConfig::clusterToken() const
{
    return mConfig->clusterConfig().token();
}

QString ShareConfig::volumePath(const QString &volume) const
{
    return mConfig->volume(volume).localPath();
}

QStringList ShareConfig::volumes() const
{
    return mConfig->volumes();
}

qint64 ShareConfig::expirationTime() const
{
    return mConfig->desktopConfig().linkExpirationTime();
}

void ShareConfig::setExpirationTime(qint64 expTime)
{
    mConfig->desktopConfig().setLinkExpirationTime(static_cast<int>(expTime));
}

QString ShareConfig::notifyEmail() const
{
    return mConfig->desktopConfig().linkNotifyEmail();
}

void ShareConfig::setNotifyEmail(const QString &email)
{
    mConfig->desktopConfig().setLinkNotifyEmail(email);
}

QByteArray ShareConfig::sxwebCertFp() const
{
    return mConfig->clusterConfig().sxwebCertFp();
}

void ShareConfig::setSxwebCertFp(const QByteArray &certFp)
{
    mConfig->clusterConfig().setSxwebCertFp(certFp);
}
