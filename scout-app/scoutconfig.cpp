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

#include "scoutconfig.h"
#include "scoutclusterconfig.h"

ScoutConfig::ScoutConfig()
{
    mSettings = new QSettings();
    mClusterConfig = new ScoutClusterConfig(*mSettings, mMutex);
    mShareConfig = new ScoutShareConfig(mClusterConfig, *mSettings, mMutex);
}

ScoutConfig::~ScoutConfig()
{
    delete mShareConfig;
    delete mClusterConfig;
    delete mSettings;
}

ClusterConfig *ScoutConfig::clusterConfig() const
{
    return mClusterConfig;
}

ScoutShareConfig *ScoutConfig::shareConfig() const
{
    return mShareConfig;
}

bool ScoutConfig::isValid() const
{
    return mClusterConfig->isValid();
}

bool ScoutConfig::checkVersion() const
{
    return mSettings->value("update/checkVersion", true).toBool();
}

bool ScoutConfig::checkBetaVersion() const
{
    return mSettings->value("update/checkBetaVersion", true).toBool();
}

void ScoutConfig::setCheckVersion(bool checkVersion, bool checkBetaVersion)
{
    mSettings->setValue("update/checkVersion", checkVersion);
    mSettings->setValue("update/checkBetaVersion", checkBetaVersion);
}

qint64 ScoutConfig::cacheFileLimit() const
{
    return mSettings->value("cache/fileSizeLimit", 10*1024*1024).toLongLong();
}

qint64 ScoutConfig::cacheSize() const
{
    return mSettings->value("cache/size", 200*1024*1024).toLongLong();
}

void ScoutConfig::setCacheSize(qint64 fileSizeLimit, qint64 cacheSize)
{
     mSettings->setValue("cache/size", cacheSize);
     mSettings->setValue("cache/fileSizeLimit", fileSizeLimit);
}

void ScoutConfig::setCacheEnabled(bool enabled)
{
    mSettings->setValue("cache/enabled", enabled);
}

bool ScoutConfig::cacheEnabled() const
{
    return mSettings->value("cache/enabled", true).toBool();
}
