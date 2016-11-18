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

#ifndef ScoutModelQUEUE_H
#define ScoutModelQUEUE_H

#include <QAbstractItemModel>
#include <QObject>
#include <QMutex>
#include <QVariant>
#include "sxcluster.h"
#include "clusterconfig.h"

struct ScoutTask {
    ScoutTask(bool upload, const QString &title, const QString &volume, const QString &localPath, const QString &remotePath);
    ~ScoutTask();
    bool upload;
    QString localPath;
    QString volume;
    QString remotePath;
    QString rev;
    qint64 size;
    QList<QPair<QString, qint64>> files;
    QString title;
    QString error;
    QList<QFile*> tmpFileList;
};

class ScoutQueue : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit ScoutQueue(ClusterConfig *config, QObject *parent = 0);
    void appendTask(ScoutTask *task);
    QVariant currentTaskData(int role) const;
    QVariant pendingTaskData(int index, int role) const;
    int pendingCount() const;
    void cancelCurrentTask();
    void cancelPendingTask(int index);
    bool isWorking() const;
    void retryCurrentTask();
    void retryTask(int index);
    QModelIndex currentTaskIndex() const;
    QModelIndex tasksIndex() const;
    bool hasFailedTasks() const;

    enum Role {
        DirectionRole = Qt::UserRole,
        TitleRole,
        ErrorRole,
        SizeRole,
        ProgressRole
    };

signals:
    void startTask();
    void finished();
    void fileUploaded(const QString &volume, const QString &file);
    void sigShowWarning(bool showWarning);

private slots:
    void executeTask();

private:
    const QModelIndex mCurrentTaskIndex = createIndex(1, 0);
    const QModelIndex mTasksIndex = createIndex(2, 0);
    mutable QMutex mMutex;
    ScoutTask *mCurrentTask;
    SxCluster *mCluster;
    QString mCurrentFile;
    qint64 mCurrentFileSize;
    qint64 mCurrentFileProgress;
    qint64 mCurrentTaskProgress;
    QList<ScoutTask *> mPendingList;
    QList<ScoutTask *> mFailedTasks;
    ClusterConfig *mClusterConfig;

    // QAbstractItemModel interface
public:
    QModelIndex index(int row, int column, const QModelIndex &parent) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
};

#endif // ScoutModelQUEUE_H
