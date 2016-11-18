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

#include "sxcontroller.h"
#include "sxlog.h"

SxController::SxController(SxConfig *config, std::function<bool(QSslCertificate &, bool)> checkCertCallback, std::function<bool(QString)> askGuiCallback, QObject *parrent)
    : QObject(parrent)
{
    mConfig = config;
    mCheckSslCallback = checkCertCallback;
    mAskGuiCallback = askGuiCallback;
    mStarted = false;
    timer = nullptr;
    mQueue = nullptr;
    mFilesystem = nullptr;
    mFilesystemScannerThread = nullptr;
    mQueueThread = nullptr;
}

SxController::~SxController()
{
    _destroyCluster();
}

SxStatus SxController::status() const
{
    return mState.status();
}

const SxState &SxController::sxState() const
{
    return mState;
}

int SxController::warningsCount() const
{
    return mState.warningsCount();
}

bool SxController::pause()
{
    if (!mQueue)
        return false;
    mState.clearWarnings();
    mQueue->setPaused(true);
    mQueue->clear();
    if (!mQueue->abortCurrentTask())
        mState.setStatus(SxStatus::paused);
    return true;
}

bool SxController::resume()
{
    if (!mQueue)
        return false;
    mQueue->setPaused(false);
    mQueue->requestVolumeList();
    mQueue->requestInitialScan();
    return true;
}

void SxController::reinitCluster()
{
    _destroyCluster();
    if (timer) {
        timer->stop();
        timer->deleteLater();
        timer = nullptr;
    }
    _initCluster();
}

void SxController::unlockVolume(const QString &volume)
{
    emit sig_unlockVolume(volume);
}

void SxController::_initCluster()
{
    mState.clearWarnings();
    mQueueThread = new QThread();
    mQueueThread->setProperty("name", "SYNC_QUEUE");
    mQueueThread->start();
    mFilesystemScannerThread = new QThread();
    mFilesystemScannerThread->setProperty("name", "FS_WATCHER");
    mFilesystemScannerThread->start();
#ifdef Q_OS_MAC
    mFilesystemScannerThread->setPriority(QThread::LowestPriority);
#endif
    if (mConfig->isValid()) {
        mQueue = new SxQueue(mConfig, mCheckSslCallback, mAskGuiCallback);
        mQueue->moveToThread(mQueueThread);
        mFilesystem = new SxFilesystem(mConfig);
        mFilesystem->moveToThread(mFilesystemScannerThread);
        connect(mFilesystem, &SxFilesystem::sig_fileModified, mQueue, &SxQueue::localFileModified);
        connect(mFilesystem, &SxFilesystem::sig_cancelUploadTask, mQueue, &SxQueue::cancelUploadTask);
        connect(mFilesystem, &SxFilesystem::sig_cancelUploadTask, [this](const QString &volume, const QString &path) {
            mState.removeWarning(volume, path);
        });
        connect(mQueue, &SxQueue::sig_satusChanged,         this, &SxController::onSatusChanged, Qt::QueuedConnection);
        connect(mQueue, &SxQueue::sig_fileSynchronised,     this, &SxController::sig_fileSynchronised);
        connect(mQueue, &SxQueue::sig_setEtaAction,         this, &SxController::sig_setEtaAction);
        connect(mQueue, &SxQueue::sig_setEtaCounters,       this, &SxController::sig_setEtaCounters);
        connect(mQueue, &SxQueue::sig_setProgress,          this, &SxController::sig_setProgress);
        connect(mQueue, &SxQueue::sig_clusterInitialized,   this, &SxController::sig_clusterInitialized);
        connect(mQueue, &SxQueue::sig_fileNotification,     this, &SxController::sig_fileNotification);
        connect(mQueue, &SxQueue::sig_lockVolume,           this, &SxController::sig_lockVolume);
        connect(mQueue, &SxQueue::sig_initializationFailed, this, &SxController::onClusterInitializationFailed);
        connect(mQueue, &SxQueue::sig_gotVcluster,          this, &SxController::sig_gotVcluster);
        connect(mQueue, &SxQueue::sig_volumeNameChanged,    this, &SxController::onVolumeNameChanged);
        connect(mQueue, &SxQueue::sig_volumeNameChanged,    this, &SxController::sig_volumeNameChanged);
        connect(this,   &SxController::sig_unlockVolume,    mQueue, &SxQueue::unlockVolume);
        connect(this,   &SxController::sig_requestVolumeList, mQueue, &SxQueue::requestVolumeList);
        connect(mQueue, &SxQueue::sig_addWarning, &mState, &SxState::addWarning);
        connect(mQueue, &SxQueue::sig_removeWarning, &mState, &SxState::removeWarning);
        mQueue->requestInitialScan();
    }
    else {
        mQueue = nullptr;
        mFilesystem = nullptr;
        mState.addWarning("", "", "Invalid configuration", true);
    }
}

void SxController::_destroyCluster()
{
    pause();
    if (mFilesystemScannerThread) {
        mFilesystemScannerThread->quit();
        if (mFilesystemScannerThread->wait())
            delete mFilesystemScannerThread;
    }
    if (mQueueThread) {
        mQueueThread->quit();
        if (mQueueThread->wait())
            delete mQueueThread;
    }
    if (mQueue != nullptr)
        delete mQueue;
    if (mFilesystem != nullptr)
        delete mFilesystem;
}

void SxController::onSatusChanged(SxStatus status)
{
    mState.setStatus(status);
}

void SxController::restartFilesystem()
{
    if (mFilesystem != nullptr) {
        mFilesystem->disconnect();
        mFilesystem->deleteLater();
        mFilesystemScannerThread->quit();
        mFilesystemScannerThread->wait();
        mFilesystemScannerThread->deleteLater();

        mFilesystemScannerThread = new QThread();
        mFilesystemScannerThread->setProperty("name", "FS_WATCHER");
        mFilesystemScannerThread->start();
#ifdef Q_OS_MAC
        mFilesystemScannerThread->setPriority(QThread::LowestPriority);
#endif

        mFilesystem = new SxFilesystem(mConfig);
        mFilesystem->moveToThread(mFilesystemScannerThread);
        connect(mFilesystem, &SxFilesystem::sig_fileModified, mQueue, &SxQueue::localFileModified);
        connect(mFilesystem, &SxFilesystem::sig_cancelUploadTask, mQueue, &SxQueue::cancelUploadTask);
        connect(mFilesystem, &SxFilesystem::sig_cancelUploadTask, [this](const QString &volume, const QString &path) {
            mState.removeWarning(volume, path);
        });
    }
}

void SxController::startCluster()
{
    if (mStarted)
        return;
    mStarted = true;
    _initCluster();
}

void SxController::onClusterInitializationFailed()
{
    if (timer != nullptr)
        return;
    timer = new QTimer();
    timer->setInterval(initializationRetryTime*1000);
    timer->setSingleShot(true);
    logWarning(QString("cluster initialization failed, retrying in %1 seconds").arg(initializationRetryTime));
    connect(timer, &QTimer::timeout, [this]() {
        delete timer;
        timer = nullptr;
        reinitCluster();
    });
    timer->start();
}

void SxController::clearWarnings()
{
    mState.clearWarnings();
}

void SxController::onVolumeNameChanged()
{
    restartFilesystem();
    mQueue->requestInitialScan();
}
