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

#include "scoutqueue.h"
#include "scoutmodel.h"
#include <QMutexLocker>
#include <QTemporaryFile>
#include "sxlog.h"
#include "util.h"

ScoutQueue::ScoutQueue(ClusterConfig *config, QObject *parent)
    : QAbstractItemModel(parent)
{
    static const int cRegister = qRegisterMetaType<QVector<int>>("QVector<int>");
    Q_UNUSED(cRegister);
    mCluster = nullptr;
    mCurrentTask = nullptr;
    mClusterConfig = config;
    mCurrentFileSize = 0;
    mCurrentFileProgress = 0;
    mCurrentTaskProgress = 0;
    connect(this, &ScoutQueue::startTask, this, &ScoutQueue::executeTask, Qt::QueuedConnection);
}

void ScoutQueue::appendTask(ScoutTask *task)
{
    QMutexLocker locker(&mMutex);

    if (mCurrentTask != nullptr && !mCurrentTask->error.isEmpty()) {
        int index = mPendingList.length()+mFailedTasks.length();
        beginInsertRows(mTasksIndex, index, index);
        mFailedTasks.append(mCurrentTask);
        endInsertRows();
        mCurrentTask = nullptr;
    }
    if (mCurrentTask == nullptr) {
        mCurrentTask = task;
        mCurrentTaskProgress = 0;
        locker.unlock();
        emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
        emit startTask();
    }
    else {
        int index = mPendingList.length();
        beginInsertRows(mTasksIndex, index, index);
        mPendingList.append(task);
        endInsertRows();
    }
}

QVariant ScoutQueue::currentTaskData(int role) const
{
    QMutexLocker locker(&mMutex);
    if (mCurrentTask == nullptr)
        return QVariant();
    if (role == DirectionRole)
        return mCurrentTask->upload ? "upload" : "download";
    if (role == TitleRole)
        return !mCurrentFile.isEmpty() ? mCurrentFile : mCurrentTask->title;
    if (role == SizeRole)
        return mCurrentTask->size;
    if (role == ProgressRole)
        return mCurrentTaskProgress+mCurrentFileProgress;
    if (role == ErrorRole)
        return mCurrentTask->error;
    return QVariant();
}

QVariant ScoutQueue::pendingTaskData(int index, int role) const
{
    QMutexLocker locker(&mMutex);
    if (index < 0 || index >= mPendingList.count()+mFailedTasks.count())
        return QVariant();
    const QList<ScoutTask *> *list;
    if (index >= mPendingList.count()) {
        index-=mPendingList.count();
        list = &mFailedTasks;
    }
    else
        list = &mPendingList;
    if (role == DirectionRole)
        return list->at(index)->upload ? "upload" : "download";
    if (role == TitleRole)
        return list->at(index)->title;
    if (role == SizeRole)
        return list->at(index)->size;
    if (role == ErrorRole)
        return list->at(index)->error;
    return QVariant();
}

int ScoutQueue::pendingCount() const
{
    QMutexLocker locker(&mMutex);
    return mPendingList.count();
}

void ScoutQueue::cancelCurrentTask()
{
    QMutexLocker locker(&mMutex);
    if (mCurrentTask == nullptr)
        return;
    if (mCurrentTask->error.isEmpty()) {
        if (mCluster == nullptr)
            return;
        mCluster->abort();
    }
    else {
        delete mCurrentTask;
        if (mFailedTasks.isEmpty()) {
            mCurrentTask = nullptr;
            emit finished();
            emit sigShowWarning(false);
        }
        else {
            beginRemoveRows(mTasksIndex, 0, 0);
            mCurrentTask = mFailedTasks.takeFirst();
            endRemoveRows();
            locker.unlock();
            emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
        }
    }
}

void ScoutQueue::cancelPendingTask(int index)
{
    QMutexLocker locker(&mMutex);
    if (index < 0)
        return;
    if (index < mPendingList.count()) {
        beginRemoveRows(mTasksIndex, index, index);
        auto task = mPendingList.takeAt(index);
        if (task != nullptr)
            delete task;
        endRemoveRows();
    }
    else if (index < mPendingList.count()+mFailedTasks.count()) {
        beginRemoveRows(mTasksIndex, index, index);
        auto task = mFailedTasks.takeAt(index-mPendingList.count());
        if (task != nullptr)
            delete task;
        endRemoveRows();
    }

}

bool ScoutQueue::isWorking() const
{
    QMutexLocker locker(&mMutex);
    return mCurrentTask != nullptr;
}

void ScoutQueue::retryCurrentTask()
{
    QMutexLocker locker(&mMutex);
    if (mCurrentTask == nullptr || mCurrentTask->error.isEmpty())
        return;
    mCurrentTask->error.clear();
    mCurrentTaskProgress = 0;
    locker.unlock();
    emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
    emit startTask();
}

void ScoutQueue::retryTask(int index)
{
    QMutexLocker locker(&mMutex);
    int localIndex = index-mPendingList.count();
    if (localIndex < 0 || localIndex >= mFailedTasks.count())
        return;
    if (mCurrentTask->error.isEmpty()) {
        if (localIndex==0) {
            auto task = mFailedTasks.takeAt(localIndex);
            task->error.clear();
            mPendingList.append(task);
            emit dataChanged(ScoutQueue::index(index, 0, mTasksIndex), ScoutQueue::index(index, 0, mTasksIndex), {ErrorRole});
        }
        else {
            beginMoveRows(mTasksIndex, index, index, mTasksIndex, mPendingList.count());
            auto task = mFailedTasks.takeAt(localIndex);
            task->error.clear();
            mPendingList.append(task);
            endMoveRows();
        }
    }
    else {
        beginRemoveRows(mTasksIndex, index, index);
        auto task = mFailedTasks.takeAt(localIndex);
        task->error.clear();
        endRemoveRows();
        beginInsertRows(mTasksIndex, mPendingList.count()+mFailedTasks.count(), mPendingList.count()+mFailedTasks.count());
        mFailedTasks.append(mCurrentTask);
        endInsertRows();
        mCurrentTask = task;
        locker.unlock();
        emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
        emit startTask();
    }
}

QModelIndex ScoutQueue::currentTaskIndex() const
{
    return mCurrentTaskIndex;
}

QModelIndex ScoutQueue::tasksIndex() const
{
    return mTasksIndex;
}

bool ScoutQueue::hasFailedTasks() const
{
    QMutexLocker locker(&mMutex);
    if (!mFailedTasks.isEmpty())
        return true;
    if (mCurrentTask != nullptr && ! mCurrentTask->error.isEmpty())
        return true;
    return false;
}

void ScoutQueue::executeTask()
{
    QMutexLocker locker(&mMutex);
    if (mCurrentTask == nullptr)
        return;
    locker.unlock();
    auto checkSsl = [](QSslCertificate&, bool) -> bool {
        return true;
    };
    QString errorMessage;
    mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), checkSsl, errorMessage);
    if (mCluster == nullptr) {
        logError(errorMessage);
        qDebug() << "executeTask failed" << __LINE__;
        return;
    }
    mCluster->reloadVolumes();
    connect(mCluster, &SxCluster::sig_setProgress, [this](qint64 size, qint64) {
        mMutex.lock();
        if (size > mCurrentFileSize) {
            mCurrentTask->size += size-mCurrentFileSize;
            mCurrentFileSize = size;
        }
        mCurrentFileProgress = mCurrentFileSize - size;
        mMutex.unlock();
        emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
    });
    while (true) {
        emit sigShowWarning(!mFailedTasks.isEmpty());
        errorMessage.clear();
        mCurrentFile.clear();
        SxVolume *volume = mCluster->getSxVolume(mCurrentTask->volume);
        if (volume == nullptr) {
            qDebug() << "executeTask failed" << __LINE__;
            errorMessage = "unable to find volume";
            goto endTask;
        }
        if (mCurrentTask->upload) {
            foreach (auto file, mCurrentTask->files) {
                QString localFile = file.first;
                QString localFolder = mCurrentTask->localPath;
                QString remotePath;
                if (mCurrentTask->remotePath.endsWith("/")) {
                    QString relativePath = makeRelativeTo(localFolder, localFile);
                    remotePath = mCurrentTask->remotePath+relativePath.mid(1);
                }
                else {
                    remotePath = mCurrentTask->remotePath;
                }
                SxFileEntry fileEntry;
                QString currentFilename = remotePath.split("/").last();
                mMutex.lock();
                mCurrentFile = currentFilename;
                mCurrentFileSize = file.second;
                mCurrentFileProgress = 0;
                mMutex.unlock();
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole});
                QTemporaryFile tmpFile;
                if (localFile.endsWith("/.sxnewdir")) {
                    QFile sxnewdir(localFile);
                    if (!sxnewdir.exists()) {
                        tmpFile.open();
                        localFile = tmpFile.fileName();
                    }
                }
                if (!mCluster->uploadFile(volume, remotePath, localFile, fileEntry, nullptr, true)) {
                    qDebug() << "executeTask failed" << __LINE__;
                    mCurrentFile.clear();
                    errorMessage = mCluster->lastError().errorMessage();
                    goto endTask;
                }
                fileUploaded(mCurrentTask->volume, remotePath);
                mMutex.lock();
                mCurrentTaskProgress += mCurrentFileSize;
                mMutex.unlock();
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
            }
        }
        else {
            QString rev;
            if (mCurrentTask->files.isEmpty()) {
                rev = mCurrentTask->rev;
                mCurrentTask->files.append({mCurrentTask->remotePath, mCurrentTask->size});
            }
            foreach (auto file, mCurrentTask->files) {
                QString remoteFile = file.first;
                QString localFile;
                if (mCurrentTask->localPath.endsWith("/")) {
                    if (mCurrentTask->remotePath.endsWith("/")) {
                        QString relativePath = makeRelativeTo(mCurrentTask->remotePath, remoteFile);
                        if (relativePath.startsWith("/"))
                            localFile = mCurrentTask->localPath+relativePath.mid(1);
                        else
                            localFile = mCurrentTask->localPath+relativePath;
                    }
                    else {
                        int index = remoteFile.lastIndexOf("/");
                        localFile = mCurrentTask->localPath + remoteFile.mid(index+1);
                    }
                }
                else {
                    localFile = mCurrentTask->localPath;
                }
                SxFileEntry fileEntry;
                QString currentFilename = remoteFile.split("/").last();
                mMutex.lock();
                mCurrentFile = currentFilename;
                mCurrentFileSize = file.second;
                mCurrentFileProgress = 0;
                mMutex.unlock();
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole});
                if (!mCluster->downloadFile(volume, remoteFile, rev, localFile, fileEntry, 4)) {
                    qDebug() << "executeTask failed" << __LINE__ << mCluster->lastError().errorMessage();
                    errorMessage = mCluster->lastError().errorMessage();
                    goto endTask;
                }
                mMutex.lock();
                mCurrentTaskProgress += mCurrentFileSize;
                mMutex.unlock();
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
            }
        }
        endTask:
        mMutex.lock();
        if (errorMessage.isEmpty())
            delete mCurrentTask;
        else {
            emit sigShowWarning(true);
            mCurrentTask->error = errorMessage;
            if (mPendingList.isEmpty()) {
                mMutex.unlock();
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
                delete mCluster;
                return;
            }
            int index = mPendingList.length()+mFailedTasks.length();
            beginInsertRows(mTasksIndex, index, index);
            mFailedTasks.append(mCurrentTask);
            endInsertRows();
        }
        if (mPendingList.isEmpty()) {
            if (mFailedTasks.isEmpty()) {
                mCurrentTask = nullptr;
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
                mMutex.unlock();
                break;
            }
            else {
                beginRemoveRows(mTasksIndex, 0, 0);
                mCurrentTask = mFailedTasks.takeFirst();
                endRemoveRows();
                emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
                mMutex.unlock();
                emit sigShowWarning(true);
                return;
            }
        }
        beginRemoveRows(mTasksIndex, 0, 0);
        mCurrentTask = mPendingList.takeFirst();
        endRemoveRows();
        mCurrentTaskProgress = 0;
        emit dataChanged(mCurrentTaskIndex, mCurrentTaskIndex, {ScoutModel::NameRole, ScoutModel::MimeTypeRole, ScoutModel::SizeRole, ScoutModel::SizeUsedRole});
        mMutex.unlock();
    }
    delete mCluster;
    emit finished();
    emit sigShowWarning(false);
}

QModelIndex ScoutQueue::index(int row, int column, const QModelIndex &parent) const
{
    if (parent == mTasksIndex)
        return createIndex(row, column, const_cast<void*>(reinterpret_cast<const void*>(&mTasksIndex)));
    if (parent == mCurrentTaskIndex)
        return createIndex(row, column, const_cast<void*>(reinterpret_cast<const void*>(&mCurrentTaskIndex)));
    return QModelIndex();
}

int ScoutQueue::rowCount(const QModelIndex &parent) const
{
    if (parent == mTasksIndex)
        return mPendingList.count() + mFailedTasks.count();
    if (parent == mCurrentTaskIndex)
        return mCurrentTask == nullptr ? 0 : 1;
    return 0;
}

int ScoutQueue::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 1;
}

QVariant ScoutQueue::data(const QModelIndex &index, int role) const
{
    if (index == mCurrentTaskIndex)
        return currentTaskData(role);
    else if (index.parent() == mTasksIndex)
        return pendingTaskData(index.row(), role);
    return QVariant();
}

QModelIndex ScoutQueue::parent(const QModelIndex &child) const
{
    if (child.internalPointer() == reinterpret_cast<const void*>(&mTasksIndex))
        return mTasksIndex;
    if (child.internalPointer() == reinterpret_cast<const void*>(&mCurrentTaskIndex))
        return mCurrentTaskIndex;
    return QModelIndex();
}

ScoutTask::ScoutTask(bool upload, const QString &title, const QString &volume, const QString &localPath, const QString &remotePath)
{
    this->upload = upload;
    this->title = title;
    this->volume = volume;
    this->localPath = localPath;
    this->remotePath = remotePath;
    this->size = 0;
}

ScoutTask::~ScoutTask()
{
    foreach (QFile *file, tmpFileList) {
        delete file;
    }
}
