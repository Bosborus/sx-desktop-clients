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

#include "scoutdatabase.h"
#include "sxlog.h"
#include <QDir>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>

ScoutDatabase::ScoutDatabase()
{
    initializeDatabase();
}

ScoutDatabase *ScoutDatabase::instance()
{
    static ScoutDatabase *sDatabase = new ScoutDatabase();
    return sDatabase;
}

bool ScoutDatabase::setFiles(const QString &volume, bool encryptedVolume, const QString &dir, const QString &etag, const QList<SxFileEntry *> &files)
{
    QSqlQuery query(mDatabase);
    QSet<QString> skippedDirs;
    if (encryptedVolume) {
        if (!query.exec("begin transaction"))
            return false;
        query.prepare("delete from remoteFiles where volume=:volume");
        query.bindValue(":volume", volume);
        if (!query.exec()) {
            query.exec("rollback transaction");
            return false;
        }
        query = QSqlQuery(mDatabase);
        if (!query.exec("commit transaction")) {
            return false;
        }
    }
    else {
        QSet<QString> dirs;
        if (!_getOnlyDirs(volume, dir, dirs, encryptedVolume))
            return false;
        foreach (auto entry, files) {
            if (entry->path().endsWith('/')) {
                if (dirs.contains(entry->path())) {
                    skippedDirs.insert(entry->path());
                    dirs.remove(entry->path());
                }
            }
        }
        if (!query.exec("begin transaction"))
            return false;
        query.prepare("delete from remoteFiles where volume=:volume and path like :path1 and path not like :path2");
        query.bindValue(":volume", volume);
        query.bindValue(":path1", dir+"%");
        query.bindValue(":path2", dir+"%/%");
        if (!query.exec()) {
            query.exec("rollback transaction");
            return false;
        }
        foreach (auto dir, dirs) {
            query = QSqlQuery(mDatabase);
            query.prepare("delete from remoteFiles where volume=:volume and path like :path");
            query.bindValue(":volume", volume);
            query.bindValue(":path", dir+"%");
            if (!query.exec()) {
                query.exec("rollback transaction");
                return false;
            }
        }
        query = QSqlQuery(mDatabase);
        if (!query.exec("commit transaction")) {
            return false;
        }
    }
    query = QSqlQuery(mDatabase);
    if (!query.exec("begin transaction"))
        return false;
    query = QSqlQuery(mDatabase);
    query.prepare("insert into remoteFiles (volume, path, etag) values (:volume, :path, :etag)");
    query.bindValue(":volume", volume);
    query.bindValue(":path", dir);
    query.bindValue(":etag", etag);
    if (!query.exec()) {
        query.exec("rollback transaction");
        return false;
    }

    foreach (auto entry, files) {
        if (entry->path().endsWith('/') && skippedDirs.contains(entry->path()))
            continue;
        query = QSqlQuery(mDatabase);
        query.prepare("insert into remoteFiles (volume, path, size) values (:volume, :path, :size)");
        query.bindValue(":volume", volume);
        query.bindValue(":path", entry->path());
        query.bindValue(":size", entry->size());
        if (!query.exec()) {
            query.exec("rollback transaction");
            return false;
        }
    }

    query = QSqlQuery(mDatabase);
    if (!query.exec("commit transaction"))
        return false;
    return true;
}

QString ScoutDatabase::getEtag(const QString &volume, const QString &dir) const
{
    QSqlQuery query(mDatabase);
    query.prepare("select etag from remoteFiles where volume=:volume and path=:path");
    query.bindValue(":volume", volume);
    query.bindValue(":path", dir);
    if (!query.exec()) {
        logError(query.lastError().text());
        return "";
    }
    if (!query.first() || query.isNull(0))
        return "";
    return query.value(0).toString();
}

bool ScoutDatabase::getFiles(const QString &volume, bool encryptedVolume, const QString &dir, QList<SxFileEntry *> &files)
{
    QSet<QString> dirSet;
    if (!_getOnlyDirs(volume, dir, dirSet, encryptedVolume))
        return false;
    QList<SxFileEntry *> fileList;
    if (!_getOnlyFiles(volume, dir, fileList))
        return false;
    foreach (auto entry, files) {
        delete entry;
    }
    files.clear();
    QStringList dirList = dirSet.toList();
    qSort(dirList);
    foreach (auto dir, dirList) {
        files.append(new SxFileEntry(dir, 0));
    }
    foreach (auto entry, fileList) {
        files.append(entry);
    }
    return true;
}

bool ScoutDatabase::_getOnlyFiles(const QString &volume, const QString &dir, QList<SxFileEntry *> &files)
{
    QSqlQuery query(mDatabase);
    query.prepare("select path, size from remoteFiles where volume=:volume and path like :path1 and path not like :path2 order by path");
    query.bindValue(":volume", volume);
    query.bindValue(":path1", dir+"%");
    query.bindValue(":path2", dir+"%/%");
    if (!query.exec()) {
        return false;
    }
    foreach (auto entry, files) {
        delete entry;
    }
    files.clear();
    while (query.next()) {
        QString path = query.value(0).toString();
        if (path==dir || path.endsWith("/.sxnewdir"))
            continue;
        qint64 size = query.value(1).toLongLong();
        files.append(new SxFileEntry(path, size));
    }
    return true;
}

bool ScoutDatabase::_getOnlyDirs(const QString &volume, const QString &dir, QSet<QString> &dirs, bool generateDirsFromFiles)
{
    QSqlQuery query(mDatabase);
    if (generateDirsFromFiles) {
        query.prepare("select path from remoteFiles where volume=:volume and path like :path order by path");
        query.bindValue(":path", dir+"%/%");
    }
    else {
        query.prepare("select path from remoteFiles where volume=:volume and path like :path and path not like :path2 order by path");
        query.bindValue(":path", dir+"%/");
        query.bindValue(":path2", dir+"%/%/");
    }
    query.bindValue(":volume", volume);
    if (!query.exec())
        return false;
    dirs.clear();

    if (generateDirsFromFiles) {
        while (query.next()) {
            QString full_path = query.value(0).toString();
            int index = full_path.indexOf('/', dir.length());
            if (index < 0)
                continue;
            QString path = full_path.mid(0, index+1);
            dirs.insert(path);
        }
    }
    else {
        while (query.next()) {
            QString path = query.value(0).toString();
            dirs.insert(path);
        }
    }
    return true;
}


void ScoutDatabase::initializeDatabase()
{
    static QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (!cacheDir.mkpath(".")) {
        throw std::runtime_error("Unable to create cache directory");
    }
    static QString dbFile = cacheDir.absoluteFilePath("sxsync.db");
    QString connectionName = "sxRemoteFilesBrowser";
    mDatabase = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    mDatabase.setDatabaseName(dbFile);
    if (!mDatabase.open()) {
        throw std::runtime_error("Cannot open the database");
    }

    QSqlQuery query(mDatabase);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA synchronous=NORMAL");
    query.exec("PRAGMA foreign_keys = ON");

    //query.exec("drop table remoteFiles");
    QString createTableQuery =  "create table if not exists remoteFiles ("
                                "volume text not null, "
                                "path text not null, "
                                "size number not null default 0, "
                                "etag text default null,"
                                "primary key (volume, path) on conflict replace)";
    if (!query.exec(createTableQuery)) {
        qDebug() << query.lastError();
        logError(query.lastError().text());
        throw std::runtime_error("Create table failed");
    }
}
