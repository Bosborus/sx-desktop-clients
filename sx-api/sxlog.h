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

#ifndef SXDEBUG_H
#define SXDEBUG_H

#include <QThread>
#include <QDebug>
#include <QDateTime>
#include <QMutex>
#include <QFile>

enum class LogLevel {
    Entry,
    Debug,
    Verbose,
    Info,
    Warning,
    Error
};

class LogModelInterface {
public:
    virtual ~LogModelInterface() {}
    virtual void appendLog(LogLevel type, const QString &func, const QString &message, const QDateTime &dateTime, const QString& thread) = 0;
};

class SxLog
{
public:
    static SxLog& instance();
    SxLog(const SxLog &) = delete;
    SxLog &operator= (const SxLog &) = delete;
    void log(LogLevel type, const QString &func, const QString &message) const;
    void setLogLevel(LogLevel level);
    void setLogModel(LogModelInterface* model);
    bool exportLogs(const QString &file);
    void removeLogFiles();
private:
    SxLog();
    LogLevel mLogLevel;
    void* hConsole;
    mutable QMutex mMutex;
    LogModelInterface *mLogModel;
    QString nameTemplate;
    const qint64 mLogFileSizeLimit = 20*1024*1024;
    mutable QFile *logFile;
};

#define logEntry(message)   SxLog::instance().log(LogLevel::Entry,   Q_FUNC_INFO, message)
#define logDebug(message)   SxLog::instance().log(LogLevel::Debug,   Q_FUNC_INFO, message)
#define logVerbose(message) SxLog::instance().log(LogLevel::Verbose, Q_FUNC_INFO, message)
#define logInfo(message)    SxLog::instance().log(LogLevel::Info,    Q_FUNC_INFO, message)
#define logWarning(message) SxLog::instance().log(LogLevel::Warning, Q_FUNC_INFO, message)
#define logError(message)   SxLog::instance().log(LogLevel::Error,   Q_FUNC_INFO, message)

#endif // SXDEBUG_H
