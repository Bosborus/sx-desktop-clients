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

#ifndef SXCONTROLLER_H
#define SXCONTROLLER_H

#include "sxqueue.h"
#include "sxfilesystem.h"
#include "sxstate.h"

#include <QObject>
#include <QThread>
#include <QSslCertificate>
#include <sxcluster.h>

class SxController : public QObject
{
    Q_OBJECT
public:
    explicit SxController(SxConfig *config, std::function<bool(QSslCertificate&,bool)> checkCertCallback, std::function<bool(QString)> askGuiCallback, QObject* parrent);
    ~SxController();
    SxStatus status() const;
    const SxState &sxState() const;
    int warningsCount() const;
    bool pause();
    bool resume();
    void reinitCluster();
    void unlockVolume(const QString &volume);

private:
    void _initCluster();
    void _destroyCluster();

signals:
    void sig_fileSynchronised(const QString& path, bool upload);
    void sig_setEtaAction(EtaAction action, qint64 taskCounter,QString file, qint64 size, qint64 speed);
    void sig_setEtaCounters(uint upload, qint64 uploadSize, uint download, qint64 downloadSize, uint remove);
    void sig_setProgress(qint64 size, qint64 speed);
    void sig_clusterInitialized(QString sxweb, QString sxshare);
    void sig_fileNotification(QString path, QString action);
    void sig_lockVolume(const QString &volume);
    void sig_gotVcluster(const QString &vcluster);
    void sig_unlockVolume(const QString &volume);
    void sig_requestVolumeList();
    void sig_volumeNameChanged();

public slots:
    void onSatusChanged(SxStatus status);
    void restartFilesystem();
    void startCluster();
    void onClusterInitializationFailed();
    void clearWarnings();

private slots:
    void onVolumeNameChanged();
private:
    const int initializationRetryTime = 60;
    QThread* mQueueThread;
    QThread* mFilesystemScannerThread;
    SxCluster *mCluster;
    SxQueue *mQueue;
    SxFilesystem *mFilesystem;
    SxState mState;
    SxConfig *mConfig;
    std::function<bool(QSslCertificate&, bool)> mCheckSslCallback;
    std::function<bool(QString)> mAskGuiCallback;
    bool mStarted;
    QTimer *timer;
};

#endif // SXCONTROLLER_H
