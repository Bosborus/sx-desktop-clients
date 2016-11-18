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

#include <QMutexLocker>
#include <QThread>

#include "sxdatabase.h"
#include "sxfilesystem.h"
#include "sxqueue.h"
#include "sxlog.h"
#include "sxauth.h"
#include "sxcluster.h"
#include "sxlog.h"

quint64 SxQueue::Task::sCounter = 0;
QSet<quint64> SxQueue::Task::sLivingTasks;

#define _reportError(task, errorMessage) logWarning(QString("Task ID %1: %2").arg(task->id()).arg(errorMessage))

SxQueue::SxQueue(SxConfig *config, std::function<bool(QSslCertificate&,bool)> checkSslCallback, std::function<bool(QString)> askGuiCallback)
{
    static auto registerEtaAction = qRegisterMetaType<EtaAction>("EtaAction");
    Q_UNUSED(registerEtaAction);
    logEntry("");
    mPaused = false;
    mAborted = false;
    mConfig = config;
    mCluster = nullptr;
    mCurrentTask = nullptr;
    mQueueIsWorking = false;
    mCheckSslCallback = checkSslCallback;
    mAskGuiCallback = askGuiCallback;
    connect(this, &SxQueue::sig_start_task, this, &SxQueue::startCurrentTask, Qt::QueuedConnection);
    connect(this, &SxQueue::sig_delete_timers, this, &SxQueue::deleteTimers, Qt::QueuedConnection);
    mUploadDoneCallback = [this](QString volName, QString path, SxError error, QString revision, quint32 mTime) {
        if (error.errorCode() == SxErrorCode::NoError) {
            VolumeConfig volumeConfig = mConfig->volume(volName);
            QString volumeRootDir = volumeConfig.localPath();
            emit sig_removeWarning(volName, path);
            emit sig_fileSynchronised(volumeRootDir+"/"+path, true);
            if (!revision.isEmpty()) {
                QFileInfo fileInfo(volumeRootDir+"/"+path);
                if (fileInfo.lastModified().toTime_t() == mTime)
                    SxDatabase::instance().onFileUploaded(volName, path, revision, mTime);
            }
        }
        else {
            emit sig_addWarning(volName, path, error.errorMessageTr(), false);
        }
        SxDatabase::instance().removeSuppression(volName, path);
    };
    connect(&SxDatabase::instance(), &SxDatabase::sig_possibleInconsistencyDetected, this, &SxQueue::onPossibleInconsistency);
}

SxQueue::~SxQueue()
{
    if (mCurrentTask)
        delete mCurrentTask;
    clear();
    if (mCluster)
        delete mCluster;
}

void SxQueue::addTask(SxQueue::Task *task)
{
    if (mLockedVolumes.contains(task->volume())) {
        delete task;
        return;
    }
    QMutexLocker locker(&mMutex);
    QString message = task->toString();
    logVerbose(message);
    if (task->priority() > 0)
        _instertPriorityTask(task);
    else {
        if (_appendRegularTask(task)) {
            emit sig_removeWarning(task->volume(), task->path());
            if (mPendingUploads.contains(task->volume()))
                mPendingUploads.value(task->volume())->removeTask(task->path());
        }
    }
    emit sig_start_task();
}

void SxQueue::clear()
{
    logEntry("");
    QMutexLocker locker(&mMutex);
    mEtaCounters.clear();
    emit sig_setEtaCounters(0, 0, 0, 0, 0);
    mTaskByPath.clear();
    foreach (Task *task, mTaskList) {
        delete task;
    }
    mTaskList.clear();
    emit sig_delete_timers();
    if (mCluster != nullptr) {
        mCluster->clearUploadJobs();
    }
}

void SxQueue::clear(const QString &volume)
{
    logEntry(volume);
    QMutexLocker locker(&mMutex);
    foreach (Task *task, mTaskList) {
        if (task->volume() == volume) {
            mTaskList.removeOne(task);
            mTaskByPath.remove(task->volume()+"/"+task->path());
            delete task;
        }
    }
}

void SxQueue::requestInitialScan()
{
    if (QThread::currentThread() != this->thread()) {
        QTimer::singleShot(0, this, SLOT(requestInitialScan()));
        return;
    }
    logVerbose("REQUEST INITIAL SCAN");
    if (mConfig->volumes().isEmpty()) {
        emit sig_addWarning("", "", tr("No volume configured to be synchronized"), true);
        requestVolumeList();
    }
    else {
        Task *listVolumesTask = new Task(TaskType::ListVolumes, "", "", 100, 0);
        _instertPriorityTask(listVolumesTask);

        emit sig_removeWarning("", "");
        foreach (QString volume, mConfig->volumes()) {
            mPendingUploads.insert(volume, new UploadQueue());
            Task *task = new Task(TaskType::VolumeInitialScan, volume, "", 99, 0);
            addTask(task);
        }
        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);
        mTimers.insert(timer);
        connect(timer, &QTimer::timeout, [timer, this]() {
            mTimers.remove(timer);
            timer->deleteLater();
            this->requestInitialScan();
        });
        timer->start(sTimeoutFullScan*1000);
    }
}

void SxQueue::requestRemoteList(const QString &volume)
{
    logEntry(volume);
    Task *task = new Task(TaskType::ListRemoteFiles, volume, "", 90, 0);
    addTask(task);
}

void SxQueue::requestVolumeList()
{
    logEntry("");
    Task *task = new Task(TaskType::ListVolumes, "", "", 100, 0);
    addTask(task);
}

void SxQueue::setPaused(bool paused)
{
    mPaused = paused;
    if (!paused) {
        mLockedVolumes.clear();
    }
    else {
        foreach (UploadQueue* queue, mPendingUploads) {
            delete queue;
        }
        mPendingUploads.clear();
    }
}

bool SxQueue::paused()
{
    return mPaused;
}

bool SxQueue::abortCurrentTask()
{
    QMutexLocker locker(&mMutex);
    if (mCurrentTask != nullptr) {
        mAborted = true;
        emit sig_abort_task();
        return true;
    }
    return false;
}

void SxQueue::localFileModified(QString volume, QString path, bool removed, qint64 size)
{
    if (removed)
        SxDatabase::instance().removeFileBlocks(volume, path);
    if (mPaused)
        return;

    static const QStringList ignoredFiles = {".DS_Store", "._.DS_Store"};
    if (ignoredFiles.contains(path.split("/").last()))
        return;



    Task *task = new Task(removed ? TaskType::RemoveRemoteFile : TaskType::UploadFile, volume, path, 0, size);
    addTask(task);
}

void SxQueue::cancelUploadTask(const QString &volume, const QString &path)
{
    if (mPendingUploads.contains(volume)) {
        mPendingUploads.value(volume)->removeTask(path);
        return;
    }
    QString taskPath = volume + "/" +path;
    QMutexLocker locker(&mMutex);
    if (mTaskByPath.contains(taskPath)) {
        auto task = mTaskByPath.take(taskPath);
        mTaskList.removeOne(task);
    }
}

void SxQueue::unlockVolume(const QString &volume)
{
    if (mLockedVolumes.contains(volume)) {
        mLockedVolumes.remove(volume);
        Task *task = new Task(TaskType::VolumeInitialScan, volume, "", 99, 0);
        addTask(task);
    }
}

void SxQueue::onPossibleInconsistency(const QString &volume, const QString &path)
{
    Task *task = new Task(TaskType::CheckFileConsistency, volume, path, 95, 0);
    addTask(task);
}

void SxQueue::startCurrentTask()
{
    if (mQueueIsWorking)
        return;
    mQueueIsWorking = true;
    if (mCluster == nullptr) {
        mAuth = mConfig->clusterConfig().sxAuth();
        QString errorMessage;
        mCluster = SxCluster::initializeCluster(mAuth, mConfig->clusterConfig().uuid(), mCheckSslCallback, errorMessage);
        if (mCluster == nullptr) {
            logError("UNABLE TO INITIALIZE CLUSTER");
            emit sig_satusChanged(SxStatus::inactive);
            emit sig_setEtaAction(EtaAction::Inactive, 0, "", 0, 0);
            emit sig_initializationFailed();
            clear();
            mQueueIsWorking = false;
            return;
        }
        mCluster->setGetLocalBlocksCallback([this](QFile *file, qint64 fileSize, int blockSize, const QStringList &fileBlocks, QSet<QString> &missingBlocks)->bool {
            return this->getLocalBlocks(file, fileSize, blockSize, fileBlocks, missingBlocks);
        });
        mCluster->setFindIdenticalFilesCallback([this](const QString& volume, qint64 fileSize, int blockSize, const QStringList &fileBlocks, QList<QPair<QString, quint32>>& files)->bool {
            return this->findIdenticalFiles(volume, fileSize, blockSize, fileBlocks, files);
        });
        emit sig_satusChanged(SxStatus::idle);
        emit sig_setEtaAction(EtaAction::Idle, 0, "", 0, 0);
        connect(this, &SxQueue::sig_abort_task, mCluster, &SxCluster::abort); //, Qt::DirectConnection);
        connect(mCluster, &SxCluster::sig_setProgress, this, &SxQueue::sig_setProgress);
        mCluster->reloadVolumes();
        requestVolumeList();
        emit sig_clusterInitialized(mCluster->sxwebAddress(), mCluster->sxshareAddress());
        emit sig_gotVcluster(mCluster->userInfo().vcluster());
    }
    if (mCluster->checkNetworkConfigurationChanged()) {
        if (!mPaused) {
            logInfo("Network configuration changed. Restarting queue");
            clear();
            abortCurrentTask();
            QTimer::singleShot(0, this, SLOT(requestInitialScan()));
            mQueueIsWorking = false;
            return;
        }
    }
    QMutexLocker locker(&mMutex);
    if (mCurrentTask != nullptr)
        return;
    if (mPaused) {
        emit sig_satusChanged(SxStatus::paused);
        emit sig_setEtaAction(EtaAction::Paused, 0, "", 0, 0);
        mQueueIsWorking = false;
        return;
    }
    foreach (QString volName, mPendingUploads.keys()) {
        UploadQueue *queue = mPendingUploads.value(volName);
        SxVolume *volume = mCluster->getSxVolume(volName);
        if (volume != nullptr) {
            if (queue->canExecuteTask(volume->freeSize())) {
                auto task = queue->takeFirstTask();
                auto volumeRootDir = mConfig->volume(volName).localPath();
                auto file = task.first;
                QFileInfo fileInfo(volumeRootDir+file);
                mCurrentTask = new Task(TaskType::UploadFile, volName, file, 0, fileInfo.size());
                break;
            }
        }
    }

    if (mTaskList.isEmpty() && mCurrentTask == nullptr) {
        emit sig_satusChanged(SxStatus::idle);
        emit sig_setEtaAction(EtaAction::Idle, 0, "", 0, 0);
        mQueueIsWorking = false;
        return;
    }
    mAborted = false;
    if (mCurrentTask == nullptr) {
        mCurrentTask = mTaskList.takeFirst();
        mEtaCounters.removeTask(mCurrentTask);
    }
    _emitEtaCounters();
    if (mCurrentTask->priority() > 0) {
        if (mTaskList.count() > 0 && mTaskList.last()->priority() == 0)
            emit sig_satusChanged(SxStatus::working);
        else
            emit sig_satusChanged(SxStatus::idle);
    }
    else
        emit sig_satusChanged(SxStatus::working);
    logVerbose("start "+mCurrentTask->toString());
    QString taskPath = mCurrentTask->volume()+"/"+mCurrentTask->path();
    mTaskByPath.remove(taskPath);
    locker.unlock();
    _executeCurrentTask();
    if (mCluster->lastError().errorCode()==SxErrorCode::NetworkError) {
        emit sig_addWarning("", "", mCluster->lastError().errorMessageTr(), false);
    }
    else if (mCluster->lastError().errorCode()==SxErrorCode::NotFound && mCluster->lastError().errorMessage()=="No such volume") {
        requestVolumeList();
    }
    else if (!mConfig->volumes().isEmpty()){
        emit sig_removeWarning("", "");
    }
    mQueueIsWorking = false;
    _finishCurrentTask();
}

void SxQueue::deleteTimers()
{
    foreach (QTimer* timer, mTimers) {
        timer->stop();
        delete timer;
    }
    mTimers.clear();
}

void SxQueue::lockVolume(const QString &volume)
{
    if (mLockedVolumes.contains(volume))
        return;
    mLockedVolumes.insert(volume);
    clear(volume);
    emit sig_lockVolume(volume);
}

bool SxQueue::getLocalBlocks(QFile *file, qint64 fileSize, int blockSize, const QStringList &fileBlocks, QSet<QString> &missingBlocks)
{
    QHash<QString, QList<qint64>> uniqueBlocks;
    qint64 offset = 0;
    foreach (QString block, fileBlocks) {
        if (uniqueBlocks.contains(block))
            uniqueBlocks[block].append(offset);
        else
            uniqueBlocks.insert(block, QList<qint64>{offset});
        offset+=blockSize;
    }
    missingBlocks = uniqueBlocks.keys().toSet();
    QList<std::tuple<QString, QString, qint64> > list;
    foreach (QString block, missingBlocks) {
        if (!SxDatabase::instance().findBlock(block, blockSize, list))
            continue;
        foreach (auto tuple, list) {
            QString volume = std::get<0>(tuple);
            QString path = std::get<1>(tuple);
            offset = std::get<2>(tuple);
            if (!mConfig->volumes().contains(volume))
                continue;
            QString volumeRoot = mConfig->volume(volume).localPath();
            if (volumeRoot.isEmpty())
                continue;
            QFile localFile(volumeRoot+path);
            if (!localFile.open(QIODevice::ReadOnly))
                continue;
            if (!localFile.seek(offset))
                continue;
            QByteArray data = localFile.read(blockSize);
            if (data.size() < blockSize) {
                int index = data.size();
                data.resize(blockSize);
                for(;index<blockSize; index++)
                    data[index] = 0;
            }
            QString hash = SxBlock::hashBlock(data, mCluster->uuid());
            if (hash != block)
                continue;
            missingBlocks.remove(block);
            foreach (auto writeOffset, uniqueBlocks.value(block)) {
                if (!file->seek(writeOffset)) {
                    logWarning("seek failed");
                    return false;
                }
                qint64 toWrite = blockSize;
                if (writeOffset + blockSize > fileSize)
                    toWrite = fileSize-writeOffset;
                if (!file->write(data.constData(), toWrite)) {
                    logWarning("write failed");
                    return false;
                }
            }
            break;
        }
    }
    return true;
}

bool SxQueue::findIdenticalFiles(const QString &volume, qint64 fileSize, int blockSize, const QStringList &fileBlocks, QList<QPair<QString, quint32> > &files)
{
    if (!mConfig->volumes().contains(volume))
        return false;
    QString volumeRoot = mConfig->volume(volume).localPath();
    if (volumeRoot.isEmpty())
        return false;
    QList<QPair<QString, quint32> > result;
    if (!SxDatabase::instance().findIdenticalFiles(volume, fileSize, blockSize, fileBlocks, result))
        return false;
    files.clear();
    foreach (auto pair, result) {
        files.append({volumeRoot+pair.first, pair.second});
    }
    return true;
}

bool SxQueue::_appendRegularTask(SxQueue::Task *task)
{
    if (mConfig->volume(task->volume()).isPathIgnored(task->path(), false)) {
        delete task;
        return false;
    }
    bool result = true;
    QString taskPath = task->volume()+"/"+task->path();
    if (mTaskByPath.contains(taskPath)) {
        Task* oldTask = mTaskByPath.value(taskPath);
        if (oldTask->type() != task->type()) {
            mTaskList.removeOne(oldTask);
            mEtaCounters.removeTask(oldTask);
            delete oldTask;
            mTaskList.append(task);
            mTaskByPath.insert(taskPath, task);
            mEtaCounters.addTask(task);
        }
        else {
            delete task;
            result = false;
        }
    }
    else {
        mTaskList.append(task);
        mTaskByPath.insert(taskPath, task);
        mEtaCounters.addTask(task);
    }
    _emitEtaCounters();
    return result;
}

void SxQueue::_instertPriorityTask(SxQueue::Task *task)
{
    auto insertPosition = mTaskList.end();
    for (auto it = mTaskList.begin(); it < mTaskList.end(); it++) {
        if ((*it)->priority() <= task->priority()) {
            insertPosition = it;
            break;
        }
    }
    if (insertPosition != mTaskList.end() && (*insertPosition)->priority()==task->priority()) {
        for (auto it = insertPosition; it < mTaskList.end(); it++) {
            if ((*it)->priority()==task->priority()) {
                insertPosition = it;
                if ((*insertPosition)->equal(*task)) {
                    delete task;
                    return;
                }
            }
            else
                break;
        }
    }
    mTaskList.insert(insertPosition, task);
}

void SxQueue::_finishCurrentTask()
{
    QMutexLocker locker(&mMutex);
    delete mCurrentTask;
    mCurrentTask = nullptr;
    logVerbose(QString("task finished, remaining tasks: %1").arg(mTaskList.count()));
    emit sig_start_task();
}

bool SxQueue::_reloadVolumeFiles(SxVolume *volume, const QString& volumeRootDir, const QString &etag, bool scanLocalFiles)
{
    QString volName = volume->name();
    if (!mCluster->_locateVolume(volume, 0, 0)) {
        _reportError(mCurrentTask, "unable to locate volume "+volName);
        return false;
    }
    QString _etag = etag;
    QList<SxFileEntry*> remoteFiles;
    logDebug("list remote files");
    if (!mCluster->_listFiles(volume, remoteFiles, _etag)) {
        if (mCluster->lastError().errorCode() == SxErrorCode::NotChanged) {
            logVerbose("Nothing changes");
            return true;
        }
        _reportError(mCurrentTask, mCluster->lastError().errorMessage());
        return false;
    }
    logInfo(QString("got remote list (etag: %1)").arg(_etag));
    mEtags.insert(volName, _etag);

    bool inconsistentEtag = false;
    QList<QPair<QString, QString> > etags;
    mCluster->getAllVolnodesEtag(volume, etags);
    _etag = etags.first().second;
    for (int i=1; i<etags.count();i++) {
        if (_etag != etags.at(i).second) {
            inconsistentEtag = true;
            logVerbose(QString("Detected etag mismatch on volume %1").arg(volName));
            break;
        }
    }
    if (inconsistentEtag)
        mInconsistentVolumes.insert(volName);
    else
        mInconsistentVolumes.remove(volName);

    SxDatabase& db = SxDatabase::instance();

    QStringList localFiles;
    if (scanLocalFiles) {
        if (_aborted())
            return true;
        QDir rootDir(volumeRootDir);
        logDebug("list local files");
        localFiles = SxFilesystem::getDirectoryContents(rootDir, true, "", true);
        if ((!localFiles.isEmpty() || !remoteFiles.isEmpty()) && mAskGuiCallback!= nullptr) {
            if (localFiles.isEmpty()) {
                quint32 count;
                if (db.getFilesCount(volName, true, count)) {
                    if (count != 0) {
                        if (!mAskGuiCallback(tr("Local volume (%1) is empty. Do you want to remove remote files?").arg(volName))) {
                            db.removeVolumeFiles(volName);
                        }
                    }
                }
            }
            else if (remoteFiles.isEmpty()) {
                quint32 count;
                if (db.getFilesCount(volName, false, count)) {
                    if (count != 0) {
                        if (!mAskGuiCallback(tr("Remote volume (%1) is empty. Do you want to remove local files?").arg(volName))) {
                            db.removeVolumeFiles(volName);
                        }
                    }
                }
            }
        }
    }

    db.startUpdatingFiles([this]()->bool {
                              QMutexLocker locker(&mMutex);
                              return mAborted;
                          });
    logDebug("update database - prepare");
    db.markVolumeFilesToRemove(volName, false, false);
    logDebug("update database - remote files");
    db.updateRemoteFiles(volName, remoteFiles);
    if (scanLocalFiles) {
        logDebug("update database - local files");
        db.markVolumeFilesToRemove(volName, true, true);
        db.updateLocalFiles(volName, localFiles, volumeRootDir);
    }
    logDebug("select task from database");
    QList<QString> toUpload = db.getMarkedFiles(volName, SxDatabase::ACTION::UPLOAD);
    foreach (QString file, toUpload) {
        QFileInfo fileInfo(volumeRootDir+file);
        Task *t = new Task(TaskType::UploadFile, volName, file, 0, fileInfo.size());
        _appendRegularTask(t);
    }
    QList<QString> remoteToRemove = db.getMarkedFiles(volName, SxDatabase::ACTION::REMOVE_REMOTE);
    foreach (QString file, remoteToRemove) {
        SxDatabase::instance().removeFileBlocks(volName, file);
        Task *t = new Task(TaskType::RemoveRemoteFile, volName, file, 0, 0);
        _appendRegularTask(t);
    }

    QList<QString> toDownload = db.getMarkedFiles(volName, SxDatabase::ACTION::DOWNLOAD);
    foreach (QString file, toDownload) {
        QStringList inconsistentRevisions;
        db.getInconsistentFile(volName, file, inconsistentRevisions);
        if (!inconsistentRevisions.isEmpty()) {
            QString rev = db.getRemoteFileRevision(volName, file);
            if (inconsistentRevisions.contains(rev))
                continue;
        }
        Task *t = new Task(TaskType::DownloadFile, volName, file, 0, db.getRemoteFileSize(volName, file));
        _appendRegularTask(t);
    }

    QList<QString> localToRemove = db.getMarkedFiles(volName, SxDatabase::ACTION::REMOVE_LOCAL);
    foreach (QString file, localToRemove) {
        QStringList inconsistentRevisions;
        db.getInconsistentFile(volName, file, inconsistentRevisions);
        if (!inconsistentRevisions.isEmpty()) {
            if (scanLocalFiles)
                onPossibleInconsistency(volName, file);
            continue;
        }
        Task *t = new Task(TaskType::RemoveLocalFile, volName, file, 0, 0);
        _appendRegularTask(t);
    }
    if (_aborted()) {
        clear();
    }

    db.endUpdatingFiles();
    foreach (SxFileEntry* entry, remoteFiles) {
        delete entry;
    }
    return true;
}

void SxQueue::_emitEtaCounters()
{
    emit sig_setEtaCounters(mEtaCounters.uploadCount, mEtaCounters.uploadSize,
                            mEtaCounters.downloadCount, mEtaCounters.downloadSize,
                            mEtaCounters.removeCount);
}

bool SxQueue::_aborted() const
{
    QMutexLocker locker(&mMutex);
    return mAborted;
}

void SxQueue::_executeCurrentTask()
{
    qint64 taskCount = mEtaCounters.downloadCount + mEtaCounters.uploadCount + mEtaCounters.removeCount;
    logInfo("execute "+mCurrentTask->toString());
    QString volName = mCurrentTask->volume();
    QString path = mCurrentTask->path();
    QString volumeRootDir;
    SxVolume *volume = nullptr;
    if (mLockedVolumes.contains(volName)) {
        return;
    }
    if (!volName.isEmpty()) {
        if (!mConfig->volumes().contains(volName)) {
            _reportError(mCurrentTask, "unable to find configuration for volume "+volName);
            return;
        }
        VolumeConfig volumeConfig = mConfig->volume(volName);
        volumeRootDir = volumeConfig.localPath();
        volume = mCluster->getSxVolume(volName);
        if (volume == nullptr) {
           _reportError(mCurrentTask, "unable to find volume "+volName);
           emit sig_addWarning(volName, "", tr("Invalid configuration: volume %1 not found").arg(volName), true);
           return;
        }
    }

    if (mCurrentTask->type() != TaskType::UploadFile && mCluster->uploadJobsCount() > 0) {
        mCluster->pollUploadJobs(mCurrentTask->priority()>0 ? 10 : 0, mUploadDoneCallback);
    }

    switch (mCurrentTask->type()) {
    case TaskType::ListClusterNodes: {
        emit sig_setEtaAction(EtaAction::ListClusterNodes, taskCount, "", 0, 0);
        if (!mCluster->reloadClusterNodes()) {
            _reportError(mCurrentTask, mCluster->lastError().errorMessage());
        }
    } break;
    case TaskType::ListVolumes: {
        emit sig_setEtaAction(EtaAction::ListVolumes, taskCount, "", 0, 0);
        bool failed = false;

        QString vcluster = mCluster->userInfo().vcluster();
        if (!mCluster->reloadClusterMeta()) {
            failed = true;
        }
        else {
            if (vcluster != mCluster->userInfo().vcluster())
                emit sig_gotVcluster(vcluster);
        }
        if (!mCluster->reloadVolumes()) {
            _reportError(mCurrentTask, mCluster->lastError().errorMessage());
            failed = true;
        }
        foreach (QTimer* timer, mTimers) {
            if (timer->property("ListVolumes").isValid()) {
                timer->stop();
                timer->deleteLater();
                mTimers.remove(timer);
                break;
            }
        }
        QTimer *timer = new QTimer(this);
        timer->setProperty("ListVolumes", true);
        mTimers.insert(timer);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, [timer, this]() {
            mTimers.remove(timer);
            timer->deleteLater();
            this->requestVolumeList();
        });
        timer->start(sTimeoutListVolumes*1000);
        if (!failed) {
            SxDatabase& db = SxDatabase::instance();
            QHash<QString, QString> modifiedNames;
            db.updateVolumes(mCluster->volumeList(), modifiedNames);
            bool configChanged = false;
            if (!modifiedNames.isEmpty()) {
                QHash<QString, QHash<QString, QVariant>> tmp;
                mConfig->syncConfig();
                QStringList volumes = mConfig->volumes();
                foreach (QString oldName, modifiedNames.keys()) {
                    if (!volumes.contains(oldName))
                        continue;
                    configChanged = true;
                    auto config = mConfig->volume(oldName).toHashtable();
                    QString newName = modifiedNames.value(oldName);
                    tmp.insert(newName, config);
                    mConfig->removeVolumeConfig(oldName);
                }
                foreach (QString newName, tmp.keys()) {
                    mConfig->addVolumeConfig(newName, tmp.value(newName));
                }
                if (configChanged) {
                    mConfig->syncConfig();
                    clear();
                    emit sig_volumeNameChanged();
                }
            }
        }
    } break;
    case TaskType::VolumeInitialScan: {
        foreach (QTimer *timer, mTimers) {
            if (timer->property("ListFiles").isValid()) {
                if (timer->property("ListFiles").toString() == volName) {
                    mTimers.remove(timer);
                    timer->disconnect();
                    timer->deleteLater();
                }
            }
        }

        emit sig_setEtaAction(EtaAction::VolumeInitialScan, taskCount, volName, 0, 0);
        QDateTime time = QDateTime::currentDateTime();
        if (!_reloadVolumeFiles(volume, volumeRootDir, "", true)) {
            if (mCluster->lastError().errorCode() == SxErrorCode::FilterError) {
                lockVolume(volName);
                emit sig_addWarning(volName, "", tr("Volume locked due to invalid configuration"), true);
                break;
            }
        }
        emit sig_removeWarning(volName, "");
        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);
        mTimers.insert(timer);
        connect(timer, &QTimer::timeout, [timer, this, volName]() {
            mTimers.remove(timer);
            timer->deleteLater();
            this->requestRemoteList(volName);
        });
        timer->start(qMax(sTimeoutListFiles*1000, static_cast<int>(time.msecsTo(QDateTime::currentDateTime()))*5));
    } break;
    case TaskType::ListRemoteFiles: {
        emit sig_setEtaAction(EtaAction::ListRemoteFiles, taskCount, volName, 0, 0);
        QDateTime time = QDateTime::currentDateTime();
        if (!_reloadVolumeFiles(volume, volumeRootDir, mEtags.value(volName, ""), false)) {
            if (mCluster->lastError().errorCode() == SxErrorCode::FilterError) {
                lockVolume(volName);
                emit sig_addWarning(volName, "", tr("Volume locked due to invalid configuration"), true);
                break;
            }
        }
        QTimer *timer = new QTimer();
        timer->setProperty("ListFiles", volName);
        timer->setSingleShot(true);
        mTimers.insert(timer);
        connect(timer, &QTimer::timeout, [timer, this, volName]() {
            mTimers.remove(timer);
            timer->deleteLater();
            this->requestRemoteList(volName);
        });
        timer->start(qMax(sTimeoutListFiles*1000, static_cast<int>(time.msecsTo(QDateTime::currentDateTime()))*5));
    } break;
    case TaskType::UploadFile: {
        QFileInfo info(volumeRootDir+"/"+path);
        emit sig_setEtaAction(EtaAction::UploadFile, taskCount, path.split("/").last(), info.size(), 0);
        SxFileEntry fileEntry;
        if (!mCluster->uploadFile(volume, path, volumeRootDir+"/"+path, fileEntry, mUploadDoneCallback)) {
            if (mCluster->lastError().errorCode() == SxErrorCode::AbortedByUser)
                return;
            _reportError(mCurrentTask, mCluster->lastError().errorMessage());
            if (mCluster->lastError().errorCode() == SxErrorCode::SoftError) {
                Task* newTask = new Task(TaskType::UploadFile, mCurrentTask->volume(), mCurrentTask->path(), 0, mCurrentTask->size());
                _appendRegularTask(newTask);
                return;
            }
            else if (mCluster->lastError().errorCode() == SxErrorCode::OutOfSpace) {
                qint64 size = fileEntry.size();
                UploadQueue *queue = mPendingUploads.value(volName);
                mCluster->_locateVolume(volume, 0, 0);
                queue->addTask(path, size, volume->freeSize());
            }
            else if (mCluster->lastError().errorCode() == SxErrorCode::FilterError) {
                emit sig_addWarning(volName, "", tr("Volume locked due to invalid configuration"), true);
                lockVolume(volName);
            }
            else if (mCluster->lastError().errorCode() == SxErrorCode::NotFound) {
                // file removed before upload
                emit sig_removeWarning(volName, path);
            }
            else if (mCluster->lastError().errorCode() != SxErrorCode::AbortedByUser) {
                QString message = QCoreApplication::translate("SxErrorMessage", "Upload file %1 failed: %2");
                emit sig_addWarning(volName, path,
                                    message.arg(volName+path).arg(mCluster->lastError().errorMessageTr()),
                                    false);
            }
            return;
        }
        SxDatabase::instance().addSuppression(volName, path);
        if (fileEntry.revision().isEmpty())
            SxDatabase::instance().onFileUploaded(volName, fileEntry, false);
        else {
            SxDatabase::instance().onFileUploaded(volName, fileEntry, true);
            SxDatabase::instance().removeSuppression(volName, fileEntry.path());
        }
    } break;
    case TaskType::DownloadFile: {
        qint64 size = SxDatabase::instance().getRemoteFileSize(volName, path);
        SxFileEntry fileEntry;
        QString filePath = volumeRootDir;
        if (path.startsWith("/"))
            filePath += path;
        else
            filePath += "/"+path;
        bool exists = QFileInfo::exists(filePath);
        emit sig_setEtaAction(EtaAction::DownloadFile, taskCount, path.split("/").last(), size, 0);
        if (size > 0) {
            if (!mCluster->downloadFile(volume, path, filePath, fileEntry, sDownloadConnectionsLimit)) {
                if (mCluster->lastError().errorCode() == SxErrorCode::AbortedByUser)
                    _reportError(mCurrentTask, mCluster->lastError().errorMessage());
                if (mCluster->lastError().errorCode() == SxErrorCode::FilterError) {
                    lockVolume(volName);
                    emit sig_addWarning(volName, "", tr("Volume locked due to invalid configuration"), true);
                }
                else if (mCluster->lastError().errorCode() != SxErrorCode::AbortedByUser){
                    QString message = QCoreApplication::translate("SxErrorMessage", "Download file %1 failed: %2");
                    emit sig_addWarning(volName, path,
                                        message.arg(volName+path).arg(mCluster->lastError().errorMessageTr()),
                                        false);
                }
                return;
            }
        }
        else {
            QFile file(filePath);
            QFileInfo fileInfo(filePath);
            QDir dir;
            dir.mkpath(fileInfo.absolutePath());
            if (!file.open(QIODevice::WriteOnly)) {
                QString message = QCoreApplication::translate("SxErrorMessage", "Download file %1 failed: %2");
                emit sig_addWarning(volName, path,
                                    message.arg(volName+path).arg(QCoreApplication::translate("SxErrorMessage", ("unable to open file"))),
                                    false);
                qDebug() << message.arg(volName+path).arg(QCoreApplication::translate("SxErrorMessage", ("unable to open file")));
                return;
            }
            file.close();
            fileInfo.refresh();
            fileEntry = SxFileEntry(path, 0, SxDatabase::instance().getRemoteFileRevision(volName, path), fileInfo.lastModified().toTime_t());
        }
        emit sig_removeWarning(volName, path);
        emit sig_fileSynchronised(filePath, false);
        emit sig_fileNotification(volName+path, exists ? "changed" : "added");
        logDebug(QString("downloaded file '%1' rev '%2'").arg(filePath).arg(fileEntry.revision()));
        SxDatabase::instance().onFileDownloaded(volName, fileEntry);
    } break;
    case TaskType::RemoveRemoteFile: {
        emit sig_setEtaAction(EtaAction::RemoveRemoteFile, taskCount, path.split("/").last(), 0, 0);
        QStringList toRemove = {path};
        QHash<QString, Task*> tasks;

        mMutex.lock();
        for (int i=0; i<sRemoveRemoteFilesLimit; i++) {
            if (mTaskList.isEmpty())
                break;
            Task *task = mTaskList.first();
            if (task->type() != TaskType::RemoveRemoteFile || task->volume() != volName)
                break;
            toRemove.append(task->path());
            mEtaCounters.removeTask(task);
            tasks.insert(task->path(), mTaskList.takeFirst());
            QString taskPath = task->volume()+"/"+task->path();
            mTaskByPath.remove(taskPath);
        }
        mMutex.unlock();

        auto onDelete = [volName, &tasks](const QString& path) {
            SxDatabase::instance().onRemoteFileRemoved(volName, path);
            tasks.remove(path);
        };

        if (!mCluster->deleteFiles(volume, toRemove, onDelete)) {
            _reportError(mCurrentTask, mCluster->lastError().errorMessage());
            foreach (Task* task, tasks.values()) {
                _appendRegularTask(task);
            }
            if (mCluster->lastError().errorCode() == SxErrorCode::NotFound) {
                this->clear(volName);
                Task *task = new Task(TaskType::VolumeInitialScan, volName, "", 99, 0);
                addTask(task);
            }
            return;
        }
    } break;
    case TaskType::RemoveLocalFile: {
        emit sig_setEtaAction(EtaAction::RemoveLocalFile, taskCount, path.split("/").last(), 0, 0);
        QFileInfo localFile(volumeRootDir+"/"+path);
        if (localFile.isFile()) {
            QFile file(localFile.absoluteFilePath());
            QStringList revisions;
            if (mInconsistentVolumes.contains(volName) && mCluster->checkFileConsistency(volume, path, revisions)) {
                SxDatabase::instance().updateInconsistentFile(volName, path, revisions);
                if (!revisions.isEmpty())
                    break;
            }
            if (file.remove()) {
                emit sig_fileNotification(volName+path, "removed");
                emit sig_removeWarning(volName, path);
                QFileInfo fileInfo(localFile.absoluteFilePath());
                QDir localDir(volumeRootDir);
                QDir dir = fileInfo.dir();
                while (true) {
                    if (dir == localDir)
                        break;
                    if (!dir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden).isEmpty())
                        break;
                    QString dirName = dir.dirName();
                    if (!dir.cdUp())
                        break;
                    if (!dir.rmdir(dirName))
                        break;
                }
            }
        }
        else
            emit sig_removeWarning(volName, path);
        SxDatabase::instance().onLocalFileRemoved(volName, path);
    } break;
    case TaskType::CheckFileConsistency: {
        QStringList revisions;
        if (mCluster->checkFileConsistency(volume, path, revisions))
            SxDatabase::instance().updateInconsistentFile(volName, path, revisions);
        else
            logWarning(QString("failed to check file %1%2 consistency").arg(volName).arg(path));
    } break;
    }
}

SxQueue::Task::Task(const SxQueue::TaskType &type, const QString &volume, const QString &path, const int &priority, qint64 size)
    : mId(sCounter++)
{
    //logInfo(QString("create task: %1").arg(mId));
    mType = type;
    mVolume = volume;
    mPath = path;
    mPriority = priority;
    mSize = size;
    sLivingTasks.insert(mId);
}

SxQueue::Task::~Task()
{
    //logInfo(QString("delete task: %1").arg(mId));
    sLivingTasks.remove(mId);
}

SxQueue::TaskType SxQueue::Task::type() const
{
    return mType;
}

QString SxQueue::Task::volume() const
{
    return mVolume;
}

QString SxQueue::Task::path() const
{
    return mPath;
}

qint64 SxQueue::Task::size() const
{
    return mSize;
}

int SxQueue::Task::priority() const
{
    return mPriority;
}

quint64 SxQueue::Task::id() const
{
    return  mId;
}

bool SxQueue::Task::equal(const SxQueue::Task &other) const
{
    if (mPriority != other.mPriority)
        return false;
    if (mType != other.mType)
        return false;
    if (mVolume != other.mVolume)
        return false;
    if (mPath != other.mPath)
        return false;
    return true;
}

QString SxQueue::Task::toString() const
{
    QString result = "Task {type: ";
    switch (mType) {
    case TaskType::ListClusterNodes:
        result += "ListClusterNodes";
        break;
    case TaskType::ListVolumes:
        result += "ListVolumes";
        break;
    case TaskType::VolumeInitialScan:
        result += "VolumeInitialScan";
        break;
    case TaskType::ListRemoteFiles:
        result += "ListRemoteFiles";
        break;
    case TaskType::UploadFile:
        result += "UploadFile";
        break;
    case TaskType::DownloadFile:
        result += "DownloadFile";
        break;
    case TaskType::RemoveRemoteFile:
        result += "RemoveRemoteFile";
        break;
    case TaskType::RemoveLocalFile:
        result += "RemoveLocalFile";
        break;
    case TaskType::CheckFileConsistency:
        result += "CheckFileConsistency";
        break;
    }
    result += QString(", volume: \"%1\", path: \"%2\"}").arg(mVolume, mPath);
    return result;
}

SxQueue::EtaCounters::EtaCounters() {
    uploadCount = 0;
    downloadCount = 0;
    removeCount = 0;
    uploadSize = 0;
    downloadSize = 0;
}

void SxQueue::EtaCounters::addTask(SxQueue::Task *task)
{
    if (task->type() == TaskType::UploadFile) {
        uploadCount++;
        uploadSize+=task->size();
    }
    else if (task->type() == TaskType::DownloadFile) {
        downloadCount++;
        downloadSize+=task->size();
    }
    else if (task->type() == TaskType::RemoveLocalFile || task->type() == TaskType::RemoveRemoteFile) {
        removeCount++;
    }
}

void SxQueue::EtaCounters::removeTask(SxQueue::Task *task)
{
    if (task->type() == TaskType::UploadFile) {
        uploadCount--;
        uploadSize-=task->size();
    }
    else if (task->type() == TaskType::DownloadFile) {
        downloadCount--;
        downloadSize-=task->size();
    }
    else if (task->type() == TaskType::RemoveLocalFile || task->type() == TaskType::RemoveRemoteFile) {
        removeCount--;
    }
}

void SxQueue::EtaCounters::clear()
{
    uploadCount = 0;
    downloadCount = 0;
    removeCount = 0;
    uploadSize = 0;
    downloadSize = 0;
}
