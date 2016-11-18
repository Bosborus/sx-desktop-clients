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

#include "sxquery.h"
#include <QMessageAuthenticationCode>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include "sxcluster.h"
#include "sxlog.h"

static int number_generator = 0;
const QStringList SxQuery::m_months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const QStringList SxQuery::m_wdays = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

SxQuery::SxQuery(const QString &path, const SxQuery::QueryType &type, const QByteArray &body) : number(number_generator++)
{
    mPath = path;
    mQueryType = type;
    mBody = body;
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(body);
    mBodyHash = sha1.result().toHex();
}

SxQuery::~SxQuery()
{
}

SxQuery::QueryType SxQuery::queryType()
{
    return mQueryType;
}

QNetworkRequest SxQuery::makeRequest(const QString& target, const SxAuth& sxAuth, const qint64 time_drift, const QString &etag)
{
    QNetworkRequest ret;

    if (sxAuth.use_ssl())
    {
        QSslConfiguration config = QSslConfiguration::defaultConfiguration();
#if QT_VERSION >= QT_VERSION_CHECK(5,6,0) && defined Q_OS_MAC
        config.setProtocol(QSsl::AnyProtocol);
#elif QT_VERSION >= QT_VERSION_CHECK(5,5,0)
        config.setProtocol(QSsl::TlsV1_0OrLater);
#else
        config.setProtocol(QSsl::AnyProtocol);
#endif
        ret.setSslConfiguration(config);
    }

    QString path;
    if (mPath.startsWith("/"))
        path = mPath;
    else
        path = "/"+mPath;

    QUrl url((sxAuth.use_ssl() ? "https://" : "http://")
            + target
            + ((sxAuth.use_ssl() && sxAuth.port() != 443) || (!sxAuth.use_ssl() && sxAuth.port() != 80) ? ":" + QString::number(sxAuth.port()) : "")
            + path, QUrl::TolerantMode);

    if(!url.isValid())
        return ret;
    url.setUrl(url.toEncoded(QUrl::NormalizePathSegments));

    QMessageAuthenticationCode hmac(QCryptographicHash::Sha1, sxAuth.token_key());
    switch(mQueryType) {
    case GET:
        hmac.addData("GET");
        break;
    case PUT:
    case JOB_PUT:
        hmac.addData("PUT");
        break;
    case DELETE:
    case JOB_DELETE:
        hmac.addData("DELETE");
        break;
    case HEAD:
        hmac.addData("HEAD");
        break;
    }
    hmac.addData("\n");

    hmac.addData(url.toEncoded(QUrl::RemoveScheme | QUrl::RemoveAuthority).mid(1));
    hmac.addData("\n");

    QDateTime qdt = QDateTime::currentDateTimeUtc();
    qdt = qdt.addSecs(time_drift);
    QString dtstr =
        m_wdays.at(qdt.date().dayOfWeek() - 1) + ", " +
        qdt.toString("dd") + " " +
        m_months.at(qdt.date().month() - 1) + " " +
        qdt.toString("yyyy hh:mm:ss") + " GMT";
    hmac.addData(dtstr.toUtf8());
    hmac.addData("\n");

    hmac.addData(mBodyHash);
    hmac.addData("\n");

    ret.setUrl(url);
    ret.setHeader(QNetworkRequest::UserAgentHeader, "sxqt-"+SxCluster::getClientVersion());
    ret.setRawHeader("Date", dtstr.toUtf8());
    if (!etag.isEmpty())
    {
        ret.setRawHeader("If-None-Match", etag.toUtf8());
    }
    QByteArray auth = sxAuth.token_user();
    auth.append(hmac.result());
    auth.append(QByteArray(2, 0));
    ret.setRawHeader("Authorization", QString(QString("SKY ") + auth.toBase64()).toUtf8());
    ret.setRawHeader("SX-Cluster-Name", sxAuth.clusterName().toUtf8());

    if(!mBody.isEmpty())
        ret.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    return ret;
}

const QByteArray &SxQuery::body() const
{
    return mBody;
}
