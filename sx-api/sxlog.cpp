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

#include "sxlog.h"
#include <iostream>

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <Windows.h>
#endif

SxLog &SxLog::instance()
{
    static SxLog sInstance;
    return sInstance;
}

void SxLog::log(LogLevel type, const QString &func, const QString &message) const {
    QMutexLocker locker(&mMutex);
    if (type < mLogLevel)
        return;
    QString logLine;
    switch (type) {
    case LogLevel::Entry: {
        logLine = "ENTRY";
    } break;
    case LogLevel::Debug: {
        logLine = "DEBUG";
    } break;
    case LogLevel::Verbose: {
        logLine = "VERB.";
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

    QString thread;
    QDateTime currentDateTime = QDateTime::currentDateTime();
    if (QThread::currentThread()->property("name").isValid())
        thread = QThread::currentThread()->property("name").toString().leftJustified(10, ' ', true);
    else
        thread = QString("0x%1").arg(reinterpret_cast<quintptr>(QThread::currentThread()), 8, 16, QChar('0'));
    QString time = currentDateTime.toString(Qt::ISODate);
    logLine += QString(" | %1 | %2 | %3 | %4").arg(thread, time, func, message);
    QDir cacheDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    cacheDir.mkdir("log");
    if (logFile == nullptr) {
        logFile = new QFile(nameTemplate.arg(0));
        if (!logFile->open(QIODevice::WriteOnly | QIODevice::Append)) {
            delete logFile;
            logFile = nullptr;
			return;
        }
    }
    if (logFile->isOpen()) {
        QByteArray data = (logLine+"\n").toLocal8Bit();
        logFile->write(data);
        logFile->flush();
        if (logFile->size() >= mLogFileSizeLimit) {
            logFile->close();
            delete logFile;
            logFile = nullptr;
            for (int i=8; i>=0; i--) {
                QFile file(nameTemplate.arg(i));
                if (!file.exists())
                    continue;
                file.rename(nameTemplate.arg(i+1));
            }
        }
    }
    if (mLogModel != nullptr)
        mLogModel->appendLog(type, func, message, currentDateTime, thread);
}

void SxLog::setLogLevel(LogLevel level)
{
    QMutexLocker locker(&mMutex);
    mLogLevel = level;
}

void SxLog::setLogModel(LogModelInterface *model)
{
    QMutexLocker locker(&mMutex);
    mLogModel = model;
}

static quint32 crc32_table[256];
#define CRC32_POLYNOMIAL 0x04c11db7

static quint32 crc32_reflect( quint32 ulReflect, char cChar )
{
    quint32 ulValue = 0;
    for( int iPos = 1; iPos < ( cChar + 1 ); iPos++ ) {
        if( ulReflect & 1 )
            ulValue |= 1 << ( cChar - iPos );
        ulReflect >>= 1;
    }
    return ulValue;
}

static bool crc32_initialize() {
    for( quint32 iCodes = 0; iCodes <= 0xFF; iCodes++ ) {
        crc32_table[iCodes] = crc32_reflect( iCodes, 8 ) << 24;
        for( int iPos = 0; iPos < 8; iPos++ )
            crc32_table[iCodes] = ( crc32_table[iCodes] << 1 ) ^ ( crc32_table[iCodes] & (static_cast<quint32>(1) << 31) ? CRC32_POLYNOMIAL : 0 );
        crc32_table[iCodes] = crc32_reflect( crc32_table[iCodes], 32 );
    }
    return true;
}


static const bool _crc_table_initialized = crc32_initialize();

static void crc32_partial( quint32 *ulInCRC, const unsigned char *sData, quint32 ulLength )
{
        while( ulLength-- )
                *ulInCRC = ( *ulInCRC >> 8 ) ^ crc32_table[( *ulInCRC & 0xFF ) ^ *sData++];
}

quint32 crc32_full( const QByteArray& data )
{
        quint32 ulCRC = 0xFFFFFFFF;
        crc32_partial( &ulCRC, reinterpret_cast<const unsigned char*>(data.data()), static_cast<quint32>(data.size()));
        return ulCRC ^ 0xFFFFFFFF;
}

QByteArray gzip_compress(const QByteArray& data)
{
    auto compressedData = qCompress(data);
    compressedData.remove(0, 6);
    compressedData.chop(4);

    QByteArray header;
    QDataStream ds1(&header, QIODevice::WriteOnly);
    ds1 << quint16(0x1f8b)
        << quint16(0x0800)
        << quint16(0x0000)
        << quint16(0x0000)
        << quint16(0x000b);

    QByteArray footer;
    QDataStream ds2(&footer, QIODevice::WriteOnly);
    ds2.setByteOrder(QDataStream::LittleEndian);
    ds2 << crc32_full(data)
        << quint32(data.size());

    return header + compressedData + footer;
}

bool SxLog::exportLogs(const QString &file)
{
    QByteArray uncommpressed;
    QFile out_file(file);
    if (!out_file.open(QIODevice::WriteOnly))
        return false;

    QMutexLocker locker(&mMutex);
    for (int i = 9; i >= 0; i--) {
        QFileInfo finfo(nameTemplate.arg(i));
        if (finfo.exists()) {
            QFile in_file(finfo.filePath());
            if (!in_file.open(QIODevice::ReadOnly)) {
                out_file.close();
                out_file.remove();
                return false;
            }
            uncommpressed.append(in_file.readAll());
            in_file.close();
        }
    }

    if (uncommpressed.isEmpty())
        return false;
    QByteArray commpressed = gzip_compress(uncommpressed);
    uncommpressed.clear();

    out_file.write(commpressed);
    if (!out_file.flush()) {
        out_file.close();
        out_file.remove();
        return false;
    }
    out_file.close();
    return true;
}

void SxLog::removeLogFiles()
{
    QMutexLocker locker(&mMutex);
    for (int i = 9; i >= 0; i--) {
        QFile file(nameTemplate.arg(i));
        if (file.exists())
            file.remove();
    }
}

SxLog::SxLog()
{
    mLogModel = nullptr;
    mLogLevel = LogLevel::Entry;
#ifdef Q_OS_WIN
    hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    Q_UNUSED(hConsole);
    nameTemplate = QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/log/sxdrive-%1.log";
    QDir dir;
    dir.mkpath(QFileInfo(nameTemplate).absolutePath());
    logFile = nullptr;
}
