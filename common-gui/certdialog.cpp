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

#include <QCryptographicHash>
#include "certdialog.h"

CertDialog::CertDialog(const QSslCertificate &cert, QWidget *parent) : QMessageBox(parent), m_cert(cert) {
    /* Doing this by hand because fucking OSX */
    setIcon(QMessageBox::Warning);
    setWindowTitle(tr("The connection to the cluster is not trusted"));
    setTextFormat(Qt::RichText);
    setStandardButtons(QMessageBox::Yes|QMessageBox::No);
    setDefaultButton(QMessageBox::No);

    QCryptographicHash sha1(QCryptographicHash::Sha1);
    sha1.addData(m_cert.toDer());
    m_certFps = sha1.result();

    QString certIssuer, certSubject;

    if(!m_cert.issuerInfo(QSslCertificate::Organization).isEmpty())
	certIssuer += "O: " + m_cert.issuerInfo(QSslCertificate::Organization).join(", ") + ", ";
    if(!m_cert.issuerInfo(QSslCertificate::OrganizationalUnitName).isEmpty())
	certIssuer += "OU: " + m_cert.issuerInfo(QSslCertificate::OrganizationalUnitName).join(", ") + ", ";
    if(!m_cert.issuerInfo(QSslCertificate::LocalityName).isEmpty())
	certIssuer += "L: " + m_cert.issuerInfo(QSslCertificate::LocalityName).join(", ") + ", ";
    if(!m_cert.issuerInfo(QSslCertificate::StateOrProvinceName).isEmpty())
	certIssuer += "ST: " + m_cert.issuerInfo(QSslCertificate::StateOrProvinceName).join(", ") + ", ";
    if(!m_cert.issuerInfo(QSslCertificate::CountryName).isEmpty())
	certIssuer += "C: " + m_cert.issuerInfo(QSslCertificate::CountryName).join(", ") + ", ";
    if(!m_cert.issuerInfo(QSslCertificate::CommonName).isEmpty())
	certIssuer += "CN: " + m_cert.issuerInfo(QSslCertificate::CommonName).join(", ") + ", ";
    certIssuer.chop(2);

    if(!m_cert.subjectInfo(QSslCertificate::Organization).isEmpty())
	certSubject += "O: " + m_cert.subjectInfo(QSslCertificate::Organization).join(", ") + ", ";
    if(!m_cert.subjectInfo(QSslCertificate::OrganizationalUnitName).isEmpty())
	certSubject += "OU: " + m_cert.subjectInfo(QSslCertificate::OrganizationalUnitName).join(", ") + ", ";
    if(!m_cert.subjectInfo(QSslCertificate::LocalityName).isEmpty())
	certSubject += "L: " + m_cert.subjectInfo(QSslCertificate::LocalityName).join(", ") + ", ";
    if(!m_cert.subjectInfo(QSslCertificate::StateOrProvinceName).isEmpty())
	certSubject += "ST: " + m_cert.subjectInfo(QSslCertificate::StateOrProvinceName).join(", ") + ", ";
    if(!m_cert.subjectInfo(QSslCertificate::CountryName).isEmpty())
	certSubject += "C: " + m_cert.subjectInfo(QSslCertificate::CountryName).join(", ") + ", ";
    if(!m_cert.subjectInfo(QSslCertificate::CommonName).isEmpty())
	certSubject += "CN: " + m_cert.subjectInfo(QSslCertificate::CommonName).join(", ") + ", ";
    certSubject.chop(2);

    setText(tr("The cluster certificate is issued by an <b>unknown entity</b><br>") +
	    tr("Certificate details:<ul>") +
	    tr("<li>Issuer: ") + certIssuer +
	    tr("<li>Subject: ") + certSubject +
	    tr("<li>SHA1 Fingerprint: ") + m_certFps.toHex() +
	    tr("</ul><b>Do you trust this certificate ?</b>"));
}

