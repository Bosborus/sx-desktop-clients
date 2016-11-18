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

#include "wizardchoosevolumepage.h"
#include "ui_wizardchoosevolumepage.h"
#include "wizard/sxwizard.h"
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringListModel>
#include <sxfilter.h>
#include "drivewizard.h"

#include "whitelabel.h"

WizardChooseVolumePage::WizardChooseVolumePage(SxWizard *wizard) :
    SxWizardPage(wizard),
    ui(new Ui::WizardChooseVolumePage),
    mAuth("", "", false, 0, "")
{
    ui->setupUi(this);

    if (isRetina())
        ui->label_3->setPixmap(QPixmap(":/icons/progress2of3@2x.png"));
    else
        ui->label_3->setPixmap(QPixmap(":/icons/progress2of3.png"));

    if (isRetina())
    {
        ui->logo->setPixmap(QPixmap(":/icons/systemicon@2x.png"));
        ui->label_volume->setPixmap(QPixmap(":/icons/volumes-icon@2x"));
    }
    mCluster = nullptr;
    scrollArea = new VolumesWidget(this);
    ui->contentLayout->addWidget(scrollArea);
}

WizardChooseVolumePage::~WizardChooseVolumePage()
{
    delete ui;
    if (mCluster)
        delete mCluster;
}

void WizardChooseVolumePage::cleanupPage()
{
    setMinimumHeight(0);
}

void WizardChooseVolumePage::initializePage()
{
    setMinimumHeight(400);
    if (mCluster != nullptr) {
        delete mCluster;
    }
    QTimer::singleShot(0, this, SLOT(loadVolumes()));
}

bool WizardChooseVolumePage::isComplete() const
{
    return true;
}

bool WizardChooseVolumePage::validatePage()
{
    this->setSelectedVolumes(scrollArea->selectedVolumes());
    return true;
}

void WizardChooseVolumePage::loadVolumes()
{
    mAuth = SxAuth(this->sxCluster(), this->sxAddress(), this->useSsl(), this->sslPort(), this->sxAuth());
    scrollArea->setAuth(mAuth);
    scrollArea->setClusterUuid(this->clusterUuid().toUtf8());
    QString errorMessage;
    QByteArray uuid = this->clusterUuid().toUtf8();
    auto checkSsl = [this](const QSslCertificate &cert, bool secondaryCert)->bool {
        QCryptographicHash sha1(QCryptographicHash::Sha1);
        sha1.addData(cert.toDer());
        QByteArray fprint = sha1.result();
        if (secondaryCert)
            return fprint == secondaryCertFingerprint();
        else
            return fprint == certFingerprint();
    };
    mCluster = SxCluster::initializeCluster(mAuth, uuid, checkSsl, errorMessage);
    if (mCluster == nullptr) {
        scrollArea->showMessage(tr("%1 was unable to download volume list").arg(__applicationName).arg(errorMessage));
        return;
    }
    bool result = mCluster->reloadVolumes();
    if (result) {
        auto volumes = mCluster->volumeList();
        QList<SxVolumeEntry> list;
        QSet<QString> lockedVolumes;
        QHash<QString, VolumeEncryptionType> encryptedVolumesTypes;

        foreach (auto volume, volumes) {
            bool unsupportedFilter = true;
            if (SxFilter::isFilterSupported(volume)) {
                unsupportedFilter = false;
                VolumeEncryptionType type = GetPasswordDialog::getVolumeEncryptionType(volume);
                if (type != VolumeEncryptionType::NOT_ENCRYPTED)
                    encryptedVolumesTypes.insert(volume->name(), type);
            }
            list.append({volume->name(), volume->usedSize(), volume->size(), unsupportedFilter});
        }
        QHash<QString, QString> config;
        if (this->clusterUuid() == this->oldClusterUuid())
            config = this->selectedVolumes();
        scrollArea->updateVolumes(mCluster->userInfo().vcluster(), list, config, lockedVolumes, encryptedVolumesTypes);
    }
    else {
        scrollArea->showMessage(tr("%1 was unable to download volume list").arg(__applicationName).arg(mCluster->lastError().errorMessageTr()));
    }
}

void WizardChooseVolumePage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
    else
        QWizardPage::changeEvent(event);
}

QHash<QString, QString> WizardChooseVolumePage::selectedVolumes() const
{
    auto wizard = qobject_cast<DriveWizard*>(sxWizard());
    if (wizard == nullptr)
        return QHash<QString, QString>();
    return wizard->selectedVolumes();
}

void WizardChooseVolumePage::setSelectedVolumes(const QHash<QString, QString> &selectedVolumes)
{
    auto wizard = qobject_cast<DriveWizard*>(sxWizard());
    if (wizard == nullptr)
        return;
    return wizard->setSelectedVolumes(selectedVolumes);
}
