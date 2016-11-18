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

#ifndef ScoutModel_H
#define ScoutModel_H

#include <QAbstractItemModel>
#include <QStack>
#include <QThread>
#include "sxcluster.h"
#include "scoutqueue.h"
#include "scoutdatabase.h"
#include "clusterconfig.h"

class ScoutModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit ScoutModel(ClusterConfig* config, ScoutQueue *queue, QObject *parent = 0);
    ~ScoutModel();
    static QString getMimeType(const QString &extension);

signals:
    void signalProgress(const QString &file, int progress, int count);
    void setViewEnabled(bool enabled);
    void configReloaded();
    void abort();
    void clusterInitialized();
    void sigError(const QString& errorMessage);

public slots:
    void cancelClusterTask();
    bool reloadVolumes();

    // QAbstractItemModel interface
public:
    SxCluster *cluster() const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QString currentVolume() const;
    QString lastError() const;
    int currentVolumeIndex() const;
    QString currentPath() const;
    void removeFiles(const QString &volume, const QStringList &files);
    void rename(const QString &volume, const QString &source, const QString &destination);
    void copyFiles(const QString &srcVolume, const QString &srcRoot, const QStringList &files, const QString &dstVolume, const QString &destination);
    void moveFiles(const QString &srcVolume, const QString &srcRoot, const QStringList &files, const QString &dstVolume, const QString &destination);
    void uploadFiles(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir);
    void downloadFiles(const QString &volume, const QString &rootDir, const QStringList &files, const QString &localDir);
    void mkdir(const QString &volume, const QString &path);
    void move(const QString &volume, const QString &path);
    void movePrev();
    void moveNext();
    void moveUp();
    void refresh(bool blockView=true);
    bool isWorking() const;
    bool canMovePrev() const;
    bool canMoveNext() const;
    bool canMoveUp() const;
    void reloadClusterConfig(ScoutQueue *queue);
    bool reloadMeta();
    void requestDownload(const QString &volume, const QString &remoteDir, const QStringList &files, const QString &localFile);
    void requestDownload(const QString &volume, const QString &remoteFile, const QString &rev, qint64 size, const QString &localFile);
    void requestUpload(const QString &volume, const QString &localDir, const QStringList &list, const QString &remoteFile, bool openLocalFiles=false);
    void setFilesColumnCount(int filesColumnCount);
    void resizeModel(const QModelIndex &parent, int rowCount, int columnCount);
    void cancelCurrentTask();
    void cancelPendingTask(int index);
    QString sxwebAddress() const;
    QString sxshareAddress() const;
    QModelIndex volumesIndex() const;
    QModelIndex filesIndex() const;
    QModelIndex currentTaskIndex() const;
    QModelIndex tasksIndex() const;
    bool sharingEnabled() const;
    int filesCount() const;
    QPair<int, qint64> countFiles(const QString &volume, const QStringList &files);
    QList<QPair<QString, qint64>> getRevisions(const QString &volume, const QString &file);
    QList<int> mapSelectionFrom2D(const QModelIndexList &list);
    QModelIndexList create2Dselection(const QList<int> &list);
    QModelIndex findNext(const QChar &c, const QModelIndex &from);
    bool isAesVolume(const QString &volume);

public:
    enum Role {
        MimeTypeRole = Qt::UserRole,
        NameRole,
        FullPathRole,
        SizeRole,
        SizeUsedRole
    };
private slots:
    void queueFileUploaded(const QString &volume, const QString &path);

private:
    void setCurrentPath(const QString &volume, const QString &path, bool blockView);
    void appendTask(ScoutTask *task);
    void copyOrCopyFiles(const QString &srcVolume, const QString &srcRoot, const QStringList &files, const QString &dstVolume, const QString &destination, bool copy);
private:
    const QModelIndex mVolumesIndex = createIndex(0, 0);
    const QModelIndex mFilesIndex = createIndex(1, 0);
    QStringList mVolumes;
    int mFilesCount;
    int mFilesColumnCount;

    ScoutQueue *mQueue;
    ScoutDatabase *mDatabase;

    QHash<const QModelIndex&, QPair<int,int>> mResizeInfo;
    QString mCurrentVolume;
    QString mCurrentPath;
    ClusterConfig *mClusterConfig;
    SxCluster *mCluster;
    QList<SxFileEntry*> mFileList;
    QStack<QPair<QString, QString>> mPrevStack;
    QStack<QPair<QString, QString>> mNextStack;
    QSet<QString> mUnlockedVolumes;
    bool mReloading;
    std::function<bool(QSslCertificate& cert, bool secondaryCert)> mCheckCertCallback;
    QTimer mRefreshTimer;
    QString mLastError;
};

class ScoutModelHelperThread : public QObject {
    Q_OBJECT
public:
    explicit ScoutModelHelperThread(const SxAuth &auth, const QByteArray& uuid, std::function<void(QString, qint64, qint64)> callback);
    ~ScoutModelHelperThread();
    void uploadFiles(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir);
    void downloadFiles(const QString &volume, const QString &rootDir, const QStringList &files, const QString &localDir);
signals:
    void startUpload(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir);
    void startDownload(const QString &volume, const QString &rootDir, const QStringList &files, const QString &localDir);
    void uploadFinished();
    void abortTask();
private slots:
    void executeUpload(const QString &rootDir, const QStringList &files, const QString &dstVolume, const QString &dstDir);
    void executeDownload(const QString &volume, const QString &remoteDir, const QStringList &files, const QString &localDir);
private:
    std::function<void(QString, qint64, qint64)> mCallback;
    SxAuth mAuth;
    QByteArray mUuid;
};

#endif // ScoutModel_H
