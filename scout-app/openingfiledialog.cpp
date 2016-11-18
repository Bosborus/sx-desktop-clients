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

#include "openingfiledialog.h"
#include <QSslCertificate>
#include <QStandardPaths>
#include <QDir>
#include <memory>
#include <QThread>

OpeningFileDialog::OpeningFileDialog(ClusterConfig *config, QWidget *parent)
    :ProgressDialog (tr("Opening file"), true, parent)
{
    mConfig = config;
    setModal(true);
}

QString OpeningFileDialog::downloadFile(const QString &volume, const QString &path, const QString &localPath, qint64 cacheSize)
{
    mDestination.clear();
    mDownloadSize = 0;

    auto helper = new OFD_Helper::HelperThread(mConfig);
    QThread *thread = new QThread();
    thread->start();
    helper->moveToThread(thread);
    connect(helper, &OFD_Helper::HelperThread::sigDownloadSize, this, &OpeningFileDialog::setDownloadSize);
    connect(helper, &OFD_Helper::HelperThread::sigSetProgress, this, &OpeningFileDialog::setDownloadProgress);
    connect(helper, &OFD_Helper::HelperThread::downloadFinished, this, &OpeningFileDialog::downloadFinished);
    connect(this, &OpeningFileDialog::sigDownloadFile, helper, &OFD_Helper::HelperThread::downloadFile);
    connect(this, &ProgressDialog::cancelTask, helper, &OFD_Helper::HelperThread::abort, Qt::DirectConnection);
    emit sigDownloadFile(volume, path, localPath, cacheSize);
    exec();
    thread->quit();
    thread->wait();
    delete helper;
    delete thread;
    return mDestination;
}

void OpeningFileDialog::downloadFinished(QString path)
{
    mDestination = path;
    accept();
}

void OpeningFileDialog::setDownloadSize(qint64 size)
{
    mDownloadSize = size;
}

void OpeningFileDialog::setDownloadProgress(qint64 size, qint64)
{
    if (mDownloadSize <= 0)
        return;
    if (size >= mDownloadSize)
        this->setProgress(1000, 1000);
    else {
        int p = static_cast<int>((mDownloadSize-size)*1000/mDownloadSize);
        this->setProgress(p, 1000);
    }
}

OFD_Helper::HelperThread::HelperThread(ClusterConfig *config)
{
    mConfig = config;
}

void OFD_Helper::HelperThread::downloadFile(const QString &volume, const QString &path, const QString &localPath, qint64 cacheSize)
{
    auto checkSsl = [](QSslCertificate &, bool) {
        return true;
    };
    QString errorMessage;
    std::unique_ptr<SxCluster> mCluster(SxCluster::initializeCluster(mConfig->sxAuth(), mConfig->uuid(), checkSsl, errorMessage));
    if (mCluster == nullptr) {
        emit downloadFinished("");
        return;
    }
    connect(this, &HelperThread::cancelDownload, mCluster.get(), &SxCluster::abort, Qt::DirectConnection);
    connect(mCluster.get(), &SxCluster::sig_setDownloadSize, this, &HelperThread::sigDownloadSize);
    connect(mCluster.get(), &SxCluster::sig_setProgress, this, &HelperThread::sigSetProgress);
    mCluster->reloadVolumes();
    SxVolume *sxVolume = mCluster->getSxVolume(volume);
    if (sxVolume == nullptr) {
        emit downloadFinished("");
        return;
    }
    QString destination;
    if (localPath.isEmpty()) {
        static QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/SxScout";
        QDir dir(tmpDir);
        if (!dir.mkpath(".")) {
            emit downloadFinished("");
            return;
        }
        destination = dir.filePath(path.split("/").last());
        auto files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDir::Time | QDir::Reversed);
        qint64 totalSize = 0;
        foreach (auto f, files) {
            totalSize+=f.size();
        }
        while (totalSize > cacheSize && !files.isEmpty()) {
            auto f = files.takeFirst();
            if (f.fileName() == path.split('/').last())
                continue;
            totalSize-=f.size();
            QFile file(f.absoluteFilePath());
            file.remove();
        }
    }
    else {
        if (localPath.endsWith("/"))
            destination = localPath+path.split("/").last();
        else
            destination = localPath;
    }

    SxFileEntry fileEntry;
    if (mCluster->downloadFile(sxVolume, path, destination, fileEntry, 6))
        emit downloadFinished(destination);
    else
        emit downloadFinished("");
}

void OFD_Helper::HelperThread::abort()
{
    emit cancelDownload();
}
