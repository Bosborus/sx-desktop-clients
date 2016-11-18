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

#ifndef SXQUERY_H
#define SXQUERY_H

#include <QObject>
#include <QNetworkRequest>

#include "sxauth.h"

class SxQuery {
public:
    enum QueryType {
        GET,
        HEAD,
        PUT,
        DELETE,
        JOB_PUT,
        JOB_DELETE
    };
    SxQuery(const QString& path, const QueryType& type, const QByteArray &body);
    ~SxQuery();
    QueryType queryType();
    QNetworkRequest makeRequest(const QString &target, const SxAuth &sxAuth, const qint64 time_drift, const QString &etag);
    const QByteArray& body() const;
    const int number;
private:
    QString mPath;
    QueryType mQueryType;
    QByteArray mBody;
    QByteArray mBodyHash;

    static const QStringList m_months;
    static const QStringList m_wdays;
};

#endif // SXQUERY_H
