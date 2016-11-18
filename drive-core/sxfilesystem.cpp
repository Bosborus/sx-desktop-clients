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

#include "sxdatabase.h"
#include "sxfilesystem.h"
#include "sxlog.h"
#include <QTimer>

#ifdef Q_OS_WIN

#include <Windows.h>

class WatchedDir{
public:
    WatchedDir(HANDLE dir, SxFilesystem *filesystemWatcher);
    ~WatchedDir();
    bool watch();
    void unwatch();
    HANDLE mDir;
    OVERLAPPED mOverlaped;
    const int mBufferSize = 16384;
    char *mBuffer;
    unsigned long mDwBytes;
    SxFilesystem *mFilesystemWatcher;
    bool destroy;
};

void notifyFileChange(WatchedDir* dir, const QString &path ) {
    SxFilesystem *watcher = dir->mFilesystemWatcher;
    QString rootDir = watcher->mDirHandlers.value(dir, "");
    if (rootDir.isEmpty()) {
        logError("logic error");
        return;
    }
    QString volume = watcher->mWatchedDirectories.key(rootDir, "");
    if (volume.isEmpty()) {
        logError("logic error");
        return;
    }

    uint32_t mtime;
    QFileInfo fileInfo(rootDir+"/"+path);
    if (fileInfo.isFile() && SxDatabase::instance().getLocalFileMtime(volume, "/"+path, mtime)) {
        if (mtime == fileInfo.lastModified().toTime_t())
            return;
    }

    watcher->mNotifyFiles.insert(volume+"/"+path);

    if (watcher->mNotifyTimer == nullptr) {
        watcher->mNotifyTimer = new QTimer();
        watcher->mNotifyTimer->setSingleShot(true);
        QObject::connect(watcher->mNotifyTimer, &QTimer::timeout, watcher, &SxFilesystem::inotifyProcess);
    }
    watcher->mNotifyTimer->start(watcher->sSignalDelay*1000);
}

QString getWatchedDirPath(WatchedDir* dir) {
    SxFilesystem *watcher = dir->mFilesystemWatcher;
    return watcher->mDirHandlers.value(dir);
}

VOID CALLBACK NotificationCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped) {
    Q_UNUSED(dwErrorCode);
    Q_UNUSED(dwNumberOfBytesTransfered);
    WatchedDir* dir = (WatchedDir*) lpOverlapped->hEvent;
    if (dir->destroy) {
        delete dir;
        return;
    }
    DWORD index = 0;
    FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION*) &dir->mBuffer[index];

    while (true) {
        QString path = QDir::fromNativeSeparators(QString::fromUtf16((const ushort*)info->FileName, info->FileNameLength/2));
        if (info->Action == FILE_ACTION_MODIFIED) {
            QFileInfo fileInfo(getWatchedDirPath(dir)+"/"+path);
            if (fileInfo.isFile() && !path.split("/").last().startsWith("._sdrvtmp")) {
                notifyFileChange(dir, path);
            }
        }
        else {
            if (!path.split("/").last().startsWith("._sdrvtmp"))
                notifyFileChange(dir, path);
        }
        if (info->NextEntryOffset == 0)
            break;
        else {
            index += info->NextEntryOffset;
            info = (FILE_NOTIFY_INFORMATION*) &dir->mBuffer[index];
        }
    }

    if (!dir->watch()) {
        logError("ReadDirectoryChangesW failed");
    }
}

WatchedDir::WatchedDir(HANDLE dir, SxFilesystem *filesystemWatcher) {
    mDir = dir;
    mBuffer = new char[mBufferSize];
    mFilesystemWatcher = filesystemWatcher;
    ZeroMemory(&mOverlaped, sizeof(mOverlaped));
    mOverlaped.hEvent = this;
    destroy = false;
}

WatchedDir::~WatchedDir() {
    delete [] mBuffer;
}

bool WatchedDir::watch() {
    return ReadDirectoryChangesW(
                mDir,
                mBuffer,
                mBufferSize,
                TRUE,
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME,
                &mDwBytes,
                &mOverlaped,
                &NotificationCompletion);
}

void WatchedDir::unwatch()
{
    CloseHandle(mDir);
    destroy = true;
}
#endif

SxFilesystem::SxFilesystem(SxConfig *config) : QObject(0)
{
#if defined Q_OS_WIN
    mNotifyTimer = nullptr;
#elif defined Q_OS_LINUX
    mInotifyDesc = inotify_init1(IN_NONBLOCK);
    if (mInotifyDesc == -1) {
        logError(QString("inotify_init1 failed: %1").arg(strerror(errno)));
        return;
    }
    mPollDesc[0].fd = mInotifyDesc;
    mPollDesc[0].events = POLLIN;
    mNotifyTimer = nullptr;
#else
    connect(&mQtWatcher, &QFileSystemWatcher::directoryChanged, this, &SxFilesystem::directoryChanged);
#endif
    foreach (QString volName, config->volumes()) {
        VolumeConfig volume = config->volume(volName);
        QString localPath = volume.localPath();
        if (!watchDirectory(volName, localPath)) {
            logWarning(QString("unable to watch directory \"%1\"").arg(localPath));
        }
    }
#ifdef Q_OS_LINUX
    if (!mDirsDesc.isEmpty())
        QTimer::singleShot(0, this, SLOT(inotifyPoll()));
#endif
}

SxFilesystem::~SxFilesystem()
{
#if !defined Q_OS_WIN && !defined Q_OS_LINUX
    foreach (QTimer* timer, mTimers) {
        timer->stop();
        timer->deleteLater();
    }
#endif
#if defined Q_OS_WIN
    foreach (auto dir, mDirHandlers.keys()) {
        dir->unwatch();
    }
    mDirHandlers.clear();
#endif
}

QList<QString> SxFilesystem::getDirectoryContents(QDir &rootDir, bool recursive, const QString &prefix, bool removeTempfiles) {
    logEntry(rootDir.absolutePath());
    QList<QString> result;
    QList<QDir> list {rootDir};
    QString absoluteRootPath = rootDir.absolutePath();
    if (!absoluteRootPath.endsWith("/"))
        absoluteRootPath+="/";
    while (!list.isEmpty()) {
        const QDir d = list.takeFirst();
        static const auto flag = QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::Hidden | QDir::NoDotAndDotDot;
        auto entryInfoList = d.entryInfoList(flag, QDir::Name);
        if (entryInfoList.isEmpty() && prefix != "/") {
            if (!prefix.isEmpty() || d!=rootDir) {
                QString absoluteFilePath = d.absoluteFilePath(".sxnewdir");
                QFile sxnewdir(absoluteFilePath);
                if (sxnewdir.open(QIODevice::WriteOnly)) {
                    sxnewdir.close();
                    if (!absoluteFilePath.startsWith(absoluteRootPath))
                        throw new std::runtime_error("makePathRelative failed");
                    QString relativePath = absoluteFilePath.mid(absoluteRootPath.length());

                    if (relativePath.startsWith("/")) {
                        if (!prefix.endsWith("/"))
                            result.append(prefix+relativePath);
                        else
                            result.append(prefix+relativePath.mid(1));
                    }
                    else if (!prefix.endsWith("/"))
                        result.append(prefix+"/"+relativePath);
                    else
                        result.append(prefix+relativePath);
                }
            }
        }
        foreach (auto entryInfo, entryInfoList) {
            if (entryInfo.isDir()) {
                if (recursive) {
                    list.append(QDir(entryInfo.absoluteFilePath()));
                }
            }
            else if (entryInfo.isFile()) {
                QString name = entryInfo.fileName().split("/").last();
                if (name.startsWith("._sdrvtmp")) {
                    if (removeTempfiles)
                        entryInfo.dir().remove(name);
                    continue;
                }
                QString absoluteFilePath = entryInfo.absoluteFilePath();
                if (!absoluteFilePath.startsWith(absoluteRootPath))
                    throw new std::runtime_error("makePathRelative failed");
                QString relativePath = absoluteFilePath.mid(absoluteRootPath.length());
                if (relativePath.startsWith("/")) {
                    if (prefix.endsWith("/"))
                        result.append(prefix+relativePath.mid(1));
                    else
                        result.append(prefix+relativePath);
                }
                else if (prefix.endsWith("/"))
                    result.append(prefix+relativePath);
                else
                    result.append(prefix+"/"+relativePath);
            }
        }
    }
    return result;
}

QList<QString> SxFilesystem::getSubdirectories(QDir &rootDir, const QString &prefix)
{
    logEntry(rootDir.absolutePath());
    QList<QString> result;
    static const auto flag = QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot;
    auto entryInfoList = rootDir.entryList(flag, QDir::Name);
    foreach (auto dir, entryInfoList) {
        if (prefix.endsWith("/"))
            result.append(prefix+dir);
        else
            result.append(prefix+"/"+dir);
    }
    return result;
}

bool SxFilesystem::watchDirectory(const QString &volume, const QString &directory)
{
    logEntry(QString("volume: %1, directory: %2").arg(volume).arg(directory));
    if (mWatchedDirectories.keys().contains(volume))
        return false;
    foreach (QString dir, mWatchedDirectories.values()) {
        if (directory.startsWith(dir+"/"))
            return false;
    }
    if (watchDirRecursively(directory)) {
        mWatchedDirectories.insert(volume, directory);
        return true;
    }
    return false;
}

bool SxFilesystem::unwatchDirectory(const QString &volume)
{
    Q_UNUSED(volume)
    return false;
}

void SxFilesystem::directoryChanged(const QString &path)
{
#if defined Q_OS_WIN || defined Q_OS_LINUX
    Q_UNUSED(path);
#else
    logDebug(path);
    QTimer *timer = mTimers.value(path, nullptr);
    if (timer == nullptr) {
        timer = new QTimer();
        timer->setSingleShot(true);
        timer->setInterval(sSignalDelay*1000);
        mTimers.insert(path, timer);
        connect(timer, &QTimer::timeout, [path, this]() {
            this->scanDirectory(path);
        });
    }
    timer->start();
#endif
}

void SxFilesystem::scanDirectory(const QString &path)
{
#if defined Q_OS_WIN || defined Q_OS_LINUX
    Q_UNUSED(path);
#else
    logDebug(path);
    if (!mTimers.contains(path)) {
        logDebug("skipping scaning directory: "+path);
        return;
    }
    QTimer *timer = mTimers.take(path);
    delete timer;
    QString volume;
    QString dir;
    QString volumeRootDir;

    foreach (QString vol, mWatchedDirectories.keys()) {
        QString rootDir = mWatchedDirectories.value(vol);
        if (path == rootDir || path.startsWith(rootDir+"/")) {
            volume = vol;
            dir = path.mid(rootDir.length());
            if (dir.isEmpty())
                dir = "/";
            volumeRootDir = rootDir;
            break;
        }
    }
    if (volume.isEmpty()) {
        logWarning("unable to scan directory "+path);
        return;
    }
    QDir targetDir(path);
    auto localFiles = getDirectoryContents(targetDir, false, dir);
    auto localDirs = getSubdirectories(targetDir, dir);
    foreach (QString d, localDirs) {
        if (!mQtWatcher.directories().contains(volumeRootDir+d)) {
            if (!watchDirRecursively(volumeRootDir+d)) {
                logWarning("unable to watch directory "+volumeRootDir+d);
            }
        }
    }
    SxDatabase &db = SxDatabase::instance();
    QStringList filesToUpload;
    QStringList filestoRemoveRemote;
    db.startUpdatingFiles(nullptr);
    if (db.markLocalDirFilesToRemove(volume, dir, true)) {
        QDir volumeRootDir(mWatchedDirectories.value(volume));
        if (db.updateLocalFiles(volume, localFiles, volumeRootDir) &&
                db.updateLocalDirs(volume, localDirs, volumeRootDir)) {
            filesToUpload = db.getMarkedFiles(volume, SxDatabase::ACTION::UPLOAD);
            filestoRemoveRemote = db.getMarkedFiles(volume, SxDatabase::ACTION::REMOVE_REMOTE);
        }
    }
    db.endUpdatingFiles();
    foreach (QString file, filesToUpload) {
        logInfo(QString("found modified file (volume: %1, path: %2)").arg(volume,file));
        QFileInfo fileInfo(volumeRootDir+file);
        emit sig_fileModified(volume, file, false, fileInfo.size());
    }
    foreach (QString file, filestoRemoveRemote) {
        if (SxDatabase::instance().remoteFileExists(volume, file)) {
            logInfo(QString("missing file (volume: %1, path: %2)").arg(volume,file));
            emit sig_fileModified(volume, file, true, 0);
        }
        else {
            emit sig_cancelUploadTask(volume, file);
            if(!SxDatabase::instance().dropFileEntry(volume, file))
                logWarning("something went wrong");
        }
    }
#endif
}

void SxFilesystem::inotifyPoll()
{
#ifdef Q_OS_LINUX
    int poll_num = poll(mPollDesc, 1, 10);
    if (poll_num > 0) {
        if (mPollDesc[0].revents & POLLIN)
            inotifyHandleEvents();
    }
    QTimer::singleShot(100, this, SLOT(inotifyPoll()));
#endif
}

void SxFilesystem::inotifyProcess()
{
    logEntry("");
    #ifdef Q_OS_WIN

    QStringList files =  mNotifyFiles.toList();
    mNotifyFiles.clear();
    qSort(files);

    SxDatabase &db = SxDatabase::instance();

    foreach (QString full_path, files) {
        int index = full_path.indexOf("/");
        QString volume = full_path.mid(0, index);
        QString path = full_path.mid(index);
        bool wasDir = false;
        bool wasFile = false;
        uint32_t mtime;
        if (db.getLocalFileMtime(volume, path, mtime)) {
            wasFile = true;
        }
        else {
            wasDir = db.isLocalDir(volume, path);
        }
        QFileInfo fileInfo(mWatchedDirectories.value(volume)+"/"+path);
        if (fileInfo.exists()) {
            bool removeDir = false;
            if (wasFile) {
                if (fileInfo.isFile()) {
                    uint32_t mtime;
                    if (SxDatabase::instance().getLocalFileMtime(volume, path, mtime)) {
                        if (fileInfo.lastModified().toTime_t() == mtime) {
                            logDebug("skipping file "+path);
                            continue;
                        }
                    }
                    fileModified(volume, path, false, fileInfo.size());
                }
                else {
                    fileModified(volume, path, true, 0);
                    if (fileInfo.isDir()) {
                        QDir localDir(mWatchedDirectories.value(volume)+"/"+path);
                        auto local_files = getDirectoryContents(localDir, true, path);
                        foreach (QString f, local_files) {
                            QFileInfo fileInfo(mWatchedDirectories.value(volume)+f);
                            fileModified(volume, f, false, fileInfo.size());
                        }
                    }
                }
            }
            else if (wasDir) {
                removeDir = true;
                db.startUpdatingFiles(nullptr);
                db.markLocalDirFilesToRemove(volume, path, true);
                if (fileInfo.isDir()) {
                    QDir volumeRootDir(mWatchedDirectories.value(volume));
                    QDir localDir(mWatchedDirectories.value(volume)+"/"+path);
                    auto local_files = getDirectoryContents(localDir, true, path);
                    db.updateLocalFiles(volume, local_files, volumeRootDir);
                }
                QStringList toRemove = db.getMarkedFiles(volume, SxDatabase::ACTION::REMOVE_REMOTE);
                QStringList toUpload = db.getMarkedFiles(volume, SxDatabase::ACTION::UPLOAD);
                db.endUpdatingFiles();
                foreach (QString f, toRemove) {
                    fileModified(volume, f, true, 0);
                }
                foreach (QString f, toUpload) {
                    QFileInfo fileInfo(mWatchedDirectories.value(volume)+f);
                    fileModified(volume, f, false, fileInfo.size());
                }
                if (fileInfo.isFile())
                    fileModified(volume, path, false, fileInfo.size());
            }
            else {
                if (fileInfo.isFile())
                    fileModified(volume, path, false, fileInfo.size());
                if (fileInfo.isDir()) {
                    QDir localDir(mWatchedDirectories.value(volume)+"/"+path);
                    auto local_files = getDirectoryContents(localDir, true, path);
                    foreach (QString f, local_files) {
                        QFileInfo fileInfo(mWatchedDirectories.value(volume)+f);
                        fileModified(volume, f, false, fileInfo.size());
                    }
                }
            }
        }
        else {
            if (wasDir) {
                db.startUpdatingFiles(nullptr);
                db.markLocalDirFilesToRemove(volume, path, true);
                QStringList list = db.getMarkedFiles(volume, SxDatabase::ACTION::REMOVE_REMOTE);
                db.endUpdatingFiles();
                foreach (QString f, list) {
                    fileModified(volume, f, true, 0);
                }
            }
            else if (wasFile){
                fileModified(volume, path, true, 0);
            }
        }
    }

    #endif
    #ifdef Q_OS_LINUX
    QStringList files =  mNotifyFiles.toList();
    mNotifyFiles.clear();
    qSort(files);
    foreach (QString path, files) {
        QString volume;
        QString rootDir;
        foreach (QString volName, mWatchedDirectories.keys()) {
            QString dir = mWatchedDirectories.value(volName);
            if (path.startsWith(dir+"/")) {
                volume = volName;
                rootDir= dir;
                break;
            }
        }
        if (volume.isEmpty()) {
            logWarning("unable to find volume for file "+path);
            continue;
        }
        if (path.endsWith("/")) {
            path = path.mid(0, path.length()-1);
            QString relativePath = path.mid(rootDir.length());
            QDir dir(path);
            if (dir.exists()) {
                auto localFiles = getDirectoryContents(dir, true, relativePath);
                foreach (QString newFile, localFiles) {
                    uint32_t mtime;
                    QFileInfo fileInfo(rootDir+newFile);
                    if (SxDatabase::instance().getLocalFileMtime(volume, newFile, mtime)) {
                        if (fileInfo.lastModified().toTime_t() == mtime) {
                            logDebug("skipping file "+path);
                            continue;
                        }
                    }
                    else
                        fileModified(volume, newFile, false, fileInfo.size());
                }
            }
            else {
                SxDatabase &db = SxDatabase::instance();
                db.startUpdatingFiles(nullptr);
                db.markLocalDirFilesToRemove(volume, relativePath, true);
                QStringList list = db.getMarkedFiles(volume, SxDatabase::ACTION::REMOVE_REMOTE);
                db.endUpdatingFiles();
                foreach (QString f, list) {
                    fileModified(volume, f, true, 0);
                }

                foreach (int d, mDirsDesc.keys()) {
                    if (mDirsDesc.value(d).startsWith(path+"/"))
                        mDirsDesc.remove(d);
                }
                int desc = mDirsDesc.key(path, -1);
                if (desc != -1)
                    mDirsDesc.remove(desc);
            }
        }
        else {
            QFileInfo fileInfo(path);
            QString relativePath = path.mid(rootDir.length());
            uint32_t mtime;
            if (fileInfo.exists()) {
                if (SxDatabase::instance().getLocalFileMtime(volume, relativePath, mtime)) {
                    QFileInfo info(path);
                    if (info.lastModified().toTime_t() == mtime) {
                        logDebug("skipping file "+path);
                        continue;
                    }
                }
                fileModified(volume, relativePath, false, fileInfo.size());
            }
            else {
                if (SxDatabase::instance().remoteFileExists(volume, relativePath))
                    fileModified(volume, relativePath, true, 0);
                else
                    emit sig_cancelUploadTask(volume, relativePath);
            }
        }
    }
    #endif
    emitQueuedSignals();
}

bool SxFilesystem::watchDirRecursively(const QString &path)
{
    logEntry(path);
    QFileInfo info(path);
    if (!info.isDir())
        return false;
#if defined Q_OS_WIN
    HANDLE dir = CreateFile(
                (LPCWSTR)QDir::toNativeSeparators(path).utf16(),
                FILE_LIST_DIRECTORY,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                NULL);
    if (dir == INVALID_HANDLE_VALUE) {
        logError("Unable to create directory handle");
        return false;
    }
    WatchedDir* watchedDir = new WatchedDir(dir, this);
    if (!watchedDir->watch()) {
        delete watchedDir;
        return false;
    }
    mDirHandlers.insert(watchedDir, path);
    return true;


#else
    QStringList list = {path};
    QStringList watchlist;
    while (!list.isEmpty()) {
        QString dir = list.takeFirst();
        watchlist.append(dir);
        QDir d(dir);
        foreach (QFileInfo info, d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            list.append(info.absoluteFilePath());
        }
    }
#if defined Q_OS_LINUX
    foreach (QString dirPath, watchlist) {
        int wd;
        wd = inotify_add_watch(mInotifyDesc, dirPath.toUtf8().constData(), IN_CLOSE_WRITE | IN_MOVE | IN_CREATE | IN_DELETE);
        if (wd == -1) {
            logError(QString("inotify_add_watch failed: %s").arg(strerror(errno)));
            return false;
        }
        mDirsDesc.insert(wd, dirPath);
    }
    return true;
#else
    return mQtWatcher.addPaths(watchlist).isEmpty();
#endif
#endif
}

void SxFilesystem::fileModified(const QString &volume, const QString &path, bool removed, qint64 size)
{
    if (mQuededTaskByName.contains(volume+path)) {
        auto pair = mQuededTaskByName.take(volume+path);
        auto task = pair.second;
        pair.first->removeOne(task);
        delete task;
    }
    auto task = new QuededTask(volume, path, size);
    if (removed) {
        mQueuedRemovals.append(task);
        mQuededTaskByName.insert(volume+path, {&mQueuedRemovals, task});
    }
    else {
        mQueuedUploads.append(task);
        mQuededTaskByName.insert(volume+path, {&mQueuedUploads, task});
    }
}

void SxFilesystem::emitQueuedSignals()
{
    mQuededTaskByName.clear();
    foreach (auto task, mQueuedUploads) {
        emit sig_fileModified(task->volume, task->path, false, task->size);
        delete task;
    }
    mQueuedUploads.clear();
    foreach (auto task, mQueuedRemovals) {
        emit sig_fileModified(task->volume, task->path, true, task->size);
        delete task;
    }
    mQueuedRemovals.clear();
}

#ifdef Q_OS_LINUX
bool SxFilesystem::inotifyHandleEvents()
{
    logEntry("SOMETHING CHANGED !!!");
    char buf[4096]
            __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;
    ssize_t len;
    char *ptr;

    for (;;) {
        len = read(mInotifyDesc, buf, sizeof buf);
        if (len == -1 && errno != EAGAIN) {
            logError(QString("read error: %1").arg(strerror(errno)));
            return false;
        }
        if (len <= 0)
            break;

        for (ptr = buf; ptr < buf + len; ptr += sizeof(struct inotify_event) + event->len) {
            event = reinterpret_cast<const struct inotify_event *>(ptr);
            if (event->len==0)
                continue;
            QString path = mDirsDesc.value(event->wd)+"/"+QString::fromLocal8Bit(event->name);
            if (path.split("/").last().startsWith("._sdrvtmp"))
                continue;
            if (event->mask & IN_ISDIR) {
                QDir dir(path);
                if (dir.exists()) {
                    if (!mDirsDesc.values().contains(path)) {
                        watchDirRecursively(path);
                    }
                }
                path.append("/");
            }
            mNotifyFiles.insert(path);
        }
    }
    if (!mNotifyFiles.isEmpty()) {
        if (mNotifyTimer == nullptr) {
            mNotifyTimer = new QTimer();
            mNotifyTimer->setSingleShot(true);
            connect(mNotifyTimer, &QTimer::timeout, this, &SxFilesystem::inotifyProcess);
        }
        mNotifyTimer->start(sSignalDelay*1000);
    }
    return true;
}
#endif
