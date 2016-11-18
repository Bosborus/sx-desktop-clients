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

#include "sxwizardpage.h"
#include "certdialog.h"

SxWizardPage::SxWizardPage(SxWizard *wizard) :
    QWizardPage(wizard)
{
    mWizard = wizard;
}

SxWizardPage::~SxWizardPage()
{
}

SxWizard *SxWizardPage::sxWizard() const
{
    return mWizard;
}

QByteArray SxWizardPage::certFingerprint() const
{
    return sxWizard()->certFingerprint();
}

void SxWizardPage::setCertFingerprint(QByteArray fingerprint)
{
    sxWizard()->setCertFingerprint(fingerprint);
}

QByteArray SxWizardPage::secondaryCertFingerprint() const
{
    return sxWizard()->secondaryCertFingerprint();
}

void SxWizardPage::setSecondaryCertFingerprint(QByteArray fingerprint)
{
    sxWizard()->setSecondaryCertFingerprint(fingerprint);
}

QString SxWizardPage::sxCluster() const
{
    return sxWizard()->sxCluster();
}

void SxWizardPage::setSxCluster(const QString &sxCluster)
{
    sxWizard()->setSxCluster(sxCluster);
}

QString SxWizardPage::sxAuth() const
{
    return sxWizard()->sxAuth();
}

void SxWizardPage::setSxAuth(const QString &sxAuth)
{
    sxWizard()->setSxAuth(sxAuth);
}

QString SxWizardPage::sxAddress() const
{
    return sxWizard()->sxAddress();
}

void SxWizardPage::setSxAddress(const QString &sxAddress)
{
    sxWizard()->setSxAddress(sxAddress);
}

bool SxWizardPage::useSsl() const
{
    return sxWizard()->useSsl();
}

void SxWizardPage::setUseSsl(bool useSsl)
{
    sxWizard()->setUseSsl(useSsl);
}

int SxWizardPage::sslPort() const
{
    return sxWizard()->sslPort();
}

void SxWizardPage::setSslPort(int sslPort)
{
    sxWizard()->setSslPort(sslPort);
}

QString SxWizardPage::clusterUuid() const
{
    return sxWizard()->clusterUuid();
}

QString SxWizardPage::oldClusterUuid() const
{
    return sxWizard()->oldClusterUuid();
}

void SxWizardPage::setClusterUuid(const QString &uuid)
{
    sxWizard()->setClusterUuid(uuid);
}

QString SxWizardPage::username() const
{
    return sxWizard()->username();
}

void SxWizardPage::setUsername(const QString &username)
{
    sxWizard()->setUsername(username);
}

QString SxWizardPage::lastAddress() const
{
    return sxWizard()->lastAddress();
}

void SxWizardPage::setLastAddress(const QString &lastAddress)
{
    sxWizard()->setLastAddress(lastAddress);
}

QString SxWizardPage::vcluster() const
{
    return sxWizard()->vcluster();
}

void SxWizardPage::setVcluster(const QString &vcluster)
{
    sxWizard()->setVcluster(vcluster);
}
