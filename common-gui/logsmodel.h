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

#ifndef LOGSMODEL_H
#define LOGSMODEL_H

#include <QAbstractTableModel>
#include <QMutex>
#include <QMutexLocker>
#include <QDateTime>
#include <QList>
#include "sxlog.h"

class QTimer;

class LogsModel : public QAbstractTableModel, public LogModelInterface
{
    Q_OBJECT
public:
    static LogsModel *instance();
    ~LogsModel();
    int rowCount(const QModelIndex &parent) const override;
    int columnCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    void appendLog(LogLevel type, const QString &func, const QString &message, const QDateTime &dateTime, const QString &thread) override;
    QString line(int row) const;

public slots:
    void removeLogs();

private slots:
    void showLogs();

private:
    struct LogData
    {
        LogData(LogLevel type, const QString &func, const QString &message, const QDateTime &dateTime, const QString &thread);
        LogLevel mType;
        QString mFunc;
        QString mMessage;
        QDateTime mTime;
        QString mThread;
    };
    LogsModel();
    //mutable QMutex m_mutex;
    QMutex m_mutexOnAppend;
    QList<LogData> m_list;
    QList<LogData> *m_newLogs;
    QTimer *m_showLogsTimer;
    const int log_limit = 10000;
};

#endif // LOGSMODEL_H
