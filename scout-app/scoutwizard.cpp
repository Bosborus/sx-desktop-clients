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

#include "scoutwizard.h"
#include "wizard/sxwizardpage.h"
#include "translations.h"
#include <QApplication>

ScoutWizard::ScoutWizard(ScoutConfig *config, QWidget *parent) : SxWizard (parent)
{
    mConfig = config;
}

QString ScoutWizard::setupCompleteText() const
{
    return  tr("Setup complete!")+"<br><br>"+
            tr("Now you can browse your volumes and manage your files easily.")+"<br>"+
            tr("SXScout gives you all advanced features of SX, including replication, deduplication and client-side encryption.");
}

QString ScoutWizard::applicationName() const
{
    return QApplication::applicationName();
}

QString ScoutWizard::wwwDocs() const
{
    return "http://www.skylable.com";
}

QString ScoutWizard::hardcodedCluster() const
{
    return "";
}

QString ScoutWizard::hardcodedSxAuthd() const
{
    return "";
}

QString ScoutWizard::hardcodedClusterDomain() const
{
    return "";
}

void ScoutWizard::_addPages()
{
    addPage(pageStart());
    addPage(pageConnectTo());
    addPage(pagetComplete());
}

void ScoutWizard::_restoreSettings()
{
    if (mConfig->isValid()) {
        setSxCluster(mConfig->clusterConfig()->cluster());
        setClusterUuid(mConfig->clusterConfig()->uuid());
        setSxAddress(mConfig->clusterConfig()->address());
        setUsername(mConfig->clusterConfig()->username());
        setUseSsl(mConfig->clusterConfig()->ssl());
        setSslPort(mConfig->clusterConfig()->port());
        setLanguage("en");
    }
    else {
        setSxCluster("");
        setClusterUuid("");
        setSxAddress("");
        setUsername("");
        setUseSsl(true);
        setSslPort(443);
        setLanguage(QLocale::system().bcp47Name().split("-").first());
    }
    setSxAuth("");
    setCertFingerprint("");
    setSecondaryCertFingerprint("");
    if (!Translations::instance()->containsLanguage(language()))
        setLanguage("en");
}

void ScoutWizard::_storeSettings()
{
    mConfig->clusterConfig()->setConfig(sxCluster(), clusterUuid(), sxAuth(), sxAddress(), useSsl(), sslPort(), certFingerprint());
    mConfig->clusterConfig()->setUsername(username());
    mConfig->clusterConfig()->setSecondaryClusterCertFp(secondaryCertFingerprint());
    setConfigChanged(true);
}

QString ScoutWizard::_oldClusterUUID() const
{
    if (mConfig->isValid())
        return mConfig->clusterConfig()->uuid();
    return "";
}
