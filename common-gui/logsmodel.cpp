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

#include "logsmodel.h"
#include <QSize>
#include <QTimer>
#include <QModelIndex>
#include <QVector>
#include <QDebug>
#include <QColor>
#include <QVariant>

LogsModel::LogsModel()
{
    m_showLogsTimer = new QTimer();
    m_newLogs = new QList<LogData>();
    connect(m_showLogsTimer, &QTimer::timeout, this, &LogsModel::showLogs);
    m_showLogsTimer->start(100);
}

LogsModel *LogsModel::instance()
{
    static LogsModel m;
    return &m;
}

LogsModel::~LogsModel()
{
    if (m_newLogs != nullptr)
        delete m_newLogs;
    delete m_showLogsTimer;
    m_list.clear();
}

int LogsModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    //QMutexLocker locker(&m_mutex);
    return m_list.count();
}

int LogsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

QVariant LogsModel::data(const QModelIndex &index, int role) const
{
    //QMutexLocker locker(&m_mutex);
    if (index.row() < 0 || index.row() >= m_list.count())
        return QVariant();
    if (role==Qt::DisplayRole)
    {
        if (index.column()==0)
        {
            return m_list[index.row()].mTime.toString("yyyy-MM-dd hh:mm:ss");
        }
        else
        {
            return m_list[index.row()].mMessage;
        }
    }
    else if (role==Qt::BackgroundRole)
    {
        switch (m_list[index.row()].mType) {
        case LogLevel::Error:
            return QColor::fromRgb(255,0,0);
        case LogLevel::Warning:
            return QColor::fromRgb(230,200,40);
        case LogLevel::Info:
            return QColor::fromRgb(255,255,255);
        case LogLevel::Verbose:
            return QColor::fromRgb(230,230,230);
        case LogLevel::Debug:
            return QColor::fromRgb(200,200,200);
        case LogLevel::Entry:
            return QColor::fromRgb(255,150,0);
        }
    }
    return QVariant();
}

void LogsModel::appendLog(LogLevel type, const QString &func, const QString &message, const QDateTime &dateTime, const QString &thread)
{
    QMutexLocker appendLocker(&m_mutexOnAppend);
    m_newLogs->append(LogData(type, func, message, dateTime, thread));
    /*
    */
}

QString LogsModel::line(int row) const
{
    //QMutexLocker locker(&m_mutex);
    if (row < 0 || row >= m_list.count())
        return QString();

    QString logLine;
    switch (m_list[row].mType) {
    case LogLevel::Entry: {
        logLine = "ENTRY";
    } break;
    case LogLevel::Verbose: {
        logLine = "VERB.";
    } break;
    case LogLevel::Debug: {
        logLine = "DEBUG";
    } break;
    case LogLevel::Info: {
        logLine = "INFO ";
    } break;
    case LogLevel::Warning: {
        logLine = "WARN ";
    } break;
    case LogLevel::Error: {
        logLine = "ERROR";
    } break;
    }
    QString thread = m_list[row].mThread;
    QString time = m_list[row].mTime.toString(Qt::ISODate);
    QString func = m_list[row].mFunc;
    QString message = m_list[row].mMessage;
    //locker.unlock();
    logLine += QString(" | %1 | %2 | %3 | %4").arg(thread, time, func, message);
    return logLine;
}

void LogsModel::removeLogs()
{
    QMutexLocker appendLocker(&m_mutexOnAppend);
    int rows = rowCount(QModelIndex());
    emit beginRemoveRows(QModelIndex(), 0, rows-1);
    //QMutexLocker locker(&m_mutex);
    m_list.clear();
    //locker.unlock();
    emit endRemoveRows();
}

void LogsModel::showLogs()
{
    QMutexLocker appendLocker(&m_mutexOnAppend);
    auto logs = m_newLogs;
    m_newLogs = new QList<LogData>();
    appendLocker.unlock();
    if (logs->isEmpty()) {
        delete logs;
        return;
    }
    int rows = rowCount(QModelIndex());
    emit beginInsertRows(QModelIndex(), rows, rows+logs->count()-1);
    foreach(const LogData& line, *logs) {
        m_list.append(line);
    }
    emit endInsertRows();
    delete logs;

    if (log_limit && m_list.count() > 2*log_limit) {
        int toRemove = m_list.count() - log_limit;
        emit beginRemoveRows(QModelIndex(), 0, toRemove-1);

        for (int i=0; i<toRemove; i++)
            m_list.removeFirst();
        emit endRemoveRows();
    }
}

LogsModel::LogData::LogData(LogLevel type, const QString &func, const QString &message, const QDateTime &dateTime, const QString &thread)
{
    mType = type;
    mFunc = func;
    mMessage = message;
    mTime = dateTime;
    mThread = thread;
}
