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

#ifndef SXFILESYSTEM_H
#define SXFILESYSTEM_H

#include <QDir>
#include <QHash>
#include <QObject>
#include "sxconfig.h"
#include <QTimer>
#include <QSet>

#if defined Q_OS_WIN
#elif defined Q_OS_LINUX
    #include <errno.h>
    #include <poll.h>
    #include <stdlib.h>
    #include <sys/inotify.h>
    #include <unistd.h>
#else
    #include <QFileSystemWatcher>
#endif

#ifdef Q_OS_WIN
    class WatchedDir;
#endif


class SxFilesystem : public QObject
{
    Q_OBJECT
public:
    explicit SxFilesystem(SxConfig *config);
    ~SxFilesystem();
    static QList<QString> getDirectoryContents(QDir &rootDir, bool recursive, const QString &prefix=QString(), bool removeTempfiles = false);
    static QList<QString> getSubdirectories(QDir &rootDir, const QString &prefix=QString());
    bool watchDirectory(const QString &volume, const QString &directory);
    bool unwatchDirectory(const QString &volume);

signals:
    void sig_fileModified(QString volume, QString path, bool removed, qint64 size);
    void sig_cancelUploadTask(const QString &volume, const QString &path);

private:

    struct QuededTask{
        QuededTask(QString volume, QString path, qint64 size) {
            this->volume = volume;
            this->path = path;
            this->size = size;
        }
        QString volume;
        QString path;
        qint64 size;
    };

    QHash<QString, QString> mWatchedDirectories;
    QList<QuededTask*> mQueuedUploads;
    QList<QuededTask*> mQueuedRemovals;
    QHash<QString, QPair<QList<QuededTask*>*, QuededTask*>> mQuededTaskByName;

    static const int sSignalDelay = 5;
    bool watchDirRecursively(const QString &path);
    void fileModified(const QString &volume, const QString &path, bool removed, qint64 size);
    void emitQueuedSignals();

private slots:
    void directoryChanged(const QString &path);
    void scanDirectory(const QString &path);
    void inotifyPoll();
    void inotifyProcess();

private:
#if defined Q_OS_WIN || defined Q_OS_LINUX
    QTimer *mNotifyTimer;
    QSet<QString> mNotifyFiles;
#endif

#if defined Q_OS_WIN
    QHash<WatchedDir*, QString> mDirHandlers;
    friend void notifyFileChange(WatchedDir* dir, const QString &path );
    friend QString getWatchedDirPath(WatchedDir* dir);
#elif defined Q_OS_LINUX
    bool inotifyHandleEvents();
    int mInotifyDesc;
    QHash<int, QString> mDirsDesc;
    struct pollfd mPollDesc[1];
#else
    QFileSystemWatcher mQtWatcher;
    QHash<QString, QTimer*> mTimers;
#endif
};

#endif // SXFILESYSTEM_H
