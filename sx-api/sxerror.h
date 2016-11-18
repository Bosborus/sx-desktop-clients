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

#ifndef SXERROR_H
#define SXERROR_H

#include <QObject>
#include <QCoreApplication>

enum class SxErrorCode {
    NoError,
    AbortedByUser,
    Timeout,
    NetworkError,
    SslError,
    InvalidServer,
    InvalidCredentials,
    InvalidArgument,
    NotFound,
    NotChanged,
    TooManyRequests,
    CriticalServerError,
    FilterError,
    IOError,
    SoftError,
    OutOfSpace,
    FileDirConflict,
    UnknownError,
    NetworkConfigurationChanged
};

class SxError
{
public:
    SxError();
    SxError(const SxErrorCode &errorCode, const QString &errorMessage, const QString &errorMessageTr);
    SxError(const SxErrorCode &errorCode, const QString &errorMessage, const QString &errorMessageTr, const QString &volume, const QString &path);
    SxErrorCode errorCode() const;
    QString errorMessage() const;
    QString errorMessageTr() const;
    QString volume() const;
    QString path() const;
    void setVolume(const QString &volume);
    void setPath(const QString &volume, const QString &path);

    static SxError errorBadReplyContent();

private:
    SxErrorCode mErrorCode;
    QString mErrorMessage;
    QString mErrorMessageTr;
    QString mVolume;
    QString mPath;
};

#endif // SXERROR_H
