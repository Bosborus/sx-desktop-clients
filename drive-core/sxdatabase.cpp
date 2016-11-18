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

#include <QHash>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <QPair>
#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QMutexLocker>
#include <sxfilter.h>
#include "util.h"


#include "sxfileentry.h"
#include "sxdatabase.h"
#include "sxlog.h"
#include "sxfilesystem.h"

QString SxDatabase::sOldVolumeName;
QHash<QThread*, QString> SxDatabase::sConnections;

SxDatabase &SxDatabase::instance()
{
    static SxDatabase *db = new SxDatabase();
    return *db;
}

void SxDatabase::setOldVolumeName(const QString &volume)
{
    sOldVolumeName = volume;
}

#define printSqlQuery(query) do {                           \
    QString str = query.lastQuery();                        \
    auto map = query.boundValues();                         \
    foreach (auto key, map.keys()) {                        \
        QString value;                                      \
        if (map.value(key).type()==QVariant::String)        \
            value = "\""+map.value(key).toString()+"\"";    \
        else                                                \
            value = map.value(key).toString();              \
        str.replace(key, value);                            \
    }                                                       \
    logDebug(str);                                          \
    } while(0)                                              \

bool SxDatabase::updateVolumes(const QList<const SxVolume *> &list, QHash<QString, QString> &modifiedNames)
{
    modifiedNames.clear();
    QHash<QString, QString> oldNames;
    QSqlQuery query("update sxVolumes set toRemove = 1", getThreadConnection());
    if (!query.exec()) {
        goto onSqlError;
    }

    if (!query.exec("select name, globalId from sxVolumes where globalId is not null")) {
        logWarning(query.lastError().text());
        return false;
    }
    while (query.next()) {
        oldNames.insert(query.value(1).toString(), query.value(0).toString());
    }

    foreach (const SxVolume* volume, list) {
        int filterType = 0;
        if (!volume->meta().value("filterActive").isEmpty()) {
            filterType = SxFilter::isFilterSupported(volume) ? 1 : 2;
        }
        QString oldName;
        if (!volume->globalId().isEmpty()) {
            if (oldNames.contains(volume->globalId()) && oldNames.value(volume->globalId()) != volume->name())
            {
                modifiedNames.insert(oldNames.value(volume->globalId()), volume->name());
                oldName = oldNames.value(volume->globalId());
            }
        }
        if (oldName.isEmpty()) {
            query = QSqlQuery(getThreadConnection());
            query.prepare("select count(*) from sxVolumes where name=:name");
            query.bindValue(":name", volume->name());
            if (!query.exec())
                goto onSqlError;

            if (query.first() && query.value(0).toInt()==1) {
                query = QSqlQuery(getThreadConnection());
                query.prepare("update sxVolumes "
                              "set owner=:owner, size=:size, usedSize=:usedSize, filterType=:filterType, globalId=:globalId, toRemove=0 "
                              "where name=:name");
            }
            else {
                query = QSqlQuery(getThreadConnection());
                query.prepare("insert into sxVolumes (name, owner, size, usedSize, filterType, globalId, toRemove) "
                              "values (:name, :owner, :size, :usedSize, :filterType, :globalId, 0)");
            }
            query.bindValue(":name",    volume->name());
            query.bindValue(":owner",   volume->owner());
            query.bindValue(":size",    volume->size());
            query.bindValue(":usedSize",volume->usedSize());
            query.bindValue(":filterType", filterType);
            query.bindValue(":globalId", volume->globalId().isEmpty() ? QVariant{} : volume->globalId());
            if (!query.exec())
                goto onSqlError;
        }
        else {
            query = QSqlQuery(getThreadConnection());
            query.prepare("update sxVolumes "
                          "set name=:newName, owner=:owner, size=:size, usedSize=:usedSize, "
                          "filterType=:filterType, globalId=:globalId, toRemove=0 "
                          "where name=:oldName");
            query.bindValue(":oldName", oldName);
            query.bindValue(":newName", oldName+"/"+volume->name());
            query.bindValue(":owner",   volume->owner());
            query.bindValue(":size",    volume->size());
            query.bindValue(":usedSize",volume->usedSize());
            query.bindValue(":filterType", filterType);
            query.bindValue(":globalId", volume->globalId().isEmpty() ? QVariant{} : volume->globalId());
            if (!query.exec()) {
                printSqlQuery(query);
                goto onSqlError;
            }
        }
    }
    foreach (QString oldName, modifiedNames.keys()) {
        QString newName = modifiedNames.value(oldName);
        query.prepare("update sxVolumes set name=:newName where name=:tmpName");
        query.bindValue(":newName", newName);
        query.bindValue(":tmpName", oldName+"/"+newName);
        if (!query.exec()) {
            printSqlQuery(query);
            goto onSqlError;
        }
    }

    query = QSqlQuery(getThreadConnection());
    if(!query.exec("delete from sxVolumes where toRemove = 1")) {
        goto onSqlError;
    }
    emit sig_volumeListUpdated();
    return true;
    onSqlError:
    logWarning(query.lastError().text());
    return false;
}

bool SxDatabase::getVolumeList(QList<SxVolumeEntry> &volumeList) const
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    QString queryString = "select name, size, usedSize, filterType from sxVolumes order by name";
    if (!query.exec(queryString))
        return false;
    volumeList.clear();
    while (query.next()) {
        QString name = query.value(0).toString();
        qint64 size = query.value(1).toLongLong();
        qint64 usedSize = query.value(2).toLongLong();
        bool filterType = query.value(3).toInt();
        volumeList.append(SxVolumeEntry(name, usedSize, size, filterType));
    }
    return true;
}

bool SxDatabase::startUpdatingFiles(std::function<bool()> abortedCB)
{
    mMutex.lock();
    mStartTime = QDateTime::currentDateTime();
    mAbortedCB = abortedCB;
    return true;
}

bool SxDatabase::endUpdatingFiles()
{
    mAbortedCB = nullptr;

    QSqlQuery query(getThreadConnection());
    if (!query.exec("update sxFiles set action=0")) {
        logWarning(query.lastError().text());
    }
    if (!mSuppressedFiles.isEmpty()) {
        foreach (auto file, mSuppressedFiles) {
            int index = file.indexOf("/");
            if (index == -1)
                continue;
            QString volume = file.mid(0, index);
            QString path = file.mid(index+1);
            QSqlQuery q(getThreadConnection());
            q.prepare("update sxFiles set action=1 where volume=:volume and path=:path");
            q.bindValue(":volume", volume);
            q.bindValue(":path", path);
            if (!q.exec()) {
                logWarning(q.lastError().text());
            }
        }
    }
    if (!query.exec("delete from sxFiles where localRevision is null and remoteRevision is null and action=0")) {
        logWarning(query.lastError().text());
    }
    logDebug(QString("lock time: %1").arg(formatEta(mStartTime.secsTo(QDateTime::currentDateTime()))));
    mMutex.unlock();
    return true;
}

bool SxDatabase::updateRemoteFiles(const QString &volume, const QList<SxFileEntry *> &list)
{
    QSqlQuery q(getThreadConnection());
    if (!q.exec("begin transaction"))
        return false;
    foreach (SxFileEntry* fileEntry, list) {
        if (mAbortedCB!=nullptr && mAbortedCB()) {
            q.exec("rollback transaction");
            return false;
        }
        static const QStringList forbiddenList = {
            #ifdef Q_OS_WIN
            "\\", ":", "*", "?", "<", ">", "|",
            #endif
            "/../", "/./"
        };

        static QStringList ignoredNames = {".", "..", ".DS_Store", "._.DS_Store"};
        if (ignoredNames.contains(fileEntry->path().split("/").last())) {
            continue;
        }
        bool skipFile = false;

        foreach (QString forbidden, forbiddenList) {
            if (fileEntry->path().contains(forbidden)) {
                skipFile = true;
            }
        }
        if (skipFile)
            continue;

        QSqlQuery query(getThreadConnection());
        query.prepare("select localRevision, remoteSize, mTime from sxFiles where volume=:volume and path=:path");
        query.bindValue(":volume", volume);
        query.bindValue(":path", fileEntry->path());
        //printSqlQuery(query);
        if (!query.exec()) {
            logWarning(query.lastError().text());
            q.exec("rollback transaction");
            return false;
        }
        if (query.first()) {
            ACTION action = ACTION::SKIP;
            if (!mSuppressedFiles.contains(volume+"/"+fileEntry->path())) {
                if (query.isNull(0)) {
                    action = query.isNull(2) ? ACTION::DOWNLOAD : ACTION::UPLOAD;
                }
                else {
                    QString localRevision = query.value(0).toString();
                    if (fileEntry->revision() > localRevision)
                        action = ACTION::DOWNLOAD;
                }
            }
            query = QSqlQuery(getThreadConnection());
            query.prepare("update sxFiles set remoteRevision=:revision, remoteSize=:size, action=:action where volume=:volume and path=:path");
            query.bindValue(":action", static_cast<int>(action));
        }
        else {
            query = QSqlQuery(getThreadConnection());
            query.prepare("insert into sxFiles (volume, path, remoteRevision, remoteSize, action) values (:volume, :path, :revision, :size, :action)");
            ACTION action = mSuppressedFiles.contains(volume+"/"+fileEntry->path()) ? ACTION::SKIP : ACTION::DOWNLOAD;
            query.bindValue(":action", static_cast<int>(action));
        }
        query.bindValue(":volume", volume);
        query.bindValue(":path", fileEntry->path());
        query.bindValue(":revision", fileEntry->revision());
        query.bindValue(":size", fileEntry->size());
        //printSqlQuery(query);
        if (!query.exec()) {
            logWarning(query.lastError().text());
            q.exec("rollback transaction");
            return false;
        }
    }
    q.exec("commit transaction");
    return  true;
}

bool SxDatabase::updateLocalFiles(const QString &volume, const QList<QString> &list, const QDir &volumeRootDir)
{
    QSqlQuery q(getThreadConnection());
    if (!q.exec("begin transaction"))
        return false;

    foreach (auto file, list) {
        QDateTime time = QDateTime::currentDateTime();
        if (mAbortedCB!=nullptr && mAbortedCB()) {
            q.exec("rollback transaction");
            return false;
        }
        QFileInfo fileInfo(volumeRootDir.absolutePath()+file);
        if (!fileInfo.exists() || !fileInfo.isFile())
            continue;
        static QStringList ignoredNames = {".DS_Store", "._.DS_Store"};
        if (ignoredNames.contains(file.split("/").last()))
            continue;

        QSqlQuery query(getThreadConnection());
        query.prepare("select mTime from sxFiles where volume=:volume and path=:path");
        query.bindValue(":volume", volume);
        query.bindValue(":path", file);
        //printSqlQuery(query);
        if (!query.exec()) {
            logWarning(query.lastError().text());
            q.exec("rollback transaction");
            return false;
        }
        QDateTime time2 = QDateTime::currentDateTime();
        bool needUpload = false;
        QString queryString;
        if (query.first()) {
            if (query.isNull(0))
                needUpload = true;
            else {
                uint mTimeDb = query.value(0).toUInt();
                uint mTimeFile = fileInfo.lastModified().toTime_t();
                if (mTimeDb != mTimeFile)
                    needUpload = true;
            }
            queryString = "update sxFiles set ";
            if (needUpload)
                queryString += "localRevision=NULL, ";
            queryString += "action=:action, mTime=:mTime where volume=:volume and path=:path";
            if (!needUpload)
                queryString += " and action!=:removeAction";
        }
        else {
            needUpload = true;
            queryString = "insert into sxFiles (volume, path, action, mTime) values (:volume, :path, :action, :mTime)";
        }
        query = QSqlQuery(getThreadConnection());
        query.prepare(queryString);
        query.bindValue(":action", static_cast<int>(needUpload ? ACTION::UPLOAD : ACTION::SKIP));
        query.bindValue(":removeAction", static_cast<int>(ACTION::REMOVE_LOCAL));
        query.bindValue(":volume", volume);
        query.bindValue(":path", file);
        query.bindValue(":mTime", fileInfo.lastModified().toTime_t());
        //printSqlQuery(query);
        if (!query.exec()) {
            logWarning(query.lastError().text());
            q.exec("rollback transaction");
            return false;
        }
    }
    q.exec("commit transaction");
    return true;
}

bool SxDatabase::updateLocalDirs(const QString &volume, const QList<QString> &list, const QDir &volumeRootDir)
{
    foreach (auto dir, list) {
        if (mAbortedCB!=nullptr && mAbortedCB())
            return false;
        QSqlQuery query(getThreadConnection());
        query.prepare("select count(*) from sxFiles where volume=:volume and path like :path");
        query.bindValue(":volume", volume);
        query.bindValue(":path", dir+"/%");
        //printSqlQuery(query);
        if (!query.exec()) {
            logWarning(query.lastError().text());
            return false;
        }
        query.first();
        if (query.value(0).toInt()==0) {
            QDir targetDir(volumeRootDir.absolutePath()+"/"+dir);
            auto files = SxFilesystem::getDirectoryContents(targetDir, true, dir);
            foreach (QString file, files) {
                if (mAbortedCB && mAbortedCB())
                    return false;
                query = QSqlQuery(getThreadConnection());
                query.prepare("insert into sxFiles (volume, path, action) values (:volume, :path, :action)");
                query.bindValue(":volume", volume);
                query.bindValue(":path", file);
                query.bindValue(":action", static_cast<int>(ACTION::UPLOAD));
                if (!query.exec()) {
                    logWarning(query.lastError().text());
                    return false;
                }
            }
        }
        else {
            query = QSqlQuery(getThreadConnection());
            query.prepare("update sxFiles set action=0 where volume=:volume and path like :path");
            query.bindValue(":volume", volume);
            query.bindValue(":path", dir+"/%");
            if (!query.exec()) {
                logWarning(query.lastError().text());
                return false;
            }
        }
    }
    return true;
}

QList<QString> SxDatabase::getMarkedFiles(const QString &volume, ACTION action) const
{
    QList<QString> result;
    QSqlQuery query(getThreadConnection());
    query.prepare("select path from sxFiles where volume=:volume and action=:action");
    query.bindValue(":volume", volume);
    query.bindValue(":action", static_cast<int>(action));
    //printSqlQuery(query);
    if (query.exec()) {
        while (query.next()) {
            QString path = query.value(0).toString();
            if (mSuppressedFiles.contains(volume+"/"+path))
                continue;
            static QStringList ignoredNames = {".DS_Store", "._.DS_Store"};
            if (ignoredNames.contains(path.split("/").last()))
                continue;
            result.append(path);
        }
    }
    return result;
}

bool SxDatabase::getLocalFileMtime(const QString &volume, const QString &path, uint32_t &mtime) const
{
    QSqlQuery query(getThreadConnection());
    query.prepare("select mtime from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path);
    if (!query.exec()) {
        logError(query.lastError().text());
        return false;
    }
    if (!query.first())
        return false;
    mtime = query.value(0).toUInt();
    return true;
}

bool SxDatabase::isLocalDir(const QString &volume, const QString &path)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("select count(*) from sxFiles where volume=:volume and path like :path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path+"/%");
    if (!query.exec() || !query.first()) {
        logError(query.lastError().text());
        return false;
    }
    int count = query.value(0).toInt();
    return count > 0;
}

void SxDatabase::onFileUploaded(const QString &volume, const SxFileEntry &fileEntry, bool _registerEvent)
{
    logDebug(QString("%1/%2:%3 [%4]").arg(volume).arg(fileEntry.path()).arg(fileEntry.revision()).arg(_registerEvent));
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());

    query.prepare("select count(*) from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", fileEntry.path());
    //printSqlQuery(query);
    if (!query.exec() || !query.first()) {
        logWarning(query.lastError().text());
        return;
    }
    QString queryString;
    if (query.value(0).toInt() == 0) {
        queryString = "insert into sxFiles (volume, path, localRevision, mTime, remoteSize, action, blockSize) values (:volume, :path, :revision, :mTime, :size, 0, :blockSize)";
    }
    else {
        queryString = "update sxFiles set localRevision=:revision, mTime=:mTime, remoteSize=:size, action=0, blockSize=:blockSize where volume=:volume and path=:path";
    }
    query.prepare(queryString);
    query.bindValue(":volume", volume);
    query.bindValue(":path", fileEntry.path());
    query.bindValue(":size", fileEntry.size());
    query.bindValue(":revision", fileEntry.revision().isEmpty() ? QString::null : fileEntry.revision());
    query.bindValue(":mTime",    fileEntry.revision().isEmpty() ? QVariant{}    : fileEntry.createdTime());
    query.bindValue(":blockSize", fileEntry.blockSize());
    //printSqlQuery(query);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        printSqlQuery(query);
        return;
    }
    updateFileBlocks(volume, fileEntry);
    if (_registerEvent)
        registerEvent(volume, fileEntry.path(), ACTION::UPLOAD);
}

void SxDatabase::onFileUploaded(const QString &volume, const QString &path, QString rev, quint32 mTime)
{
    logDebug(QString("%1/%2:%3").arg(volume).arg(path).arg(rev));
    QSqlQuery query(getThreadConnection());
    query.prepare("update sxFiles set localRevision=:revision, mTime=:mTime where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path);
    query.bindValue(":revision", rev.isEmpty() ? QString::null : rev);
    query.bindValue(":mTime", rev.isEmpty() ? QVariant{} : mTime);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        printSqlQuery(query);
        return;
    }
    registerEvent(volume, path, ACTION::UPLOAD);
}

void SxDatabase::onFileDownloaded(const QString &volume, const SxFileEntry &fileEntry)
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("update sxFiles set localRevision=:revision, mTime=:mTime, remoteSize=:size, action=0, blockSize=:blockSize where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", fileEntry.path());
    query.bindValue(":size", fileEntry.size());
    query.bindValue(":revision", fileEntry.revision().isEmpty() ? QString::null : fileEntry.revision());
    query.bindValue(":mTime", fileEntry.revision().isEmpty() ? QVariant{} : fileEntry.createdTime());
    query.bindValue(":blockSize", fileEntry.blockSize());
    //printSqlQuery(query);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return;
    }
    updateFileBlocks(volume, fileEntry);
    registerEvent(volume, fileEntry.path(), ACTION::DOWNLOAD, fileEntry.revision());
    int count;
    if (testHistoryRevision(volume, fileEntry.path(), fileEntry.revision(), count)) {
        if (count != 1)
            emit sig_possibleInconsistencyDetected(volume, fileEntry.path());
        else
            updateInconsistentFile(volume, fileEntry.path(), {});
    }
}

void SxDatabase::onRemoteFileRemoved(const QString &volume, const QString &file)
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", file);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return;
    }
    registerEvent(volume, file, ACTION::REMOVE_REMOTE);
}

void SxDatabase::onLocalFileRemoved(const QString &volume, const QString &file)
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", file);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return;
    }
    removeFileBlocks(volume, file);
    registerEvent(volume, file, ACTION::REMOVE_LOCAL);
}

bool SxDatabase::getHistoryRowIds(QList<qint64>& list) const
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare(QString("SELECT rowId FROM sxHistory ORDER BY eventDate DESC, rowId DESC limit %1").arg(sShowHistoryLimit));
    if (!query.exec())
        return false;
    list.clear();
    while (query.next()) {
        list.push_back(query.value(0).toLongLong());
    }
    return true;
}

bool SxDatabase::getHistoryEntry(qint64 rowId, QString &path, uint32_t &eventDate, ACTION &eventType) const
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("SELECT volume, path, eventDate, action FROM sxHistory WHERE rowId=?");
    query.addBindValue(rowId);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return false;
    }
    if (query.next()) {
        path = query.value(0).toString() + query.value(1).toString();
        eventDate = query.value(2).toUInt();
        eventType = static_cast<ACTION>(query.value(3).toInt());
    }
    return true;
}

qint64 SxDatabase::getRemoteFileSize(const QString &volume, const QString &path)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("select remoteSize from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path);
    if (!query.exec() || !query.first())
        return 0;
    return query.value(0).toLongLong();
}

QString SxDatabase::getRemoteFileRevision(const QString &volume, const QString &path)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("select remoteRevision from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path);
    if (!query.exec() || !query.first())
        return "";
    return query.value(0).toString();
}

bool SxDatabase::removeVolumeFiles(const QString &volume)
{
    QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxFiles where volume=:volume");
    query.bindValue(":volume", volume);
    return query.exec();
}

bool SxDatabase::removeVolumeHistory(const QString &volume)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxHistory where volume=:volume");
    query.bindValue(":volume", volume);
    return query.exec();
}

QList<QPair<QString, QString>> SxDatabase::getRecentHistory(bool shareHistory, int limit) const
{
    QList<QPair<QString, QString>> result;
    QString queryString = "select h.volume, h.path from sxHistory h";
    if (!shareHistory)
        queryString += " where action = 3 or action = 4 ";
    else {
        queryString += ", sxVolumes v where action = 4 and h.volume=v.name and v.filterType=0 ";
    }
    queryString += "ORDER BY h.eventDate DESC, h.rowId DESC limit "+QString::number(limit);
    QSqlQuery query(getThreadConnection());
    if (!query.exec(queryString)) {
        logWarning(query.lastError().text());
        return result;
    }
    while (query.next()) {
        result.insert(0, {query.value(0).toString(),query.value(1).toString()});
    }
    return result;
}

void SxDatabase::updateFileBlocks(const QString &volume, const SxFileEntry &fileEntry)
{
    auto time1 = QDateTime::currentDateTime();
    QMutexLocker locker(&mMutex);
    auto time2 = QDateTime::currentDateTime();
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxBlocks where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", fileEntry.path());
    if (!query.exec()) {
        logWarning(query.lastError().text());
        printSqlQuery(query);
        return;
    }
    auto time3 = QDateTime::currentDateTime();
    auto blocks = fileEntry.blocks();
    int i;
    qint64 offset;
    query = QSqlQuery(getThreadConnection());
    if (!query.exec("begin immediate transaction"))
        return;
    for (i=0, offset=0; i<blocks.count(); i++, offset+=fileEntry.blockSize()) {
        query = QSqlQuery(getThreadConnection());
        query.prepare("insert into sxBlocks (volume, path, offset, blockSize, hash) values (:volume, :path, :offset, :blockSize, :hash)");
        query.bindValue(":volume", volume);
        query.bindValue(":path", fileEntry.path());
        query.bindValue(":offset", offset);
        query.bindValue(":blockSize", fileEntry.blockSize());
        query.bindValue(":hash", blocks.at(i));
        if (!query.exec()) {
            logWarning(query.lastError().text());
            query = QSqlQuery(getThreadConnection());
            query.exec("rollback transaction");
            return;
        }
    }
    query = QSqlQuery(getThreadConnection());
    query.exec("commit transaction");
    auto time4 = QDateTime::currentDateTime();
    if (time1.msecsTo(time4) > 1000) {
        auto line = QString("lock: %1ms, delete: %2ms, insert: %3ms (count: %4)")
                .arg(time1.msecsTo(time2))
                .arg(time2.msecsTo(time3))
                .arg(time3.msecsTo(time4))
                .arg(blocks.count());
        logWarning(line);
    }
}

void SxDatabase::removeFileBlocks(const QString &volume, const QString &path)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxBlocks where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        printSqlQuery(query);
    }
}

bool SxDatabase::findBlock(const QString &hash, int blockSize, QList<std::tuple<QString, QString, qint64> > &result)
{
    result.clear();
    QSqlQuery query(getThreadConnection());
    query.prepare("select volume, path, offset from sxBlocks where "
                  "blockSize = :blockSize and hash = :hash");
    query.bindValue(":blockSize", blockSize);
    query.bindValue(":hash", hash);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        printSqlQuery(query);
        return false;
    }
    while (query.next())
        result.append(std::make_tuple(query.value(0).toString(), query.value(1).toString(), query.value(2).toLongLong()));
    return true;
}

bool SxDatabase::findIdenticalFiles(const QString &volume, qint64 remoteFileSize, int blockSize, const QStringList &blocks, QList<QPair<QString, quint32> > &result)
{
    result.clear();
    QSqlQuery query(getThreadConnection());
    query.prepare("select f.path, f.mTime from sxBlocks b, sxFiles f where "
                  "b.volume=f.volume and b.path=f.path and "
                  "b.volume=:volume and f.remoteSize=:remoteSize and "
                  "b.offset=0 and b.hash=:hash and b.blockSize=:blockSize");
    query.bindValue(":volume", volume);
    query.bindValue(":remoteSize", remoteFileSize);
    query.bindValue(":hash", blocks.first());
    query.bindValue(":blockSize", blockSize);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        printSqlQuery(query);
        return false;
    }
    QHash<QString, quint32> files;
    while (query.next()) {
        files.insert(query.value(0).toString(), query.value(1).toUInt());
    }
    foreach (QString file, files.keys()) {
        query = QSqlQuery(getThreadConnection());
        query.prepare("select offset, hash from sxBlocks where "
                      "volume=:volume and path=:path "
                      "order by offset");
        query.bindValue(":volume", volume);
        query.bindValue(":path", file);
        if (!query.exec()) {
            logWarning(query.lastError().text());
            printSqlQuery(query);
            continue;
        }
        qint64 offset = 0;
        int counter=0;
        while (query.next()) {
            if (query.value(0).toLongLong() != offset)
                goto nextLoop;
            if (query.value(1).toString() != blocks.at(counter))
                goto nextLoop;
            ++counter;
            offset+=blockSize;
        }
        if (counter != blocks.size())
            continue;
        result.append({file, files.value(file)});
        nextLoop:
        continue;
    }
    return true;
}

void SxDatabase::addSuppression(const QString &volume, const QString &path)
{
    logDebug(QString("%1/%2").arg(volume).arg(path));
    mSuppressedFiles.insert(volume+"/"+path);
}

void SxDatabase::removeSuppression(const QString &volume, const QString &path)
{
    logDebug(QString("%1/%2").arg(volume).arg(path));
    mSuppressedFiles.remove(volume+"/"+path);
}

bool SxDatabase::getFilesCount(const QString &volume, bool localFiles, quint32 &count)
{
    QSqlQuery query(getThreadConnection());
    query.prepare(QString("select count(*) from sxFiles where volume=:volume and ") +
                  (localFiles ? "localRevision not null" : "remoteRevision not null"));
    query.bindValue(":volume", volume);
    if (!query.exec()) {
        return false;
    }
    if (!query.first())
        return false;
    count = query.value(0).toUInt();
    return true;
}

bool SxDatabase::testHistoryRevision(const QString &volume, const QString &file, const QString &revision, int &count)
{
    if (volume.isEmpty() || file.isEmpty() || revision.isEmpty())
        return false;
    QSqlQuery query(getThreadConnection());
    query.prepare("select count(*) from sxHistory where volume=:volume and path=:path and revision=:rev");
    query.bindValue(":volume", volume);
    query.bindValue(":path", file);
    query.bindValue(":rev", revision);
    if (!query.exec()) {
        return false;
    }
    if (!query.first())
        return false;
    count = query.value(0).toInt();
    return true;
}

bool SxDatabase::updateInconsistentFile(const QString &volume, const QString &file, const QStringList &revisions)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxInconsistentFiles where volume=:volume and path=:file");
    query.bindValue(":volume", volume);
    query.bindValue(":file", file);
    if (!query.exec())
        return false;
    foreach (QString rev, revisions) {
        query = QSqlQuery(getThreadConnection());
        query.prepare("insert into sxInconsistentFiles (volume, path, revision) values (:volume, :file, :revision)");
        query.bindValue(":volume", volume);
        query.bindValue(":file", file);
        query.bindValue(":revision", rev);
        if (!query.exec())
            return false;
    }
    return true;
}

bool SxDatabase::getInconsistentFile(const QString &volume, const QString &file, QStringList &revisions)
{
    QSqlQuery query(getThreadConnection());
    query.prepare("select (revision) from sxInconsistentFiles where volume=:volume and path=:file");
    query.bindValue(":volume", volume);
    query.bindValue(":file", file);
    if (!query.exec())
        return false;
    revisions.clear();
    while (query.next()) {
        revisions.append(query.value(0).toString());
    }
    return true;
}

QSqlDatabase SxDatabase::getThreadConnection()
{
    QThread *t = QThread::currentThread();
    if (sConnections.contains(t))
        return QSqlDatabase::database(sConnections.value(t));

    static QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!cacheDir.mkpath(".")) {
        throw std::runtime_error("Unable to create cache directory");
    }
    static QString dbFile = cacheDir.absoluteFilePath("sxsync.db");

    QString connectionName = QString("sxdrive_%1").arg(reinterpret_cast<quintptr>(t));
    QSqlDatabase connection = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    connection.setDatabaseName(dbFile);
    if (!connection.open()) {
        throw std::runtime_error("Cannot open the database");
    }
    sConnections.insert(t, connectionName);
    connect(t, &QThread::finished, [t]() {
        if (sConnections.contains(t)) {
            QSqlDatabase::removeDatabase(sConnections.value(t));
            sConnections.remove(t);
        }
    });
    return connection;
}

void SxDatabase::registerEvent(const QString &volume, const QString &path, SxDatabase::ACTION action, const QString &revision)
{
    static const QStringList ignoredFiles = {".sxnewdir", ".DS_Store", "._.DS_Store"};
    if (ignoredFiles.contains(path.split("/").last()))
        return;
    QSqlQuery query(getThreadConnection());
    query.prepare("insert into sxHistory (volume, path, revision, action, eventDate) values (:volume, :path, :revision, :action, :eventDate)");
    query.bindValue(":volume", volume);
    query.bindValue(":path", path);
    query.bindValue(":revision", revision.isEmpty() ? QVariant{} : revision);
    query.bindValue(":action", static_cast<int>(action));
    query.bindValue(":eventDate", QDateTime::currentDateTime().toTime_t());
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return;
    }
    qint64 rowId = -1;
    qint64 removeId = -1;

    if (query.exec("SELECT last_insert_rowid()") && query.first()) {
        rowId = query.value(0).toLongLong();
    }
    if (query.exec(QString("select min(rowId) from (select rowId from sxHistory order by eventDate DESC, rowId DESC limit %1)").arg(sHistoryLimit)) && query.first()) {
        removeId = query.value(0).toUInt();
        query = QSqlQuery(getThreadConnection());
        query.prepare("delete from sxHistory where rowId < :rowId");
        query.bindValue(":rowId", removeId);
        if (!query.exec()) {
            logWarning(query.lastError().text());
        }
    }
    if (rowId != -1) {
        if (rowId-sShowHistoryLimit > removeId ) {
            removeId = rowId-sShowHistoryLimit;
        }
        emit sig_historyChanged(rowId, removeId);
    }
}

bool SxDatabase::markVolumeFilesToRemove(const QString &volume, bool removeRemote, bool onlySkipped)
{
    QSqlQuery q(getThreadConnection());
    if (!q.exec("begin transaction"))
        return false;
    QSqlQuery query(getThreadConnection());
    QString queryString = "update sxFiles set action=:action where volume= :volume";
    if (onlySkipped) {
        queryString += " and action=:actionSkip";
    }
    query.prepare(queryString);
    query.bindValue(":volume", volume);
    query.bindValue(":action", static_cast<int>(removeRemote ? ACTION::REMOVE_REMOTE : ACTION::REMOVE_LOCAL));
    if (onlySkipped) {
        query.bindValue(":actionSkip", static_cast<int>(ACTION::SKIP));
    }
    //printSqlQuery(query);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        q.exec("rollback transaction");
        return false;
    }
    q.exec("commit transaction");
    return true;
}

bool SxDatabase::markLocalDirFilesToRemove(const QString &volume, const QString &dir, bool onlyExisting)
{
    QSqlQuery q(getThreadConnection());
    if (!q.exec("begin transaction"))
        return false;
    QSqlQuery query(getThreadConnection());
    if(!query.exec("update sxFiles set action=0")) {
        logWarning(query.lastError().text());
        q.exec("rollback transaction");
        return false;
    }
    QString queryString = "update sxFiles set action=:action where volume=:volume and path like :path";
    if (onlyExisting)
        queryString += " and localRevision not null";
    query.prepare(queryString);
    query.bindValue(":volume", volume);
    query.bindValue(":action", static_cast<int>(ACTION::REMOVE_REMOTE));
    if (dir.endsWith("/"))
        query.bindValue(":path", dir+"%");
    else
        query.bindValue(":path", dir+"/%");
    //printSqlQuery(query);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        q.exec("rollback transaction");
        return false;
    }
    q.exec("commit transaction");
    return true;
}

bool SxDatabase::remoteFileExists(const QString &volume, const QString &file)
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("select remoteRevision from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", file);
    //printSqlQuery(query);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return false;
    }
    if (!query.first())
        return false;
    if (query.isNull(0))
        return false;
    return true;
}

bool SxDatabase::dropFileEntry(const QString &volume, const QString &file)
{
    //QMutexLocker lock(&mMutex);
    QSqlQuery query(getThreadConnection());
    query.prepare("delete from sxFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", file);
    //printSqlQuery(query);
    if (!query.exec()) {
        logWarning(query.lastError().text());
        return false;
    }
    return query.numRowsAffected() > 0;
}

SxDatabase::SxDatabase() : QObject(nullptr)
{
    mAbortedCB = nullptr;
    setupTables();
}

static const QHash<QString, QPair<int,QString>> createTableQueries = {
    {"sxVolumes",   {2, "create table if not exists sxVolumes "
                     "(name text primary key, owner text not null, "
                     "size integer not null, usedSize integer not null, "
                     "filterType integer not null, globalId text, "
                     "toRemove integer not null default 0)"}},
    {"sxFiles",     {2, "create table if not exists sxFiles "
                     "(volume text references sxVolumes(name) on delete cascade on update cascade, path text not null, "
                     "remoteRevision text, localRevision text, mTime integer, "
                     "remoteSize integer, action integer not null default 0, "
                     "blockSize integer, "
                     "primary key(volume, path))"}},
    {"sxInconsistentFiles", {1, "create table if not exists sxInconsistentFiles"
                             "(volume text not null references sxVolumes(name) on delete cascade on update cascade, "
                             "path text not null, revision text not null,"
                             "primary key (volume, path, revision))"}},
    {"sxHistory",   {2, "create table if not exists sxHistory "
                     "(volume text references sxVolumes(name) on delete cascade on update cascade, path text, revision text, "
                     "action integer not null, eventDate integer not null)"}},
    {"sxBlocks",    {1, "create table if not exists sxBlocks "
                     "(volume text not null, path text not null, blockSize integer not null, offset integer not null, hash text not null, "
                     "foreign key (volume, path) references sxFiles(volume, path) on delete cascade on update cascade, "
                     "primary key (volume, path, offset))"
    }}
};

void SxDatabase::report_error(const QString &msg, const QSqlError &error) const {
    throw std::runtime_error(msg.toStdString() + ": " + error.text().toStdString());
}


void SxDatabase::setupTables()
{
    QSqlQuery query(getThreadConnection());
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA synchronous=NORMAL");
    query.exec("PRAGMA foreign_keys = ON");

    if (!query.exec("create table if not exists sxTables (name text primary key, version integer not null)")) {
        report_error("Failed to create table sxTables", query.lastError());
    }

    struct OldFileEntry{
        OldFileEntry(QString path, QString rev, uint mtime) {
            this->path = path;
            this->rev = rev;
            this->mtime = mtime;
        }
        QString path;
        QString rev;
        uint mtime;
    };
    QList<OldFileEntry> files;

    if (!sOldVolumeName.isEmpty() && query.exec("select l.path, r.rev, l.mtime from local l, remote r where l.path=r.path")) {
        while (query.next()) {
            QString path = query.value(0).toString();
            QString rev = query.value(1).toString();
            uint mtime = query.value(2).toUInt();
            files.append(OldFileEntry(path, rev, mtime));
        }
    }

    query.exec("drop if exists local");
    query.exec("drop if exists remote");
    query.exec("drop if exists syncState");
    query.exec("drop if exists history");

    auto sxTables = tables();
    static const QStringList tableList{"sxVolumes", "sxFiles", "sxHistory", "sxInconsistentFiles", "sxBlocks"};
    foreach (QString table, tableList) {
        if (sxTables.contains(table))
            updateSxTable(table, sxTables.value(table));
        else {
            int version = createTableQueries.value(table).first;
            QString createString = createTableQueries.value(table).second;
            QString insertString = QString("insert into sxTables (name, version) values (\"%1\", %2)").arg(table).arg(version);

            if (!query.exec(createString)) {
                printSqlQuery(query);
                report_error("Failed to create table "+table, query.lastError());
            }
            if (!query.exec(insertString))
                report_error("Failed to register table "+table, query.lastError());
        }
    }

    if (!query.exec("create index if not exists sxBlocks_index on sxBlocks (blockSize, hash)"))
        report_error("Failed to create index sxBlocks_index", query.lastError());
    if (!query.exec("create index if not exists sxFiles_action_index on sxFiles (action)"))
        report_error("Failed to create index sxFiles_action_index", query.lastError());
    if (!query.exec("create index if not exists sxInconsistentFiles_index on sxInconsistentFiles (volume, path)"))
        report_error("Failed to create index sxFiles_action_index", query.lastError());

    if (!sOldVolumeName.isEmpty()) {
        query = QSqlQuery(getThreadConnection());
        query.prepare("insert into sxVolumes (name, owner, size, usedSize, filterType, toRemove) values (:name, :owner, :size, :usedSize, :filterType, 0)");
        query.bindValue(":name",    sOldVolumeName);
        query.bindValue(":owner",   "unknown");
        query.bindValue(":size",     0);
        query.bindValue(":usedSize", 0);
        query.bindValue(":filterType", 0);
        query.exec();
        foreach (auto entry, files) {
            query = QSqlQuery(getThreadConnection());
            query.prepare("insert into sxFiles (volume, path, localRevision, mTime) values (:volume, :path, :rev, :mTime)");
            query.bindValue(":volume", sOldVolumeName);
            query.bindValue(":path", entry.path);
            query.bindValue(":rev", entry.rev);
            query.bindValue(":mTime", entry.mtime);
            if (!query.exec()) {
                break;
            }
        }
    }
}

QHash<QString, int> SxDatabase::tables()
{
    QHash<QString, int> result;
    QSqlQuery query(getThreadConnection());
    if (!query.exec("select name, version from sxTables")) {
        logWarning(query.lastError().text());
        return result;
    }
    while (query.next()) {
        QString table = query.value(0).toString();
        int version = query.value(1).toInt();
        result.insert(table,version);
    }
    return result;
}

void SxDatabase::updateSxTable(QString table, int fromVersion)
{
    if (createTableQueries.value(table).first == fromVersion)
        return;
    if (table == "sxVolumes") {
        QSqlQuery query(getThreadConnection());
        switch (fromVersion) {
        case 0: {
            QString createString = createTableQueries.value(table).second;
            QString updateString = QString("update sxTables set version=%2 where name=\"%1\"")
                    .arg(table).arg(1);
            QString insertString = "insert into sxVolumes (name, owner, size, usedSize, filterType, toRemove) "
                                   " values select name, owner, size, usedSize, 0, 0 from sxVolumes_old";
            QString dropString = "drop table sxVolumes_old";
            query.exec("PRAGMA foreign_keys = OFF");
            query.exec(dropString);
            if (!query.exec("alter table sxVolumes rename to sxVolumes_old"))
                goto onError;
            if (!query.exec(createString))
                goto onError;
            if (!query.exec(updateString))
                goto onError;
            if (!query.exec(insertString))
                goto onError;
            if (!query.exec(dropString))
                goto onError;
            query.exec("PRAGMA foreign_keys = ON");
        }
        case 1: {
            QString updateString = QString("update sxTables set version=%2 where name=\"%1\"")
                    .arg(table).arg(2);
            QString alterString = "alter table sxVolumes add column globalId text";
            if (!query.exec(alterString))
                goto onError;
            if (!query.exec(updateString))
                goto onError;
        }
        }
        return;
        onError:
        logWarning(query.lastError().text());
        throw std::runtime_error("Altering table sxVolumes failed");
    }
    if (table == "sxFiles") {
        QString createString = createTableQueries.value(table).second;
        QString dropString = "drop table sxFiles";
        QSqlQuery query(getThreadConnection());
        QString updateString = QString("update sxTables set version=%2 where name=\"%1\"")
                .arg(table).arg(createTableQueries.value(table).first);
        query.exec("PRAGMA foreign_keys = OFF");
        if (!query.exec(dropString))
            goto onErrorSxFiles;
        if (!query.exec(createString))
            goto onErrorSxFiles;
        if (!query.exec(updateString))
            goto onErrorSxFiles;
        query.exec("PRAGMA foreign_keys = ON");
        return;
        onErrorSxFiles:
        logWarning(query.lastError().text());
        throw std::runtime_error("Altering table sxFiles failed");
    }
    if (table == "sxBlocks") {
        QString createString = createTableQueries.value(table).second;
        QString dropString = "drop table sxBlocks";
        QSqlQuery query(getThreadConnection());
        QString updateString = QString("update sxTables set version=%2 where name=\"%1\"")
                .arg(table).arg(createTableQueries.value(table).first);
        query.exec("PRAGMA foreign_keys = OFF");
        if (!query.exec(dropString))
            goto onSxBlocks;
        if (!query.exec(createString))
            goto onSxBlocks;
        if (!query.exec(updateString))
            goto onSxBlocks;
        query.exec("PRAGMA foreign_keys = ON");
        return;
        onSxBlocks:
        logWarning(query.lastError().text());
        throw std::runtime_error("Altering table sxFiles failed");
    }
    if (table == "sxHistory") {
        QString createString = createTableQueries.value(table).second;
        QString dropString = "drop table sxHistory";
        QSqlQuery query(getThreadConnection());
        QString updateString = QString("update sxTables set version=%2 where name=\"%1\"")
                .arg(table).arg(createTableQueries.value(table).first);
        query.exec("PRAGMA foreign_keys = OFF");
        if (!query.exec(dropString))
            goto onErrorSxHistory;
        if (!query.exec(createString))
            goto onErrorSxHistory;
        if (!query.exec(updateString))
            goto onErrorSxHistory;
        query.exec("PRAGMA foreign_keys = ON");
        return;
        onErrorSxHistory:
        logWarning(query.lastError().text());
        throw std::runtime_error("Altering table sxFiles failed");
    }
    logWarning("table " + table + "should be altered");
}
