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

#include "uploadqueue.h"

UploadQueue::UploadQueue()
{
    mLastFreeSize = 0;
}

bool UploadQueue::isEmpty() const
{
    return mMap.isEmpty();
}

int UploadQueue::count() const
{
    return mMap.count();
}

QPair<QString, qint64> UploadQueue::takeFirstTask()
{
    if (mMap.isEmpty())
        return {"",0};
    auto first = mMap.begin();
    qint64 size = first.key();
    QString path = first.value();
    mMap.remove(size, path);
    mLastFreeSize = -1;
    return {path, size};
}

void UploadQueue::addTask(QString path, qint64 size, qint64 volumeFreeSize)
{
    removeTask(path);
    mMap.insert(size, path);
    mSet.insert(path);
    mLastFreeSize = volumeFreeSize;
}

void UploadQueue::removeTask(QString path)
{
    if (!mSet.contains(path))
        return;
    qint64 size = mMap.key(path);
    mMap.remove(size, path);
    mSet.remove(path);
}

qint64 UploadQueue::volumeFreeSize() const
{
    return mLastFreeSize;
}

bool UploadQueue::canExecuteTask(qint64 volumeFreeSize)
{
    if (mMap.isEmpty())
        return false;
    if (mLastFreeSize != -1 && volumeFreeSize <= mLastFreeSize)
        return false;
    qint64 size = mMap.firstKey();
    return size < volumeFreeSize;
}
