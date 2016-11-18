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

#ifndef SXQUEUE_H
#define SXQUEUE_H

#include "sxconfig.h"
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QSslCertificate>
#include <QTimer>
#include <memory>
#include "sxstate.h"
#include "sxauth.h"
#include "uploadqueue.h"
#include "sxerror.h"
#include <functional>

class SxVolume;
class SxCluster;

enum class EtaAction {
    Idle,
    Inactive,
    Paused,
    ListClusterNodes,
    ListVolumes,
    VolumeInitialScan,
    ListRemoteFiles,
    UploadFile,
    DownloadFile,
    RemoveRemoteFile,
    RemoveLocalFile
};

Q_DECLARE_METATYPE(EtaAction);

class SxQueue : public QObject
{
    Q_OBJECT
    enum class TaskType {
        ListClusterNodes,
        ListVolumes,
        VolumeInitialScan,
        ListRemoteFiles,
        UploadFile,
        DownloadFile,
        RemoveRemoteFile,
        RemoveLocalFile,
        CheckFileConsistency
    };

    class Task {
    public:
        Task(const TaskType &type, const QString &volume, const QString &path, const int &priority, qint64 size);
        ~Task();
        TaskType type() const;
        QString volume() const;
        QString path() const;
        qint64 size() const;
        int priority() const;
        quint64 id() const;
        bool equal(const Task& other) const;
        QString toString() const;
    private:
        TaskType mType;
        QString mVolume;
        QString mPath;
        int mPriority;
        qint64 mSize;
        const quint64 mId;
        static quint64 sCounter;
    public:
        static QSet<quint64> sLivingTasks;
    };

public:
    SxQueue(SxConfig* config, std::function<bool(QSslCertificate& ,bool)> checkSslCallback, std::function<bool(QString)> askGuiCallback);
    ~SxQueue();
    void addTask(Task* task);
    void clear();
    void clear(const QString& volume);
    void requestRemoteList(const QString& volume);
    void requestVolumeList();
    void setPaused(bool paused);
    bool paused();
    bool abortCurrentTask();

public slots:
    void requestInitialScan();
    void localFileModified(QString volume, QString path, bool removed, qint64 size);
    void cancelUploadTask(const QString &volume, const QString &path);
    void unlockVolume(const QString& volume);
    void onPossibleInconsistency(const QString &volume, const QString &path);

signals:
    void sig_start_task();
    void sig_abort_task();
    void sig_delete_timers();
    void sig_satusChanged(SxStatus status);
    void sig_fileSynchronised(const QString& path, bool upload);
    void sig_setEtaAction(EtaAction action, qint64 taskCounter,QString file, qint64 size, qint64 speed);
    void sig_setEtaCounters(uint upload, qint64 uploadSize, uint download, qint64 downloadSize, uint remove);
    void sig_setProgress(qint64 size, qint64 speed);
    void sig_clusterInitialized(QString sxweb, QString sxshare);
    void sig_fileNotification(QString path, QString action);
    void sig_lockVolume(const QString &volume);
    void sig_initializationFailed();
    void sig_gotVcluster(const QString &vcluster);
    void sig_addWarning(const QString &volume, const QString &file, const QString &message, bool critical);
    void sig_removeWarning(const QString &volume, const QString &file);
    void sig_volumeNameChanged();

private slots:
    void startCurrentTask();
    void deleteTimers();
    void lockVolume(const QString& volume);

private:
    bool _appendRegularTask(Task* task);
    void _instertPriorityTask(Task* task);
    void _executeCurrentTask();
    void _finishCurrentTask();
    bool _reloadVolumeFiles(SxVolume *volume, const QString &volumeRootDir, const QString &etag, bool scanLocalFiles);
    void _emitEtaCounters();
    bool _aborted() const;
    bool getLocalBlocks(QFile *file, qint64 fileSize, int blockSize, const QStringList &fileBlocks, QSet<QString> &missingBlocks);
    bool findIdenticalFiles(const QString& volume, qint64 fileSize, int blockSize, const QStringList &fileBlocks, QList<QPair<QString, quint32>>& files);

    static const int sDownloadConnectionsLimit = 0; //use volume nodes count
    static const int sRemoveRemoteFilesLimit = 10;
    static const int sTimeoutFullScan = 60*60;
    static const int sTimeoutListVolumes = 15;
    static const int sTimeoutListFiles = 15;

    SxConfig *mConfig;
    SxCluster *mCluster;
    Task* mCurrentTask;
    QList<Task*> mTaskList;
    QHash<QString, Task*> mTaskByPath;
    QHash<QString, QString> mEtags;
    mutable QMutex mMutex;
    std::function<bool(QSslCertificate&, bool)> mCheckSslCallback;
    bool mPaused;
    bool mAborted;
    QSet<QTimer*> mTimers;
    SxAuth mAuth;
    QSet<QString> mLockedVolumes;
    QSet<QString> mInconsistentVolumes;
    QHash<QString, UploadQueue*> mPendingUploads;
    std::function<void(QString, QString, SxError, QString, quint32)> mUploadDoneCallback;
    std::function<bool(QString)> mAskGuiCallback;
    bool mQueueIsWorking;

    struct EtaCounters {
        uint uploadCount;
        uint downloadCount;
        uint removeCount;
        qint64 uploadSize;
        qint64 downloadSize;
        EtaCounters();
        void addTask(Task *task);
        void removeTask(Task *task);
        void clear();
    };
    EtaCounters mEtaCounters;
};

#endif // SXQUEUE_H
