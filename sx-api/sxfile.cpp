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

#include "sxfile.h"
#include <QCryptographicHash>
#include <QDebug>
#include "sxfilter.h"
#include "sxlog.h"

SxFile::SxFile(SxVolume *volume, const QString& path, const QString &revision, bool localFile)
{
    mVolume = volume;
    mLocalPath = path;
    mRemotePath = path;
    mRevision = revision;
    mLocalSize = 0;
    mRemoteSize = 0;
    mCreatedAt = 0;
    mBlockSize = 0;
    mMultipart = false;
    mIsAbortedCb = nullptr;
    if (localFile)
        cryptRemoteName(localFile);
}

SxFile::SxFile(SxVolume *volume, const QString &path, const QByteArray& salt, const QString &localFile, const int blockSize, const qint64 localSize, std::function<bool()> isAborted, bool multipart)
{
    mVolume = volume;
    mLocalPath = path;
    mRemotePath = path;
    mRevision = "";
    mCreatedAt = 0;
    mLocalSize =localSize;
    mMultipart = multipart;
    cryptRemoteName(true);
    mLocalFile.setFileName(localFile);
    mIsAbortedCb = isAborted;
    mSalt = salt;

    if (mLocalFile.size() == 0) {
        mRemoteSize = 0;
        mLocalSize = 0;
        return;
    }

    if (mLocalFile.open(QIODevice::ReadOnly)) {
        mRemoteSize = mLocalFile.size();
        if (mRemoteSize <= cChunkSize) {
            mMultipart = false;
        }
        mBlockSize = blockSize;
        QByteArray blockData;

        qint64 offset = 0;
        qint64 readLimit = mMultipart ? cChunkSize : mRemoteSize;

        while(offset + blockSize <= readLimit) {
            if (isAborted != nullptr && isAborted())
                return;
            offset+=blockSize;
            blockData = mLocalFile.read(blockSize);
            if (blockData.size() != blockSize)
                goto onFileReadError;
            QString hash = QString::fromUtf8(SxBlock::hashBlock(blockData,salt));
            appendBlock(hash, QStringList());
        }
        if (readLimit - offset > 0) {
            int size = static_cast<int>(readLimit - offset);
            blockData = mLocalFile.read(size);
            if (blockData.size() != size)
                goto onFileReadError;
            blockData.resize(blockSize);
            for (int i=size; i<blockSize; i++) {
                blockData[i]=0;
            }
            QString hash = QString::fromUtf8(SxBlock::hashBlock(blockData,salt));
            appendBlock(hash, QStringList());
        }
    }
    else {
        logWarning("unable to open file");
        mLocalSize = 0;
        mRemoteSize = 0;
        mBlockSize = 0;
    }
    mLocalFile.close();
    return;
    onFileReadError:
    logWarning("read error");
    mLocalFile.close();
    mLocalSize = 0;
    mRemoteSize = 0;
    mBlockSize = 0;
    return;
}

SxFile::~SxFile()
{
    clearBlocks();
}

bool SxFile::haveEqualContent(qint64 remoteSize, QStringList blocks) const {
    if (mRemoteSize != remoteSize)
        return false;
    if (mBlocks.count() != blocks.count())
        return false;
    for (int i=0; i<mBlocks.count(); i++) {
        auto block = mBlocks.at(i)->mHash;
        if (block != blocks.at(i))
            return false;
    }
    return true;
}

bool SxFile::haveEqualContent(SxFile &other) const
{
    if (mRemoteSize != other.mRemoteSize)
        return false;
    if (mBlocks.count() != other.mBlocks.count())
        return false;
    for (int i=0; i<mBlocks.count(); i++) {
        SxBlock *b1 = mBlocks.at(i);
        SxBlock *b2 = other.mBlocks.at(i);
        if (b1->mHash != b2->mHash)
            return false;
    }
    return true;
}

void SxFile::fakeFile(qint64 fileSize, QStringList blockList, int blockSize)
{
    mLocalSize = mRemoteSize = fileSize;
    foreach (QString block, blockList) {
        SxBlock *b = 0;
        if (mUniqueBlocks.contains(block))
            b = mUniqueBlocks.value(block);
        else
            b = new SxBlock(block, QByteArray(), QStringList());
        mBlocks.append(b);
        mUniqueBlocks.insert(block, b);
    }
    mBlockSize = blockSize;
}

void SxFile::print()
{
    qDebug() << "FILE:" << mLocalPath << mRemotePath;
    qDebug() << "upload token:" << mUploadToken;
    qDebug() << "blocks to send:";
    foreach (SxBlock *block, mBlocksToSend) {
        qDebug() << "----" << block->mHash << block->mNodeList;
    }
}

qint64 SxFile::remoteSize() const
{
    return mRemoteSize;
}

QString SxFile::revision() const
{
    return mRevision;
}

bool SxFile::multipart() const
{
    return mMultipart;
}

void SxFile::clearBlocks()
{
    foreach (SxBlock* block, mUniqueBlocks.values()) {
        mBlocks.removeAll(block);
        mBlocksToSend.removeAll(block);
        delete block;
    }
    mUniqueBlocks.clear();
    Q_ASSERT(mBlocks.isEmpty());
    Q_ASSERT(mBlocksToSend.isEmpty());
}

void SxFile::appendBlock(const QString &hash, const QStringList &nodeList)
{
    SxBlock *block;
    if (mUniqueBlocks.contains(hash))
        block = mUniqueBlocks.value(hash);
    else {
        block = new SxBlock(hash, QByteArray(), nodeList);
        mUniqueBlocks.insert(hash, block);
    }
    mBlocks.append(block);
}

bool SxFile::readBlock(qint64 offset, int blockSize, char* data)
{
    if (!mLocalFile.open(QIODevice::ReadOnly)) {
        logWarning("unable to open file" + mLocalFile.fileName());
        return false;
    }
    if (mLocalFile.size() <= offset) {
        mLocalFile.close();
        return false;
    }
    mLocalFile.seek(offset);
    int toRead;
    if (offset + blockSize > mLocalFile.size())
        toRead = static_cast<int>(mLocalFile.size() - offset);
    else
        toRead = blockSize;

    qint64 readen = mLocalFile.read(data, toRead);

    mLocalFile.close();
    if (readen != toRead) {
        return false;
    }
    for (int i=static_cast<int>(readen); i<blockSize; i++)
        data[i]=0;
    return true;
}

bool SxFile::canReadNextChunk() const
{
    if (!mLocalFile.exists())
        return false;
    return (mLocalSize > mBlocks.count()*static_cast<qint64>(mBlockSize));
}

bool SxFile::readNextChunk()
{
    if (!canReadNextChunk())
        return false;
    if (!mLocalFile.open(QIODevice::ReadOnly)) {
        logWarning("unable to open file" + mLocalFile.fileName());
        return false;
    }

    QByteArray blockData;
    qint64 offset = static_cast<qint64>(mBlockSize)*mBlocks.count();
    if (!mLocalFile.seek(offset)) {
        logWarning("seek failed");
        mLocalFile.close();
        return false;
    }
    qint64 readLimit = qMin(offset+cChunkSize, mRemoteSize);
    while(offset + mBlockSize <= readLimit) {
        if (mIsAbortedCb != nullptr && mIsAbortedCb()) {
            mLocalFile.close();
            return false;
        }
        offset+=mBlockSize;
        blockData = mLocalFile.read(mBlockSize);
        if (blockData.size() != mBlockSize) {
            mLocalFile.close();
            return false;
        }
        QString hash = QString::fromUtf8(SxBlock::hashBlock(blockData,mSalt));
        appendBlock(hash, QStringList());
    }
    if (readLimit - offset > 0) {
        int size = static_cast<int>(readLimit - offset);
        blockData = mLocalFile.read(size);
        if (blockData.size() != size) {
            mLocalFile.close();
            return false;
        }
        blockData.resize(mBlockSize);
        for (int i=size; i<mBlockSize; i++) {
            blockData[i]=0;
        }
        QString hash = QString::fromUtf8(SxBlock::hashBlock(blockData,mSalt));
        appendBlock(hash, QStringList());
    }
    mLocalFile.close();
    return true;
}

void SxFile::cryptRemoteName(bool localFile)
{
    SxFilter* filter = SxFilter::getActiveFilter(mVolume);
    if (filter && filter->filemetaProcess()) {
        filter->filemetaProcess(*this, localFile);
    }
    if (filter)
        delete filter;
}

QHash<SxBlock *, QList<qint64> > SxFile::getBlocksOffsets()
{
    QHash<SxBlock *, QList<qint64>* > tmpList;
    foreach (SxBlock* block, mUniqueBlocks) {
        tmpList.insert(block, new QList<qint64>());
    }
    for (int i=0; i<mBlocks.count(); i++) {
        qint64 offset = i*static_cast<qint64>(mBlockSize);
        SxBlock* block = mBlocks.at(i);
        tmpList.value(block)->append(offset);
    }
    QHash<SxBlock *, QList<qint64> > list;
    foreach (SxBlock* block, mUniqueBlocks) {
        QList<qint64>* nodes = tmpList.take(block);
        list.insert(block, *nodes);
        delete nodes;
    }

    Q_ASSERT(tmpList.isEmpty());
    return list;
}
