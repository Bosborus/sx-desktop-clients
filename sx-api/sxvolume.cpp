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

#include "sxvolume.h"
#include <QDebug>
#include "sxcluster.h"

SxVolume::SxVolume(const SxCluster *cluster, QString name, QString owner, qint64 size, qint64 usedSize, bool canRead, bool canWrite, QString globalId)
{
    mCluster = cluster;
    mName = name;
    mOwner=owner;
    mSize = size;
    mUsedSize = usedSize;
    mCanRead = canRead;
    mCanWrite= canWrite;
    mGlobalId = globalId;
}

SxVolume::~SxVolume()
{

}

QString SxVolume::name() const
{
    return mName;
}

QString SxVolume::owner() const
{
    return mOwner;
}

qint64 SxVolume::size() const
{
    return mSize;
}

qint64 SxVolume::usedSize() const
{
    return mUsedSize;
}

qint64 SxVolume::freeSize() const
{
    if (mUsedSize >= mSize)
        return 0;
    return mSize - mUsedSize;
}

bool SxVolume::canRead() const
{
    return mCanRead;
}

bool SxVolume::canWrite() const
{
    return mCanWrite;
}

SxMeta &SxVolume::meta()
{
    return mMeta;
}

QString SxVolume::globalId() const
{
    return mGlobalId;
}

const SxMeta &SxVolume::meta() const
{
    return mMeta;
}

SxMeta &SxVolume::customMeta()
{
    return mCustomMeta;
}

const SxMeta &SxVolume::customMeta() const
{
    return mCustomMeta;
}

const SxCluster* SxVolume::cluster() const
{
    return mCluster;
}

void SxVolume::setSize(qint64 size)
{
    mSize = size;
}

void SxVolume::setUsedSize(qint64 usedSize)
{
    mUsedSize = usedSize;
}

void SxVolume::setPerms(bool canRead, bool canWrite)
{
    mCanRead = canRead;
    mCanWrite = canWrite;
}

void SxVolume::setNodeList(const QStringList &nodes)
{
    mNodeList = nodes;
}

void SxVolume::setGlobalId(const QString &globalId)
{
    mGlobalId = globalId;
}

const QStringList &SxVolume::nodeList() const
{
    return mNodeList;
}

