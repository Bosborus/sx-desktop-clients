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

#include "drivewizard.h"
#include "translations.h"
#include <QLocale>
#include "whitelabel.h"
#include <QApplication>

DriveWizard::DriveWizard(SxConfig *config, QWidget *parent)
    : SxWizard (parent)
{
    m_config = config;
    restoreSetings(config);
    mVolumePage = nullptr;
}

QHash<QString, QString> DriveWizard::selectedVolumes() const
{
    return mSelectedVolumes;
}

void DriveWizard::setSelectedVolumes(const QHash<QString, QString> &selectedVolumes)
{
    mSelectedVolumes = selectedVolumes;
}

void DriveWizard::restoreSetings(SxConfig *config)
{
    if (config->isValid()) {
        setSxCluster(config->clusterConfig().cluster());
        setSxAddress(config->clusterConfig().address());
        setUseSsl(config->clusterConfig().ssl());
        setSslPort(config->clusterConfig().port());
        setCertFingerprint(config->clusterConfig().clusterCertFp());
        setUsername(config->clusterConfig().username());
        setLanguage(config->desktopConfig().language());

        foreach (QString volume, config->volumes()) {
            mSelectedVolumes.insert(volume, config->volume(volume).localPath());
        }
    }
    else {
        setSxCluster("");
        setSxAddress("");
        setUseSsl(true);
        setSslPort(443);
        setCertFingerprint("");
        setUsername("");
        setLanguage(QLocale::system().bcp47Name().split("-").first());
        if (!Translations::instance()->containsLanguage(language()))
            setLanguage("en");
    }
}

void DriveWizard::saveSettings()
{
    if (m_config->clusterConfig().uuid() != clusterUuid())
        m_config->clear();
    m_config->clusterConfig().setConfig(sxCluster(), clusterUuid(), sxAuth(), sxAddress(), useSsl(), sslPort(), certFingerprint());
    m_config->clusterConfig().setUsername(username());
    m_config->desktopConfig().setLanguage(language());

    auto new_volumes = selectedVolumes();
    if (oldClusterUuid() != clusterUuid()) {
        foreach (QString volume, m_config->volumes()) {
            m_config->removeVolumeConfig(volume);
        }
    } else {
        foreach (QString volume, m_config->volumes()) {
            if (!new_volumes.contains(volume))
                m_config->removeVolumeConfig(volume);
        }
    }
    foreach (QString volume, new_volumes.keys()) {
        m_config->addVolumeConfig(volume, new_volumes.value(volume));
    }
    m_config->syncConfig();
    setConfigChanged(true);
}

QString DriveWizard::applicationName() const
{
    return __applicationName;
}

QString DriveWizard::wwwDocs() const
{
    return __wwwDocs;
}

QString DriveWizard::hardcodedCluster() const
{
    return __hardcodedCluster;
}

QString DriveWizard::hardcodedSxAuthd() const
{
    return __hardcodedSxAuthd;
}

QString DriveWizard::hardcodedClusterDomain() const
{
    return __hardcodedClusterDomain;
}

void DriveWizard::_addPages()
{
    mVolumePage = new WizardChooseVolumePage(this);
    addPage(pageStart());
    addPage(pageConnectTo());
    addPage(mVolumePage);
    addPage(pagetComplete());
}

void DriveWizard::_restoreSettings()
{
    restoreSetings(m_config);
}

void DriveWizard::_storeSettings()
{
    saveSettings();
}

QString DriveWizard::_oldClusterUUID() const
{
    if (m_config->isValid())
        return m_config->clusterConfig().uuid();
    else
        return "";
}

QString DriveWizard::setupCompleteText() const
{
    return "<html><head/><body><p>" +
            QApplication::translate("WizardSetupCompletePage", "Your files will start syncing shortly.")+
            "</p><p>"+
            QApplication::translate("WizardSetupCompletePage", "From now on, when you copy or save files to the local %1 folder they will be automatically synchronized to the cloud and to your other %1 devices.")
            .arg(__applicationName)+
            "</p><p>"+
            QApplication::translate("WizardSetupCompletePage", "You can change the settings and control the application by right clicking the %1 icon on the tray.")
            .arg(__applicationName)+
            "</p><p>"+
            QApplication::translate("WizardSetupCompletePage", "Check out %1 for more info.").arg("<a href=\"%1\"><span style=\" text-decoration: underline; color:#0000ff;\">%1</span></a>")
            .arg(__wwwAbout)+
            "</p></body></html>";
}
