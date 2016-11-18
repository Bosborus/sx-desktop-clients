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

#include "getpassworddialog.h"
#include "ui_getpassworddialog.h"
#include "util.h"
#include "sxcluster.h"
#include <QMessageBox>
#include <memory>
#include <QCloseEvent>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QPushButton>
#include "sxfilter.h"

GetPasswordDialog::GetPasswordDialog(const SxAuth &auth, const QByteArray &clusterUuid, const QString &volume, VolumeEncryptionType type, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GetPasswordDialog)
{
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationName());
    if (type == VolumeEncryptionType::CONFIGURED) {
        ui->labelPassword2->setVisible(false);
        ui->password2->setVisible(false);
        ui->encryptFilenames->setVisible(false);
        ui->labelTitle->setText(tr("Enter encryption password for volume %1").arg(volume));
    }
    else if (type == VolumeEncryptionType::NOT_CONFIGURED_OLD) {
        ui->labelTitle->setText(tr("Set encryption password for volume %1").arg(volume));
        ui->encryptFilenames->setVisible(false);
    }
    else {
        ui->labelTitle->setText(tr("Set encryption password for volume %1").arg(volume));
        ui->encryptFilenames->setCheckState(Qt::Checked);
    }
    mAuth = auth;
    mClusterUuid = clusterUuid;
    mType = type;
    mVolume = volume;
    ui->progressBar->setVisible(false);
    adjustSize();
    mPasswordSizeLimit = 0;
    mIgnoreClose = false;
    mClose = false;
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    connect(ui->password1, &QLineEdit::textChanged, this, &GetPasswordDialog::validate);
    connect(ui->password2, &QLineEdit::textChanged, this, &GetPasswordDialog::validate);
}

GetPasswordDialog::~GetPasswordDialog()
{
    delete ui;
}


VolumeEncryptionType GetPasswordDialog::getVolumeEncryptionType(const MetaHash &meta, const MetaHash &customMeta)
{
    VolumeEncryptionType type = VolumeEncryptionType::NOT_ENCRYPTED;
    if (meta.contains("filterActive")) {
        auto uuid = ActiveFilterUtils::uuidFormat(meta.value("filterActive").toHex());
        if (uuid == "35a5404d-1513-4009-904c-6ee5b0cd8634" || uuid == "15b0ac3c-404f-481e-bc98-6598e4577bbd") {
            bool oldFilter = uuid == "35a5404d-1513-4009-904c-6ee5b0cd8634";
            int config_len = meta.value(uuid+"-cfg").count();
            if (config_len == 17) {
                if (customMeta.contains("aes256_fp"))
                    type = VolumeEncryptionType::CONFIGURED;
                else
                    type = oldFilter ? VolumeEncryptionType::NOT_CONFIGURED_OLD : VolumeEncryptionType::NOT_CONFIGURED_NEW;
            }
            else {
                type = VolumeEncryptionType::CONFIGURED;
            }
        }
    }
    return type;
}

VolumeEncryptionType GetPasswordDialog::getVolumeEncryptionType(const SxVolume *volume)
{
    QHash<QString, QByteArray> meta;
    QHash<QString, QByteArray> customMeta;
    foreach (QString key, volume->meta().keys()) {
        meta.insert(key, volume->meta().value(key));
    }
    foreach (QString key, volume->customMeta().keys()) {
        customMeta.insert(key, volume->customMeta().value(key));
    }
    return getVolumeEncryptionType(meta, customMeta);
}

void GetPasswordDialog::closeEvent(QCloseEvent *event)
{
    if (mIgnoreClose)
        event->ignore();
}

void GetPasswordDialog::accept()
{
    if (mClose)
        QDialog::accept();
}

void GetPasswordDialog::on_buttonBox_accepted()
{
    ui->buttonBox->setVisible(false);
    ui->progressBar->setVisible(true);
    ui->password1->setEnabled(false);
    ui->password2->setEnabled(false);
    ui->encryptFilenames->setEnabled(false);

    removeVolumeFilterConfig(mClusterUuid, mVolume);

    auto getInputCB = [this] (sx_input_args & args) -> int {
        QString prompt = QString::fromUtf8(args.prompt);
        if (args.type == SXC_INPUT_YN) {
            if (prompt == "[aes256]: Enable filename encryption (introduces additional slowdown)?") {
                args.in[0] = ui->encryptFilenames->isChecked() ? 'y' : 'n';
                if (args.insize > 1)
                    args.in[1] = 0;
                return 0;
            }
        }
        else {
            if (prompt == "[aes256]: Enter encryption password: "
                    || prompt == "[aes256]: Re-enter encryption password: ") {
                auto password = ui->password1->text().toUtf8();
                if (static_cast<unsigned int>(password.length())+1 > args.insize) {
                    mPasswordSizeLimit = args.insize-1;
                    return 1;
                }
                memcpy(args.in, password.constData(), static_cast<unsigned int>(password.length()));
                args.in[password.length()]=0;
                return 0;
            }
        }
        return 1;
    };

    auto job = [this, getInputCB]() -> QString {
        mPasswordSizeLimit = 0;
        QString errorMessage;
        std::unique_ptr<SxCluster> cluster(SxCluster::initializeCluster(mAuth, mClusterUuid, [](QSslCertificate&,bool) -> bool {return true;}, errorMessage));
        if (!cluster)
            return "Unable to initialize cluster: " + errorMessage;
        cluster->setFilterInputCallback(getInputCB);
        cluster->reloadVolumes();
        SxVolume *volume = cluster->getSxVolume(mVolume);
        if (!volume)
            return "Unable to find volume";
        QString remoteName = "/.tmp/.sxnewdirTMP";
        QTemporaryFile tempFile(QStandardPaths::writableLocation(QStandardPaths::TempLocation)+"/XXXXXX");
        if (!tempFile.open())
            return "Unable to create temporary file";
        tempFile.resize(1);
        tempFile.close();
        SxFileEntry rev;
        if (!cluster->uploadFile(volume, remoteName, tempFile.fileName(), rev)) {
            if (cluster->lastError().errorMessage()=="[aes256]: Can't obtain password" && mPasswordSizeLimit > 0)
                return QString("Entered password is too long, password size limit: %1").arg(mPasswordSizeLimit);
            else
                return "Failed to upload file: "+ cluster->lastError().errorMessageTr();
        }
        cluster->deleteFile(volume, remoteName);
        return "";
    };

    mIgnoreClose = true;
    QEventLoop eventLoop;
    QFutureWatcher<QString> futureWatcher;
    connect(&futureWatcher, &QFutureWatcher<QString>::finished, &eventLoop, &QEventLoop::quit);
    QFuture<QString> future = QtConcurrent::run(job);
    futureWatcher.setFuture(future);
    eventLoop.exec();
    mIgnoreClose = false;

    ui->buttonBox->setVisible(true);
    ui->progressBar->setVisible(false);
    ui->password1->setEnabled(true);
    ui->password2->setEnabled(true);
    ui->encryptFilenames->setEnabled(true);

    if (!future.result().isEmpty()) {
        QMessageBox::warning(this, QApplication::applicationName(), future.result());
        return;
    }

    mClose = true;
    accept();
}

void GetPasswordDialog::validate()
{
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    QString warning;
    if (ui->password1->text().length() < 8) {
        warning = tr("Password cannot be shorter than 8 characters");
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else if (ui->password2->isVisible()) {
        if (ui->password1->text() != ui->password2->text()) {
            if (!ui->password2->text().isEmpty())
                warning = tr("Entered passwords don't match");
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        }
    }
    if (warning.isEmpty())
        ui->labelWarning->clear();
    else
        ui->labelWarning->setText("<font color='red'>"+warning+"</font>");
}
