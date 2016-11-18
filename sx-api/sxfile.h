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

#ifndef SXFILE_H
#define SXFILE_H

#include <QObject>
#include <QFile>
#include "sxvolume.h"
#include "sxblock.h"
#include "sxmeta.h"
#include <functional>

class SxCluster;
class SxFilter;

class SxFile
{
public:
    SxFile(SxVolume* volume, const QString &path, const QString &revision, bool localFile);
    SxFile(SxVolume* volume, const QString &path, const QByteArray &salt, const QString& localFile, const int blockSize, const qint64 localSize, std::function<bool()> isAborted=nullptr, bool multipart=false);
    ~SxFile();

    bool haveEqualContent(SxFile& oter) const;
    bool haveEqualContent(qint64 remoteSize, QStringList blocks) const;

    //DEBUG-ONLY FUNCTIONS
    void fakeFile(qint64 fileSize, QStringList blockList, int blockSize);
    void print();
    qint64 remoteSize() const;
    QString revision() const;
    bool multipart() const;

private:
    void clearBlocks();
    QHash<SxBlock*, QList<qint64>> getBlocksOffsets();
    void appendBlock(const QString& hash, const QStringList& nodeList);
    bool readBlock(qint64 offset, int blockSize, char *data);
    bool canReadNextChunk() const;
    bool readNextChunk();

private:
    SxVolume* mVolume;
    QString mLocalPath;
    QString mRemotePath;
    QString mRevision;
    qint64 mLocalSize;
    qint64 mRemoteSize;
    int mCreatedAt;
    int mBlockSize;
    SxMeta mMeta;
    QString mUploadToken;
    QString mUploadPollTarget;
    bool mMultipart;
    std::function<bool()> mIsAbortedCb;
    QByteArray mSalt;

    QHash<QString, SxBlock*> mUniqueBlocks;
    QList<SxBlock*> mBlocks;
    QList<SxBlock*> mBlocksToSend;

    void cryptRemoteName(bool localFile);
    const qint64 cChunkSize = 128*1024*1024;

    QFile mLocalFile;

    friend class SxCluster;
    friend class SxFilter;
};

#endif // SXFILE_H
