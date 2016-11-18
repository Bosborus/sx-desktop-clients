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

#ifndef SXDATABASE_H
#define SXDATABASE_H

#include <QDir>
#include <QFile>
#include <QObject>
#include <QSqlDatabase>
#include <QMutex>
#include <QSet>
#include "sxvolume.h"
#include "sxfileentry.h"
#include "sxvolumeentry.h"
#include <functional>

#ifdef Q_OS_WIN
using uint32_t = uint;
#endif

class SxDatabase : public QObject
{
    Q_OBJECT
public:
    enum class ACTION {
        SKIP = 0,
        REMOVE_REMOTE = 1,
        REMOVE_LOCAL = 2,
        DOWNLOAD = 3,
        UPLOAD = 4
    };

    SxDatabase(const SxDatabase&) = delete;
    SxDatabase& operator=(const SxDatabase&) = delete;

    static SxDatabase& instance();
    static void setOldVolumeName(const QString &volume);

    bool updateVolumes(const QList<const SxVolume*> &list, QHash<QString, QString> &modifiedNames);
    bool getVolumeList(QList<SxVolumeEntry> &volumeList) const;
    bool startUpdatingFiles(std::function<bool()> abortedCB);
    bool endUpdatingFiles();
    bool updateRemoteFiles(const QString &volume, const QList<SxFileEntry*> &list);
    bool updateLocalFiles(const QString &volume, const QList<QString> &list, const QDir &volumeRootDir);
    bool updateLocalDirs(const QString &volume, const QList<QString> &list, const QDir &volumeRootDir);
    bool markVolumeFilesToRemove(const QString& volume, bool removeRemote, bool onlySkipped);
    bool markLocalDirFilesToRemove(const QString& volume, const QString& dir, bool onlyExisting=false);
    bool remoteFileExists(const QString& volume, const QString& file);
    bool dropFileEntry(const QString& volume, const QString& file);
    QList<QString> getMarkedFiles(const QString& volume, ACTION action) const ;
    bool getLocalFileMtime(const QString &volume, const QString &path, uint32_t &mtime) const;
    bool isLocalDir(const QString& volume, const QString& path);
    void onFileUploaded(const QString &volume, const SxFileEntry &fileEntry, bool _registerEvent);
    void onFileUploaded(const QString &volume, const QString &path, QString rev, quint32 mTime);
    void onFileDownloaded(const QString &volume, const SxFileEntry &fileEntry);
    void onRemoteFileRemoved(const QString &volume, const QString &file);
    void onLocalFileRemoved(const QString &volume, const QString &file);
    bool getHistoryRowIds(QList<qint64> &list) const;
    bool getHistoryEntry(qint64 rowId, QString &path, uint32_t &eventDate, ACTION &eventType) const;
    qint64 getRemoteFileSize(const QString& volume, const QString &path);
    QString getRemoteFileRevision(const QString& volume, const QString &path);
    bool removeVolumeFiles(const QString &volume);
    bool removeVolumeHistory(const QString &volume);
    QList<QPair<QString, QString> > getRecentHistory(bool shareHistory, int limit) const;
    void updateFileBlocks(const QString &volume, const SxFileEntry &fileEntry);
    void removeFileBlocks(const QString &volume, const QString& path);
    bool findBlock(const QString &hash, int blockSize, QList<std::tuple<QString, QString, qint64>>& result);
    bool findIdenticalFiles(const QString &volume, qint64 remoteFileSize, int blockSize, const QStringList& blocks, QList<QPair<QString, quint32>> &result);
    void addSuppression(const QString &volume, const QString &path);
    void removeSuppression(const QString &volume, const QString &path);
    bool getFilesCount(const QString& volume, bool localFiles, quint32 &count);
    bool testHistoryRevision(const QString &volume, const QString &file, const QString &revision, int &count);
    bool updateInconsistentFile(const QString &volume, const QString &file, const QStringList &revisions);
    bool getInconsistentFile(const QString &volume, const QString &file, QStringList &revisions);

signals:
    void sig_historyChanged(qint64 rowId, qint64 removeId);
    void sig_volumeListUpdated();
    void sig_possibleInconsistencyDetected(const QString &volume, const QString &file);

private:
    static QSqlDatabase getThreadConnection();
    void registerEvent(const QString& volume, const QString& path, ACTION action, const QString &revision="");
    static const int sShowHistoryLimit = 1000;
    static const int sHistoryLimit = 100000;
    SxDatabase();
    void setupTables();
    QHash<QString, int> tables();
    static QHash<QThread*, QString> sConnections;
    QDateTime mStartTime;
    void updateSxTable(QString table, int fromVersion);
    std::function<bool()> mAbortedCB;
    QSet<QString> mSuppressedFiles;

#ifdef Q_OS_WIN
    void report_error(const QString &msg, const QSqlError &error) const;
#else
    [[ noreturn ]] void report_error(const QString &msg, const QSqlError &error) const;
#endif

    mutable QMutex mMutex;
    static QString sOldVolumeName;
};

Q_DECLARE_METATYPE(SxDatabase::ACTION)

#endif // SXDATABASE_H
