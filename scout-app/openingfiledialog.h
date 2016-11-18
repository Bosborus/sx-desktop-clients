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

#ifndef OPENINGFILEDIALOG_H
#define OPENINGFILEDIALOG_H

#include "progressdialog.h"
#include "sxcluster.h"
#include "clusterconfig.h"

class OpeningFileDialog : public ProgressDialog
{
    Q_OBJECT
public:
    OpeningFileDialog(ClusterConfig *config, QWidget *parent = nullptr);
    QString downloadFile(const QString &volume, const QString &path, const QString &localPath, qint64 cacheSize);
private slots:
    void downloadFinished(QString path);
    void setDownloadSize(qint64 size);
    void setDownloadProgress(qint64 size, qint64);
signals:
    void sigDownloadFile(const QString &volume, const QString &path, const QString &localPath, qint64 cacheSize);
private:
    ClusterConfig *mConfig;
    QString mDestination;
    qint64 mDownloadSize;
    QEventLoop mLoop;
};

namespace OFD_Helper {
    class HelperThread : public QObject{
        Q_OBJECT
    public:
        HelperThread(ClusterConfig *config);
    public slots:
        void downloadFile(const QString &volume, const QString &path, const QString &localPath, qint64 cacheSize);
        void abort();
    signals:
        void downloadFinished(QString);
        void sigDownloadSize(qint64);
        void sigSetProgress(qint64, qint64);
        void cancelDownload();
    private:
        ClusterConfig *mConfig;
    };
};

#endif // OPENINGFILEDIALOG_H
