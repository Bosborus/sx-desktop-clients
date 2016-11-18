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

#ifndef SXAUTH_H
#define SXAUTH_H

#include <QObject>

class SxAuth
{
public:
    SxAuth();
    SxAuth(const QString& cluster, const QString& initialAddress, const bool& ssl, const int& port, const QString& token);
    SxAuth(const SxAuth& other);
    QString clusterName() const;
    QString initialAddress() const;
    bool use_ssl() const;
    int port() const;
    QByteArray token_user() const;
    QByteArray token_key() const;
    bool isValid() const;
    bool checkToken(const QString& token) const;
    void setToken(const QString& token);
private:
    QString mCluster;
    QString mInitialAddress;
    bool mSsl;
    int mPort;
    QByteArray mTokenUser;
    QByteArray mTokenKey;
};

#endif // SXAUTH_H
