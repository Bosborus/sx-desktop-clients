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

#include "scoutmodel.h"
#include "sxlog.h"
#include <QDir>
#include <QFileInfo>
#include <QSslCertificate>
#include <sxfilter.h>
#include <memory>
#include <QStandardPaths>

QStringList listDir(const QString &path) {
    QStringList result;
    QStringList dirList = {path};
    while (!dirList.isEmpty()) {
        QDir dir(dirList.takeFirst());
        if (!dir.exists())
            continue;
        foreach (auto d, dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot)) {
            dirList.append(dir.absoluteFilePath(d));
        }
        auto files = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
        if (files.isEmpty()) {
            result.append(dir.absoluteFilePath(".sxnewdir"));
            continue;
        }
        foreach (auto file, files) {
            result.append(dir.absoluteFilePath(file));
        }
    }
    return result;
}

ScoutModel::ScoutModel(ClusterConfig* config, ScoutQueue *queue,  QObject *parent) : QAbstractItemModel(parent)
{
    mCheckCertCallback = [this](QSslCertificate& cert, bool secondaryCert) -> bool {
        QCryptographicHash sha1(QCryptographicHash::Sha1);
        sha1.addData(cert.toDer());
        QByteArray fprint = sha1.result();
        QByteArray clusterFp = secondaryCert ? mClusterConfig->secondaryClusterCertFp() : mClusterConfig->clusterCertFp();

        if (fprint == clusterFp)
            return true;
        return false;
    };
    mReloading = false;

    mQueue = queue;
    mClusterConfig = config;
    mDatabase = ScoutDatabase::instance();

    mCurrentVolume = mCurrentPath = "";
    mFilesCount = 0;
    mFilesColumnCount = 1;

    mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), mCheckCertCallback, mLastError);
    if (mCluster == nullptr)
        logWarning("ScoutModel: Unable to initialize cluster: "+mLastError);
    else
        reloadMeta();
    connect(mQueue, &ScoutQueue::fileUploaded,
            this, &ScoutModel::queueFileUploaded, Qt::QueuedConnection);
    connect(&mRefreshTimer, &QTimer::timeout, this, &ScoutModel::reloadVolumes);
    mRefreshTimer.setInterval(10000);
    //mRefreshTimer.start();
}

ScoutModel::~ScoutModel()
{
    delete mCluster;
    foreach (auto entry, mFileList) {
        delete entry;
    }
}

SxCluster *ScoutModel::cluster() const
{
    return mCluster;
}

QModelIndex ScoutModel::index(int row, int column, const QModelIndex &parent) const
{
    /*
    if (parent == mFilesIndex)
        qWarning() << QDateTime::currentDateTime() << "ScoutModel::index" << row << column;
    */
    if (!parent.isValid()) {
        if (column == 0) {
            if (row == 0)
                return mVolumesIndex;
            if (row == 1)
                return mFilesIndex;
        }
        return QModelIndex();
    }
    if (parent == mFilesIndex)
        return createIndex(row, column, const_cast<void*>(reinterpret_cast<const void*>(&mFilesIndex)));
    if (parent == mVolumesIndex)
        return createIndex(row, column, const_cast<void*>(reinterpret_cast<const void*>(&mVolumesIndex)));
    return QModelIndex();
}

QModelIndex ScoutModel::parent(const QModelIndex &child) const
{
    if (child.internalPointer() == reinterpret_cast<const void*>(&mFilesIndex))
        return mFilesIndex;
    if (child.internalPointer() == reinterpret_cast<const void*>(&mVolumesIndex))
        return mVolumesIndex;
    return QModelIndex();
}

int ScoutModel::rowCount(const QModelIndex &parent) const
{
    /*
    if (parent == mFilesIndex)
        qWarning() << QDateTime::currentDateTime() << "ScoutModel::rowCount";
    */
    if (mResizeInfo.contains(parent))
        return mResizeInfo.value(parent).first;

    if (parent == mFilesIndex) {
        int count = (mCurrentVolume.isEmpty()) ? mVolumes.count() : mFilesCount;
        if (count <= 0)
            return count;
        int rows = count / mFilesColumnCount + ((count%mFilesColumnCount) ? 1 : 0 );
        return rows;
    }
    if (parent == mVolumesIndex)
        return mVolumes.count();
    return 4;
}

int ScoutModel::columnCount(const QModelIndex &parent) const
{
    /*
    if (parent == mFilesIndex)
        qWarning() << QDateTime::currentDateTime() << "ScoutModel::columnCount";
    */
    if (mResizeInfo.contains(parent))
        return mResizeInfo.value(parent).second;
    if (parent == mFilesIndex)
        return mFilesColumnCount;
    return 1;
}

static const QList<QPair<QString, QStringList>> sKnownExtensions = {
    {"archive", {"zip", "rar", "arj", "gz", "tar", "tgz", "7z", "xz"}},
    {"code", {"xml", "html", "php", "js"}},
    {"executable", {"exe"}},
    {"image", {"png", "jpg", "jpeg", "bmp", "gif", "tiff"}},
    {"application", {"dmg", "msi"}},
    {"music", {"mp3", "wma", "wave", "ogg"}},
    {"pdf", {"pdf"}},
    {"presentation", {"ppt", "pptx", "odp"}},
    {"spreadsheet", {"xls", "xlsx", "ods"}},
    {"text", {"doc", "docx", "odt", "rtf", "txt"}},
    {"video", {"avi", "mov", "mp4", "wmv", "mkv", "mpg", "mpeg", "mp2"}}
};

QHash<QString, QString> reverseMapping(const QList<QPair<QString, QStringList>> &list) {
    QHash<QString, QString> result;
    foreach (auto pair, list) {
        foreach (auto ext, pair.second) {
            result.insert(ext, "File/"+pair.first);
        }
    }
    return result;
}

QString ScoutModel::getMimeType(const QString &extension) {
    static const auto extensionMap = reverseMapping(sKnownExtensions);
    return extensionMap.value(extension.toLower(), "File");
}

void ScoutModel::cancelClusterTask()
{
    mCluster->abort();
}

QVariant ScoutModel::data(const QModelIndex &index, int role) const
{
    /*
    if (index.parent() == mFilesIndex)
        qWarning() << QDateTime::currentDateTime() << "ScoutModel::data" << role << index;
    */
    if (mReloading)
        return QVariant();

    bool isVolume = index.parent() == mVolumesIndex;
    int itemIndex;
    if (index.parent() == mFilesIndex) {
        if (index.column() < 0 || index.column() >= mFilesColumnCount || index.row() < 0)
            return QVariant();
        itemIndex = index.row()*mFilesColumnCount+index.column();
        if (mCurrentVolume.isEmpty())
            isVolume = true;
        else {
            if (itemIndex >= mFilesCount || itemIndex >= mFileList.count())
                return QVariant();
            if (role == FullPathRole) {
                return mFileList.at(itemIndex)->path();
            }
            if (role == NameRole) {
                QString name = mFileList.at(itemIndex)->path().mid(mCurrentPath.length());
                if (name.endsWith("/"))
                    return name.mid(0, name.length()-1);
                return name;
            }
            if (role == SizeRole) {
                return mFileList.at(itemIndex)->size();
            }
            else if (role == MimeTypeRole) {
                if (mFileList.at(itemIndex)->path().endsWith("/"))
                    return "Directory";
                else {
                    int index = mFileList.at(itemIndex)->path().lastIndexOf('.');
                    if (index <= 0)
                        return "File";
                    QString ext = mFileList.at(itemIndex)->path().mid(index+1);
                    return getMimeType(ext);
                }
            }
        }
    }
    else if (index.column() != 0)
        return QVariant();
    else
        itemIndex = index.row();
    if (isVolume) {
        if (itemIndex < 0 || itemIndex >= mVolumes.count())
            return QVariant();
        if (role == NameRole)
            return mVolumes.value(itemIndex);
        if (role == MimeTypeRole) {
            QString volName = mVolumes.value(itemIndex);
            SxVolume* vol = mCluster->getSxVolume(volName);
            if (vol == nullptr || !SxFilter::isFilterSupported(vol))
                return "Volume/Unsupported";
            std::unique_ptr<SxFilter> filter(SxFilter::getActiveFilter(vol));
            if (filter != nullptr && filter->isAes256()) {
                if (mUnlockedVolumes.contains(volName))
                    return "Volume/Unlocked";
                else
                    return "Volume/Locked";
            }
            return "Volume";
        }
        if (role == SizeRole || role == SizeUsedRole) {
            QString volName = mVolumes.value(itemIndex);
            SxVolume* vol = mCluster->getSxVolume(volName);
            if (vol != nullptr) {
                if (role == SizeRole)
                    return vol->size();
                else
                    return vol->usedSize();
            }
        }
    }
    return QVariant();
}

QString ScoutModel::currentVolume() const
{
    return mCurrentVolume;
}

QString ScoutModel::lastError() const
{
    return mLastError;
}

int ScoutModel::currentVolumeIndex() const
{
    for (int i=0; i<mVolumes.count(); i++) {
        if (mVolumes.at(i)==mCurrentVolume)
            return i;
    }
    return -1;
}

QString ScoutModel::currentPath() const
{
    return mCurrentPath;
}

void ScoutModel::removeFiles(const QString &volume, const QStringList &paths)
{
    auto sxVolume = mCluster->getSxVolume(volume);
    if (sxVolume == nullptr)
        return;
    QStringList toRemove;
    foreach (QString path, paths) {
        if (!path.endsWith("/")) {
            toRemove.append(path);
        }
        else {
            QList<SxFileEntry*> list;
            QString etag;
            if (!mCluster->_listFiles(sxVolume, path, true, list, etag)) {
                continue;
            }
            foreach (auto entry, list) {
                toRemove.append(entry->path());
                delete entry;
            }
        }
    }
    int removed = 0;
    int toRemoveCount = toRemove.count();

    while (!toRemove.isEmpty()) {
        auto notify = [this, &toRemove, &removed, &toRemoveCount](const QString&) {
            ++removed;
            if (!toRemove.isEmpty())
                emit signalProgress(toRemove.first(), removed, toRemoveCount);
        };
        emit signalProgress(toRemove.first(), removed, toRemoveCount);
        if (!mCluster->deleteFiles(sxVolume, toRemove, notify)) {
            if (mCluster->lastError().errorCode() == SxErrorCode::NotFound) {
                qDebug() << "file not found";
                continue;
            }
            emit sigError(mCluster->lastError().errorMessage());
            return;
        }
    }
}

void ScoutModel::rename(const QString &volume, const QString &source, const QString &destination)
{
    auto sxVolume = mCluster->getSxVolume(volume);
    if (sxVolume == nullptr)
        return;
    if (!mCluster->rename(sxVolume, source, destination))
        emit sigError(mCluster->lastError().errorMessage());
}

void ScoutModel::copyFiles(const QString &srcVolume, const QString &srcRoot, const QStringList &files, const QString &dstVolume, const QString &destination)
{
    copyOrCopyFiles(srcVolume, srcRoot, files, dstVolume, destination, true);
}

void ScoutModel::moveFiles(const QString &srcVolume, const QString &srcRoot, const QStringList &files, const QString &dstVolume, const QString &destination)
{
    copyOrCopyFiles(srcVolume, srcRoot, files, dstVolume, destination, false);
}

void ScoutModel::uploadFiles(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir)
{
    qint64 size = 0;
    auto callback = [this, &size](QString file, qint64 done, qint64) {
        int max = 1000;
        int progress = 0;
        if (size == 0)
            max = 0;
        else
            progress = static_cast<int>((max*done)/size);
        QMetaObject::invokeMethod(this, "signalProgress",
                                  Q_ARG(QString, file),
                                  Q_ARG(int, progress),
                                  Q_ARG(int, max));
    };
    QStringList paths;
    foreach (QString file, files){
        QString localPath = rootDir+"/"+file;
        if (localPath.endsWith("/")) {
            auto files = listDir(localPath);
            foreach (auto file, files) {
                paths.append(file);
                QFileInfo fileInfo(file);
                size += fileInfo.size();
            }
        }
        else {
            paths.append(localPath);
            QFileInfo fileInfo(localPath);
            size += fileInfo.size();
        }
    }

    QThread thread;
    thread.start();
    ScoutModelHelperThread uploadThread(mCluster->auth(), mCluster->uuid(), callback);
    uploadThread.moveToThread(&thread);
    connect(this, &ScoutModel::abort, &uploadThread, &ScoutModelHelperThread::abortTask, Qt::DirectConnection);
    uploadThread.uploadFiles(rootDir, paths, dstVolume, dstDir);
    thread.quit();
    thread.wait();
}

void ScoutModel::downloadFiles(const QString &volume, const QString &rootDir, const QStringList &files, const QString &localDir)
{
    auto callback = [this](QString file, qint64 done, qint64 size) {
        int max = 1000;
        int progress = 0;
        if (size == 0)
            max = 0;
        else
            progress = static_cast<int>((max*done)/size);
        QMetaObject::invokeMethod(this, "signalProgress",
                                  Q_ARG(QString, file),
                                  Q_ARG(int, progress),
                                  Q_ARG(int, max));
    };

    /*
    QStringList paths;
    foreach (QString file, files){
        QString localPath = rootDir+"/"+file;
        if (localPath.endsWith("/")) {
            auto files = listDir(localPath);
            foreach (auto file, files) {
                paths.append(file);
                QFileInfo fileInfo(file);
                size += fileInfo.size();
            }
        }
        else {
            paths.append(localPath);
            QFileInfo fileInfo(localPath);
            size += fileInfo.size();
        }
    }*/

    QThread thread;
    thread.start();
    ScoutModelHelperThread downloadThread(mCluster->auth(), mCluster->uuid(), callback);
    downloadThread.moveToThread(&thread);
    connect(this, &ScoutModel::abort, &downloadThread, &ScoutModelHelperThread::abortTask, Qt::DirectConnection);
    downloadThread.downloadFiles(volume, rootDir, files, localDir);
    thread.quit();
    thread.wait();
}

void ScoutModel::mkdir(const QString &volume, const QString &path)
{
    auto sxVolume = mCluster->getSxVolume(volume);
    if (sxVolume == nullptr)
        return;
    QString sxnewdir = path;
    if (!path.endsWith('/'))
        sxnewdir.append('/');
    sxnewdir.append(".sxnewdir");
    if (!mCluster->createEmptyFile(sxVolume, sxnewdir))
        emit sigError(mCluster->lastError().errorMessage());
}

void ScoutModel::move(const QString &volume, const QString &path)
{
    if (mCurrentVolume != volume || mCurrentPath != path) {
        mNextStack.clear();
        mPrevStack.push({mCurrentVolume, mCurrentPath});
    }
    setCurrentPath(volume, path, true);
}

void ScoutModel::movePrev()
{
    if (mPrevStack.isEmpty())
        return;
    mNextStack.push({mCurrentVolume, mCurrentPath});
    QPair<QString, QString> prev = mPrevStack.pop();
    setCurrentPath(prev.first, prev.second, true);
}

void ScoutModel::moveNext()
{
    if (mNextStack.isEmpty())
        return;
    mPrevStack.push({mCurrentVolume, mCurrentPath});
    QPair<QString, QString> next = mNextStack.pop();
    setCurrentPath(next.first, next.second, true);
}

void ScoutModel::moveUp()
{
    if (mCurrentVolume.isEmpty() || mCurrentPath.isEmpty())
        return;
    mNextStack.clear();
    mPrevStack.push({mCurrentVolume, mCurrentPath});
    if (mCurrentPath == "/") {
        setCurrentPath("","", true);
    }
    else {
        int index = mCurrentPath.lastIndexOf("/", mCurrentPath.length()-2);
        QString upPath = mCurrentPath.mid(0, index+1);
        setCurrentPath(mCurrentVolume, upPath, true);
    }
}

void ScoutModel::refresh(bool blockView)
{
    setCurrentPath(mCurrentVolume, mCurrentPath, blockView);
}

bool ScoutModel::isWorking() const
{
    if (mQueue == nullptr)
        return false;
    return mQueue->isWorking();
}

bool ScoutModel::canMovePrev() const
{
    return !mPrevStack.isEmpty();
}

bool ScoutModel::canMoveNext() const
{
    return !mNextStack.isEmpty();
}

bool ScoutModel::canMoveUp() const
{
    return (!mCurrentVolume.isEmpty() && !mCurrentPath.isEmpty());
}

void ScoutModel::reloadClusterConfig(ScoutQueue *queue)
{
    mReloading = true;
    if (mCluster == nullptr) {
        QString errorString;
        mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), mCheckCertCallback, errorString);
        if (mCluster == nullptr) {
            logWarning("ScoutModel: Unable to initialize cluster: "+errorString);
        }
    }
    else {
        delete mCluster;
        QString errorString;
        mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), mCheckCertCallback, errorString);
        if (mCluster == nullptr) {
            logWarning("ScoutModel: Unable to initialize cluster: "+errorString);
        }
    }
    reloadMeta();
    mQueue = queue;
    connect(mQueue, &ScoutQueue::fileUploaded, this, &ScoutModel::queueFileUploaded, Qt::QueuedConnection);
    mPrevStack.clear();
    mNextStack.clear();
    setCurrentPath("","", true);
    mReloading = false;
    emit configReloaded();
}

void ScoutModel::setCurrentPath(const QString &volume, const QString &path, bool blockView)
{
    static bool sWorking = false;
    if (sWorking)
        return;
    sWorking = true;
    int count = 0;

    if (mCluster == nullptr) {
        mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), mCheckCertCallback, mLastError);
        if (mCluster == nullptr)
            goto label0;
        emit clusterInitialized();
    }

    if (blockView)
        emit setViewEnabled(false);
    if (volume.isEmpty()) {
        reloadVolumes();
        count = mVolumes.count();
    }
    else {
        auto sxVolume = mCluster->getSxVolume(volume);
        if (sxVolume == nullptr)
            goto label0;

        if (mUnlockedVolumes.contains(volume)) {
            QString etag = mDatabase->getEtag(volume, "/");
            if (mCluster->_listFiles(sxVolume, "/", true, mFileList, etag)) {
                mDatabase->setFiles(volume, true, "/", etag, mFileList);
            }
            else if (mCluster->lastError().errorCode() != SxErrorCode::NotChanged)
                goto label0;
            mDatabase->getFiles(volume, true, path, mFileList);
        }
        else {
            QString etag = mDatabase->getEtag(volume, path);
            if (!mCluster->_listFiles(sxVolume, path, false, mFileList, etag)) {
                if (mCluster->lastError().errorCode() != SxErrorCode::NotChanged)
                    goto label0;
                mDatabase->getFiles(volume, false, path, mFileList);
            }
            else {
                mDatabase->setFiles(volume, false, path, etag, mFileList);
            }
        }
        count = mFileList.count();
    }

    label0:
    if (mCluster != nullptr) {
        auto errorCode = mCluster->lastError().errorCode();
        if (errorCode == SxErrorCode::NoError || errorCode == SxErrorCode::NotChanged)
            mLastError.clear();
        else
            mLastError = mCluster->lastError().errorMessage();
    }
    if (!mLastError.isEmpty()) {
        emit sigError(mLastError);
    }
    int newColumnCount = mFilesColumnCount;
    int newRowCount = count / mFilesColumnCount + ((count%mFilesColumnCount) ? 1 : 0 );
    resizeModel(mFilesIndex, newRowCount, newColumnCount);
    mCurrentVolume = volume;
    mCurrentPath = path;
    mFilesCount = count;

    if (blockView)
        emit setViewEnabled(true);
    if (count > 0) {
        emit dataChanged(index(0,0,mFilesIndex), index(newRowCount-1, mFilesColumnCount-1, mFilesIndex), {NameRole, FullPathRole, SizeRole, SizeUsedRole, MimeTypeRole});
    }
    sWorking = false;
}

void ScoutModel::appendTask(ScoutTask *task)
{
    if (mQueue == nullptr)
        delete task;
    else
        mQueue->appendTask(task);
}

void ScoutModel::copyOrCopyFiles(const QString &srcVolume, const QString &srcRoot, const QStringList &files, const QString &dstVolume, const QString &destination, bool copy)
{
    auto sxVolSrc = mCluster->getSxVolume(srcVolume);
    auto sxVolDst = mCluster->getSxVolume(dstVolume);
    if (sxVolSrc == nullptr || sxVolDst == nullptr)
        return;

    auto srcFilter = SxFilter::getActiveFilter(sxVolSrc);
    auto dstFilter = SxFilter::getActiveFilter(sxVolDst);
    if (srcFilter != nullptr || dstFilter != nullptr) {
        if (srcFilter != nullptr)
            delete srcFilter;
        if (dstFilter != nullptr)
            delete dstFilter;
        if (sxVolSrc != sxVolDst)
            return;
    }

    QStringList fileList;
    int len = srcRoot.length();
    foreach (auto file, files) {
        if (file.endsWith("/")) {
            QList<SxFileEntry*> list;
            QString etag;
            if (!mCluster->_listFiles(sxVolSrc, file, true, list, etag)) {
                qDebug() << "copyFiles failed";
                return;
            }
            foreach (auto entry, list) {
                fileList.append(entry->path().mid(len));
                delete entry;
            }
        }
        else
            fileList.append(file.mid(len));
    }
    auto callback = [this](int done, int size) {
        emit this->signalProgress("", done, size);
    };
    if (copy) {
        if (!mCluster->copyFiles(sxVolSrc, srcRoot, fileList, sxVolDst, destination, callback)) {
            if (mCluster->lastError().errorCode() != SxErrorCode::AbortedByUser)
                emit sigError(mCluster->lastError().errorMessage());
            return;
        }
    }
    else {
        if (!mCluster->moveFiles(sxVolSrc, srcRoot, fileList, sxVolDst, destination, callback)) {
            if (mCluster->lastError().errorCode() != SxErrorCode::AbortedByUser)
                emit sigError(mCluster->lastError().errorMessage());
            return;
        }
    }
}

/*
void ScoutModel::setClusterConfig(ClusterConfig *clusterConfig)
{
    auto checkSsl = [](QSslCertificate&, bool) -> bool {
        return true;
    };
    mClusterConfig = clusterConfig;
    if (mCluster == nullptr) {
        QString errorString;
        mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), checkSsl, errorString);
        if (mCluster == nullptr) {
            logWarning("ScoutModel: Unable to initialize cluster: "+errorString);
        }
    }
    else {
        delete mCluster;
        QString errorString;
        mCluster = SxCluster::initializeCluster(mClusterConfig->sxAuth(), mClusterConfig->uuid(), checkSsl, errorString);
        if (mCluster == nullptr) {
            logWarning("ScoutModel: Unable to initialize cluster: "+errorString);
        }
    }
    reloadMeta();
    if (mQueue == nullptr) {
        mQueue = new ScoutQueue(mClusterConfig);
    }
    else {
        mQueue->disconnect();
        mQueue->deleteLater();
        mQueue = new ScoutQueue(mClusterConfig);
    }
    connect(mQueue, &ScoutQueue::rowAboutToBeInserted,
            this, &ScoutModel::queueRowAboutToBeInserted, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::rowInserted,
            this, &ScoutModel::queueRowInserted, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::rowToBeRemoved,
            this, &ScoutModel::queueRowToBeRemoved, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::rowRemoved,
            this, &ScoutModel::queueRowRemoved, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::currentTaskChanged,
            this, &ScoutModel::queueCurrentTaskChanged, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::currentTaskNameChanged,
            this, &ScoutModel::queueCurrentTaskNameChanged, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::currentTaskProgressChanged,
            this, &ScoutModel::queueCurrentTaskProgressChanged, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::finished,
            this, &ScoutModel::tasksQueueFinished, Qt::QueuedConnection);
    connect(mQueue, &ScoutQueue::fileUploaded,
            this, &ScoutModel::queueFileUploaded, Qt::QueuedConnection);
    mQueue->moveToThread(mQueueThread);
    //TODO allow to reinitialize cluster
}
*/

bool ScoutModel::reloadVolumes()
{
    if (mCluster == nullptr)
        return false;
    if(!mCluster->reloadVolumes())
        return false;

    QStringList newList;
    foreach (auto volume, mCluster->volumeList()) {
        newList.append(volume->name());
    }

    if (mVolumes != newList) {
        resizeModel(mVolumesIndex, newList.count(), 1);
        if (mCurrentVolume.isEmpty()) {
            int newFilesRowCount = newList.count() / mFilesColumnCount + ((newList.count() % mFilesColumnCount == 0) ? 0 : 1);
            resizeModel(mFilesIndex, newFilesRowCount, mFilesColumnCount);
        }
        mVolumes = newList;
    }

    mUnlockedVolumes.clear();
    foreach (auto volume, mVolumes) {
        auto sxVolume = mCluster->getSxVolume(volume);
        if (SxFilter::testFilterConfig(sxVolume))
            mUnlockedVolumes.insert(volume);
    }
    emit dataChanged(index(0,0,mVolumesIndex), index(mVolumes.count()-1, 0, mVolumesIndex), {MimeTypeRole, NameRole, SizeRole, SizeUsedRole});
    return true;
}

bool ScoutModel::reloadMeta()
{
    if (mCluster == nullptr)
        return false;
    return mCluster->reloadClusterMeta();
}

void ScoutModel::requestDownload(const QString &volume, const QString &remoteDir, const QStringList &files, const QString &localFile)
{
    if (mCluster == nullptr)
        return;
    SxVolume *vol = mCluster->getSxVolume(volume);
    if (vol == nullptr) {
        qDebug() << "ScoutModel::requestDownload failed" << __LINE__;
        return;
    }
    QString etag;

    ScoutTask *task = new ScoutTask(false, files.join(", "), volume, localFile, remoteDir);
    qint64 size = 0;
    foreach (auto file, files) {
        if (file.endsWith("/")) {
            QList<SxFileEntry*> list;
            if (!mCluster->_listFiles(vol, remoteDir+file, true, list, etag)) {
                qDebug() << "ScoutModel::requestDownload failed" << __LINE__;
                delete task;
                return;
            }
            foreach (auto entry, list) {
                task->files.append({entry->path(), entry->size()});
                size += entry->size();
                delete entry;
            }
            list.clear();
        }
        else {
            QList<SxFileEntry*> list;
            if (!mCluster->_listFiles(vol, remoteDir+file, false, list, etag)) {
                qDebug() << "ScoutModel::requestDownload failed" << __LINE__;
                delete task;
                return;
            }
            SxFileEntry* fileEntry = nullptr;
            if (list.length() == 1) {
                fileEntry = list.takeFirst();
            }
            else if (list.length() == 2) {
                if (list.at(0)->path().endsWith("/"))
                    fileEntry = list.takeLast();
                else
                    fileEntry = list.takeFirst();
            }
            foreach (auto entry, list) {
                delete entry;
            }
            list.clear();
            if (fileEntry == nullptr) {
                qDebug() << "ScoutModel::requestDownload failed" << __LINE__;
                delete task;
                return;
            }
            task->files.append({fileEntry->path(), fileEntry->size()});
            size += fileEntry->size();
            delete fileEntry;
        }
    }
    task->size = size;

    if (task->files.isEmpty())
        delete task;
    else
        appendTask(task);
}

void ScoutModel::requestDownload(const QString &volume, const QString &remoteFile, const QString &rev, qint64 size, const QString &localFile)
{
    if (remoteFile.endsWith('/') || remoteFile.isEmpty() || rev.isEmpty())
        return;
    if (localFile.endsWith('/') || localFile.isEmpty())
        return;
    if (mCluster == nullptr)
        return;
    SxVolume *vol = mCluster->getSxVolume(volume);
    if (vol == nullptr) {
        qDebug() << "ScoutModel::requestDownload failed" << __LINE__;
        return;
    }

    ScoutTask *task = new ScoutTask(false, remoteFile.split("/").last(), volume, localFile, remoteFile);
    task->rev = rev;
    task->size = size;
    appendTask(task);
}

void ScoutModel::requestUpload(const QString &volume, const QString &localDir, const QStringList &list, const QString &remoteFile, bool openLocalFiles)
{
    if (mCluster == nullptr)
        return;
    SxVolume *vol = mCluster->getSxVolume(volume);
    if (vol == nullptr) {
        qDebug() << "ScoutModel::requestDownload failed" << __LINE__;
        return;
    }

    ScoutTask *task = new ScoutTask(true, list.join(", "), volume, localDir+"/", remoteFile);
    qint64 size = 0;

    foreach (QString file, list){
        QString localPath = localDir+"/"+file;
        if (localPath.endsWith("/")) {
            auto files = listDir(localPath);
            foreach (auto file, files) {
                QFileInfo fileInfo(file);
                task->files.append({file, fileInfo.size()});
                size += fileInfo.size();
                if (openLocalFiles) {
                    QFile *f = new QFile(file);
                    qDebug() << file << f->size() << f->open(QIODevice::ReadOnly);
                    //f->open(QIODevice::ReadOnly);
                    task->tmpFileList.append(f);
                }
            }
        }
        else {
            QFileInfo fileInfo(localPath);
            task->files.append({localPath, fileInfo.size()});
            size += fileInfo.size();
            if (openLocalFiles) {
                QFile *f = new QFile(localPath);
                qDebug() << localPath << f->size() << f->open(QIODevice::ReadOnly);
                //f->open(QIODevice::ReadOnly);
                task->tmpFileList.append(f);
            }
        }
    }
    task->size = size;
    appendTask(task);
}

QModelIndex ScoutModel::volumesIndex() const
{
    return mVolumesIndex;
}

QModelIndex ScoutModel::filesIndex() const
{
    return mFilesIndex;
}

bool ScoutModel::sharingEnabled() const
{
    return !mCluster->sxwebAddress().isEmpty() || !mCluster->sxshareAddress().isEmpty();
}

int ScoutModel::filesCount() const
{
    if (mCurrentVolume.isEmpty())
        return mVolumes.count();
    return mFilesCount;
}

QPair<int, qint64> ScoutModel::countFiles(const QString &volume, const QStringList &files)
{
    int count=0;
    qint64 size=0;

    SxVolume *sxVolume = mCluster->getSxVolume(volume);
    if (volume == nullptr) {
        logWarning("unable to get volume "+volume);
        return {0, 0};
    }

    foreach (QString file, files) {
        if (file.endsWith('/')) {
            QList<SxFileEntry*> list;
            QString etag;
            if (!mCluster->_listFiles(sxVolume, file, true, list, etag)) {
                logWarning("file list failed");
                return {0, 0};
            }
            foreach (auto entry, list) {
                if (!entry->path().endsWith("/.sxnewdir")) {
                    ++count;
                    size+=entry->size();
                }
                delete entry;
            }
        }
        else {
            foreach (auto entry, mFileList) {
                if (entry->path() == file) {
                    ++count;
                    size+=entry->size();
                    goto nextLoop;
                }
            }
            logWarning("unable to get file '"+file+"' info");
            return {0, 0};
            nextLoop:
            continue;
        }
    }

    return {count, size};
}

QList<QPair<QString, qint64>> ScoutModel::getRevisions(const QString &volume, const QString &file)
{
    QList<QPair<QString, qint64>> result;
    QList<std::tuple<QString, qint64, quint32>> tmpRevs;
    SxVolume *sxVolume = mCluster->getSxVolume(volume);
    if (volume == nullptr) {
        logWarning("unable to get volume "+volume);
        return result;
    }
    mCluster->listFileRevisions(sxVolume, file, tmpRevs);
    foreach (auto rev, tmpRevs) {
        QString revStr = std::get<0>(rev);
        qint64 size = std::get<1>(rev);
        result.append({revStr, size});
    }
    return result;
}

QList<int> ScoutModel::mapSelectionFrom2D(const QModelIndexList &list)
{
    QList<int> result;
    foreach (auto index, list) {
        result.append(index.row()*mFilesColumnCount+index.column());
    }
    return result;
}

QModelIndexList ScoutModel::create2Dselection(const QList<int> &list)
{
    QModelIndexList result;
    foreach (auto index1d, list) {
        int row = index1d/mFilesColumnCount;
        int column = index1d%mFilesColumnCount;
        result.append(index(row, column, mFilesIndex));
    }
    return result;
}

QModelIndex ScoutModel::findNext(const QChar &c, const QModelIndex &from)
{
    int pos;
    if (from.isValid())
        pos = from.column() + from.row()*mFilesColumnCount;
    else
        pos = -1;
    int itemsCount;
    std::function<QString(int)> getName;
    if (mCurrentVolume.isEmpty()) {
        itemsCount = mVolumes.count();
        getName = [this](int index) {
            return mVolumes.at(index).toLower();
        };
    }
    else {
        itemsCount = mFilesCount;
        getName = [this](int index) {
            return mFileList.at(index)->path().split('/').last().toLower();
        };
    }
    auto mapIndex = [this](int index) ->QModelIndex {
        int row = index/mFilesColumnCount;
        int col = index%mFilesColumnCount;
        return this->index(row, col, mFilesIndex);
    };

    for (int i=pos+1; i<itemsCount; i++) {
        auto name = getName(i);
        if (name.startsWith(c))
            return mapIndex(i);
    }
    for (int i=0; i<pos; i++) {
        auto name = getName(i);
        if (name.startsWith(c))
            return mapIndex(i);
    }
    return QModelIndex();
}

bool ScoutModel::isAesVolume(const QString &volume)
{
    SxVolume* vol = mCluster->getSxVolume(volume);
    if (vol == nullptr || !SxFilter::isFilterSupported(vol))
        return false;
    std::unique_ptr<SxFilter> filter(SxFilter::getActiveFilter(vol));
    if (filter != nullptr && filter->isAes256()) {
        return true;
    }
    return false;
}

void ScoutModel::queueFileUploaded(const QString &volume, const QString &path)
{
    if (mCurrentVolume != volume || !path.startsWith(mCurrentPath))
        return;
    int index = path.indexOf('/', mCurrentPath.length());
    QString testPath;
    if (index == -1)
        testPath = path;
    else
        testPath = path.mid(0, index+1);
    foreach (auto entry, mFileList) {
        if (entry->path() == testPath)
            return;
    }
    refresh(false);
}

void ScoutModel::setFilesColumnCount(int filesColumnCount)
{
    if (filesColumnCount < 1)
        return;
    if (filesColumnCount == mFilesColumnCount)
        return;

    int filesCount = (mCurrentVolume.isEmpty()) ? mVolumes.count() : mFilesCount;
    int newFilesRowCount = filesCount / filesColumnCount + ((filesCount % filesColumnCount == 0) ? 0 : 1);

    resizeModel(mFilesIndex, newFilesRowCount, filesColumnCount);
    mFilesColumnCount = filesColumnCount;
}

void ScoutModel::resizeModel(const QModelIndex &parent, int newRowCount, int newColumnCount)
{
    if (!parent.isValid())
        return;

    int oldColumnCount = columnCount(parent);
    int oldRowCount = rowCount(parent);
    if (newColumnCount > oldColumnCount) {
        beginInsertColumns(parent, oldColumnCount, newColumnCount-1);
        mResizeInfo.insert(parent, {oldRowCount, newColumnCount});
        endInsertColumns();
    }
    else if (newColumnCount < oldColumnCount) {
        beginRemoveColumns(parent, newColumnCount, oldColumnCount-1);
        mResizeInfo.insert(parent, {oldRowCount, newColumnCount});
        endRemoveColumns();
    }
    if (newRowCount > oldRowCount) {
        beginInsertRows(parent, oldRowCount, newRowCount-1);
        mResizeInfo.insert(parent, {newRowCount, newColumnCount});
        endInsertRows();
    }
    else if (newRowCount < oldRowCount) {
        beginRemoveRows(parent, newRowCount, oldRowCount-1);
        mResizeInfo.insert(parent, {newRowCount, newColumnCount});
        endRemoveRows();
    }
    if (mResizeInfo.contains(parent))
        mResizeInfo.remove(parent);
}

void ScoutModel::cancelCurrentTask()
{
    if (mQueue != nullptr)
        mQueue->cancelCurrentTask();
}

void ScoutModel::cancelPendingTask(int index)
{
    if (mQueue != nullptr)
        mQueue->cancelPendingTask(index);
}

QString ScoutModel::sxwebAddress() const
{
    return mCluster->sxwebAddress();
}

QString ScoutModel::sxshareAddress() const
{
    return mCluster->sxshareAddress();
}

ScoutModelHelperThread::ScoutModelHelperThread(const SxAuth &auth, const QByteArray& uuid, std::function<void(QString, qint64, qint64)> callback)
    : QObject()
{
    mCallback = callback;
    mAuth = auth;
    mUuid = uuid;
    connect(this, &ScoutModelHelperThread::startUpload, this, &ScoutModelHelperThread::executeUpload, Qt::QueuedConnection);
    connect(this, &ScoutModelHelperThread::startDownload, this, &ScoutModelHelperThread::executeDownload, Qt::QueuedConnection);
}

ScoutModelHelperThread::~ScoutModelHelperThread()
{
}

void ScoutModelHelperThread::uploadFiles(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir)
{
    QEventLoop eventLoop;
    connect(this, &ScoutModelHelperThread::uploadFinished, &eventLoop, &QEventLoop::quit);
    emit startUpload(rootDir, files, dstVolume, dstDir);
    eventLoop.exec();
}

void ScoutModelHelperThread::downloadFiles(const QString &volume, const QString &rootDir, const QStringList &files, const QString &localDir)
{
    QEventLoop eventLoop;
    connect(this, &ScoutModelHelperThread::uploadFinished, &eventLoop, &QEventLoop::quit);
    emit startDownload(volume, rootDir, files, localDir);
    eventLoop.exec();
}

void ScoutModelHelperThread::executeUpload(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir)
{
    QString errorMessage;
    auto mCheckCert = [this](QSslCertificate &, bool) -> bool {
        return true;
    };
    SxCluster *mCluster = SxCluster::initializeCluster(mAuth, mUuid, mCheckCert, errorMessage);
    if (mCluster == nullptr) {
        emit uploadFinished();
        return;
    }
    connect(this, &ScoutModelHelperThread::abortTask, mCluster, &SxCluster::abort, Qt::DirectConnection);
    mCluster->reloadVolumes();
    SxVolume *volume = mCluster->getSxVolume(dstVolume);
    if (volume == nullptr) {
        emit uploadFinished();
        delete mCluster;
        return;
    }
    mCluster->uploadFiles(rootDir, files, volume, dstDir, mCallback, true);
    emit uploadFinished();
    delete mCluster;
}

void ScoutModelHelperThread::executeDownload(const QString &volume, const QString &remoteDir, const QStringList &files, const QString &localDir)
{
    QString errorMessage;
    auto mCheckCert = [this](QSslCertificate &, bool) -> bool {
        return true;
    };
    SxCluster *mCluster = SxCluster::initializeCluster(mAuth, mUuid, mCheckCert, errorMessage);
    if (mCluster == nullptr) {
        emit uploadFinished();
        qDebug() << "ERROR" << __LINE__;
        return;
    }
    connect(this, &ScoutModelHelperThread::abortTask, mCluster, &SxCluster::abort, Qt::DirectConnection);
    mCluster->reloadVolumes();
    SxVolume *vol = mCluster->getSxVolume(volume);
    if (vol == nullptr) {
        emit uploadFinished();
        delete mCluster;
        qDebug() << "ERROR" << __LINE__;
        return;
    }

    qint64 taskSize = 0;
    QList<QPair<QString, qint64>> toDownload;
    foreach (auto file, files) {
        QString etag;
        if (file.endsWith("/")) {
            QList<SxFileEntry*> list;
            if (!mCluster->_listFiles(vol, file, true, list, etag)) {
                emit uploadFinished();
                qDebug() << "ERROR" << __LINE__;
                return;
            }
            foreach (auto entry, list) {
                if (entry->size()==0) {
                    ++taskSize;
                    toDownload.append({entry->path(), 1});
                }
                else {
                    taskSize += entry->size();
                    toDownload.append({entry->path(), entry->size()});
                }
                delete entry;
            }
            list.clear();
        }
        else {
            QList<SxFileEntry*> list;
            if (!mCluster->_listFiles(vol, file, false, list, etag)) {
                emit uploadFinished();
                qDebug() << "ERROR" << __LINE__;
                return;
            }
            SxFileEntry* fileEntry = nullptr;
            if (list.length() == 1) {
                fileEntry = list.takeFirst();
            }
            else if (list.length() == 2) {
                if (list.at(0)->path().endsWith("/"))
                    fileEntry = list.takeLast();
                else
                    fileEntry = list.takeFirst();
            }
            foreach (auto entry, list) {
                delete entry;
            }
            list.clear();
            if (fileEntry == nullptr) {
                emit uploadFinished();
                qDebug() << "ERROR" << __LINE__;
                return;
            }
            if (fileEntry->size() == 0) {
                ++taskSize;
                toDownload.append({fileEntry->path(), 1});
            }
            else {
                taskSize += fileEntry->size();
                toDownload.append({fileEntry->path(), fileEntry->size()});
            }
            delete fileEntry;
        }
    }
    qint64 done = 0;
    qint64 currentFileSize = 0;
    QString currentFile;

    auto setProgress = [&currentFile, &done, &currentFileSize, &taskSize, this](qint64 size, qint64) {
        if (size >= currentFileSize)
            return;
        qint64 progress = currentFileSize-size;
        currentFileSize = size;
        done+=progress;
        mCallback(currentFile, done, taskSize);
    };
    QMetaObject::Connection connection = connect(mCluster, &SxCluster::sig_setProgress, setProgress);

    foreach (auto file, toDownload) {
        QString path = file.first;
        currentFileSize = file.second;
        SxFileEntry fileEntry;
        QString localPath = localDir+path.mid(remoteDir.length()-1);

        currentFile = path.mid(remoteDir.length()-1);
        mCallback(currentFile, done, taskSize);
        if (!mCluster->downloadFile(vol, path, localPath, fileEntry, 10)) {
            emit uploadFinished();
            qDebug() << "ERROR" << __LINE__;
            break;
        }
        done += currentFileSize;
    }
    disconnect(connection);
    emit uploadFinished();
    delete mCluster;
}
