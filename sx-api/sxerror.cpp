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

#include "sxerror.h"

#include <QCoreApplication>

SxError::SxError()
{
    mErrorCode = SxErrorCode::NoError;
}

SxError::SxError(const SxErrorCode &errorCode, const QString &errorMessage, const QString &errorMessageTr)
{
    mErrorCode = errorCode;
    mErrorMessage = errorMessage;
    mErrorMessageTr = errorMessageTr;
}

SxError::SxError(const SxErrorCode &errorCode, const QString &errorMessage, const QString &errorMessageTr, const QString &volume, const QString &path)
{
    mErrorCode = errorCode;
    mErrorMessage = errorMessage;
    mErrorMessageTr = errorMessageTr;
    mVolume = volume;
    mPath = path;
}

SxErrorCode SxError::errorCode() const
{
    return mErrorCode;
}

QString SxError::errorMessage() const
{
    if (mErrorMessage.isEmpty()) {
        switch (mErrorCode) {
        case SxErrorCode::NoError:
            return "SxErrorCode::NoError";
        case SxErrorCode::AbortedByUser:
            return "SxErrorCode::AbortedByUser";
        case SxErrorCode::Timeout:
            return "SxErrorCode::Timeout";
        case SxErrorCode::NetworkError:
            return "SxErrorCode::NetworkError";
        case SxErrorCode::SslError:
            return "SxErrorCode::SslError";
        case SxErrorCode::InvalidServer:
            return "SxErrorCode::InvalidServer";
        case SxErrorCode::InvalidCredentials:
            return "SxErrorCode::InvalidCredentials";
        case SxErrorCode::InvalidArgument:
            return "SxErrorCode::InvalidArgument";
        case SxErrorCode::NotFound:
            return "SxErrorCode::NotFound";
        case SxErrorCode::NotChanged:
            return "SxErrorCode::NotChanged";
        case SxErrorCode::TooManyRequests:
            return "SxErrorCode::TooManyRequests";
        case SxErrorCode::CriticalServerError:
            return "SxErrorCode::CriticalServerError";
        case SxErrorCode::FilterError:
            return "SxErrorCode::FilterError";
        case SxErrorCode::IOError:
            return "SxErrorCode::IOError";
        case SxErrorCode::SoftError:
            return "SxErrorCode::SoftError";
        case SxErrorCode::OutOfSpace:
            return "SxErrorCode::OutOfSpace";
        case SxErrorCode::FileDirConflict:
            return "SxErrorCode::FileDirConflict";
        case SxErrorCode::UnknownError:
            return "SxErrorCode::UnknownError";
        case SxErrorCode::NetworkConfigurationChanged:
            return "SxErrorCode::NetworkConfigurationChanged";
        }
        return QString("SxErrorCode: %1").arg(static_cast<int>(mErrorCode));
    }
    return mErrorMessage;
}

QString SxError::errorMessageTr() const
{
    if (mErrorMessageTr.isEmpty())
        return errorMessage();
    return mErrorMessageTr;
}

QString SxError::volume() const
{
    return mVolume;
}

QString SxError::path() const
{
    return mPath;
}

void SxError::setVolume(const QString &volume)
{
    mVolume = volume;
    mPath.clear();
}

void SxError::setPath(const QString &volume, const QString &path)
{
    mVolume = volume;
    mPath = path;
}

SxError SxError::errorBadReplyContent()
{
    return SxError(SxErrorCode::UnknownError, "bad reply content", QCoreApplication::translate("SxErrorMessage", "bad reply content"));
}
