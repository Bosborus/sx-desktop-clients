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

#ifndef CLUSTERCONFIG_H
#define CLUSTERCONFIG_H

#include <QSettings>
#include <QMutex>

class SxAuth;

class ClusterConfig {
public:
    QString cluster() const;
    QByteArray uuid() const;
    QString address() const;
    QString token() const;
    QString username() const;
    QString vcluster() const;
    void setVcluster(const QString &vcluster);
    QByteArray clusterCertFp() const;
    void setClusterCertFp(const QByteArray &certFp);
    QByteArray secondaryClusterCertFp() const;
    void setSecondaryClusterCertFp(const QByteArray &certFp);
    QByteArray sxwebCertFp() const;
    void setSxwebCertFp(const QByteArray &certFp);
    bool ssl() const;
    int port() const;
    SxAuth sxAuth() const;
    void setConfig(const QString &cluster, const QString& uuid, const QString &token, const QString &initialAddress, bool useSsl, int port, const QByteArray& certFp);
    void setUsername(const QString &username);
    bool isValid();
protected:
    ClusterConfig(QSettings &settings, QMutex &mutex);
private:
    bool mValid;
    QSettings &mSettings;
    QMutex &mMutex;
    bool validate();
    friend class SxConfig;
};

#endif // CLUSTERCONFIG_H
