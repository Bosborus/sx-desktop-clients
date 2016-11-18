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

#include "sxauth.h"
#include "sxlog.h"

SxAuth::SxAuth()
{
    mSsl = false;
    mPort = 0;
}

SxAuth::SxAuth(const QString &cluster, const QString &initialAddress, const bool &ssl, const int &port, const QString &token)
{
    mCluster = cluster;
    mInitialAddress = initialAddress;
    mSsl = ssl;
    mPort = port;
    setToken(token);
}

SxAuth::SxAuth(const SxAuth &other)
{
    mCluster = other.mCluster;
    mInitialAddress = other.mInitialAddress;
    mPort = other.mPort;
    mSsl = other.mSsl;
    mTokenKey = other.mTokenKey;
    mTokenUser = other.mTokenUser;
}

QString SxAuth::clusterName() const
{
    return mCluster;
}

QString SxAuth::initialAddress() const
{
    return mInitialAddress;
}

bool SxAuth::use_ssl() const
{
    return mSsl;
}

int SxAuth::port() const
{
    return mPort;
}

QByteArray SxAuth::token_user() const
{
    return mTokenUser;
}

QByteArray SxAuth::token_key() const
{
    return mTokenKey;
}

bool SxAuth::isValid() const
{
    return !mCluster.isEmpty() && !mTokenKey.isEmpty() && !mTokenUser.isEmpty() && mPort > 0;
}

bool SxAuth::checkToken(const QString &token) const
{
    QByteArray user, key;
    QByteArray usrkey = QByteArray::fromBase64(token.toUtf8());
    if(token.size() == 56 && usrkey.size() == 42) {
        user = usrkey.left(20);
        key = usrkey.mid(20, 20);
    }
    return (mTokenUser == user && mTokenKey == key);
}

void SxAuth::setToken(const QString &token)
{
    QByteArray usrkey = QByteArray::fromBase64(token.toUtf8());
    if(token.size() == 56 && usrkey.size() == 42) {
        mTokenUser = usrkey.left(20);
        mTokenKey = usrkey.mid(20, 20);
    }
    else {
        logError("invalid token size");
        mTokenUser.clear();
        mTokenKey.clear();
    }
}
