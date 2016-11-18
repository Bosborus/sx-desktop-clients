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

#include <QHostAddress>

#include "sxurl.h"
#include "sxcluster.h"

/*
 * awesome scheme:
 * sx://clustername;token=TOKEN,ip=1.2.3.4,port=443,ssl=(y|n),volume=VOLNAME,certhash=SHA1
 */

SxUrl::SxUrl(const QString &url) {
    QStringList args;
    int sep, index;

    QChar sepFirst = '?';
    QChar sepSecond = '&';

    if (url.contains(';'))
    {
        sepFirst = ';';
        sepSecond = ',';
    }

    if(!url.startsWith("sx://"))
	goto badurl;

    sep = url.indexOf(sepFirst);
    if(sep < 0)
	goto badurl;

    m_name = url.mid(5, sep - 5).toLower();
    index = m_name.lastIndexOf('@');
    if (index != -1)
    {
        m_username = QUrl::fromPercentEncoding(m_name.left(index).toUtf8());
        m_name = m_name.mid(index+1);
    }
    if (m_name.endsWith('/'))
        m_name = m_name.left(m_name.length()-1);
    m_port = -1;
    m_ssl = true;

    args = url.mid(sep+1).split(sepSecond);
    while(!args.empty()) {
	QString arg = args.takeFirst();
	if((sep = arg.indexOf('=')) < 0)
	    continue;
	QString k = arg.left(sep);
	QString v = arg.mid(sep+1);
	if(k == "token") {
	    m_auth = QUrl::fromPercentEncoding(v.toUtf8());
	} else if(k == "ip") {
	    m_addr = v;
	} else if(k == "port") {
	    m_port = v.toInt();
	} else if(k == "ssl") {
	    if(v.compare("n", Qt::CaseInsensitive) ||
	       v.compare("no", Qt::CaseInsensitive) ||
	       v.compare("false", Qt::CaseInsensitive) ||
	       v != "0")
		continue;
	    m_ssl = false;
	} else if(k == "volume") {
	    m_vol = v;
	} else if(k == "certhash") {
	    QString hash = v.remove(':');
	    if(hash.size() != 40)
		continue;
	    m_crtsha1 = QByteArray::fromHex(hash.toUtf8());
	}
    }

    if(m_port < 0)
	m_port = m_ssl ? 443 : 80;

    if(m_name.isEmpty())
	goto badurl;

    if(m_auth.size() != 56 || QByteArray::fromBase64(m_auth.toUtf8()).size() != 42)
        goto badurl;

    if(!m_addr.isNull()) {
	QHostAddress addr;
	if(!addr.setAddress(m_addr))
	    goto badurl;
    }

    if(m_port <= 0 || m_port >= 0x10000)
	goto badurl;

    if(!m_ssl)
	m_crtsha1 = QByteArray();
    else if(!m_crtsha1.isNull() && m_crtsha1.size() != 20)
	goto badurl;

    return;
 badurl:
    m_name = QString();
    m_username = QString();
    m_auth = QString();
    m_addr = QString();
    m_vol = QString();
    m_port = -1;
    m_crtsha1 = QByteArray();
}

bool SxUrl::isValid() const {
    return !m_name.isNull();
}

QString SxUrl::getClusterName() const {
    return m_name;
}

QString SxUrl::getUsername() const {
    return m_username;
}

QString SxUrl::getAuth() const {
    return m_auth;
}

QString SxUrl::getNodeAddess() const {
    return m_addr;
}

int SxUrl::getPort() const {
    return m_port;
}

bool SxUrl::getSslFlag() const {
    return m_ssl;
}

QByteArray SxUrl::getCertFingerprint() const {
    return m_crtsha1;
}

QString SxUrl::getVolume() const {
    return m_vol;
}

QString SxUrl::url() const {
    if(!isValid())
        return QString();

    QString ret("sx://");
    ret += m_name;
    ret += ";token=" + m_auth;
    if(!m_addr.isNull())
	ret += ",ip=" + m_addr;
    ret += ",port=" + QString::number(m_port);
    ret += ",ssl=" + QString(m_ssl ? "y" : "n");
    ret += ",volume=" + m_vol;
    if(m_ssl && !m_crtsha1.isNull())
	ret += "certhash=" + m_crtsha1.toHex();
    return ret;
}

bool SxUrl::operator!=(const SxUrl &other) const {
    if(!isValid() || !other.isValid())
	return true;
    return url() != other.url();
}

void SxUrl::setCertFingerprint(QByteArray certFingerprint)
{
    if (!m_ssl || certFingerprint.size() != 20)
        qDebug() << Q_FUNC_INFO << " unable to set certificate fingerprint";
    else
        m_crtsha1 = certFingerprint;
}
