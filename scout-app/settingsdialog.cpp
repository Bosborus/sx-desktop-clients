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

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "logsmodel.h"
#include "versioncheck.h"
#include "sizevalidator.h"
#include "util.h"
#include <QMessageBox>

SettingsDialog::SettingsDialog(ScoutConfig *config, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SettingsDialog)
{
    mConfig = config;
    ui->setupUi(this);
    ui->labelVersion->setText(QApplication::applicationVersion());
    initializeGeneralPage(config);
    initializeLoggingPage();
    connect(ui->listWidget, &QListWidget::currentRowChanged, this, &SettingsDialog::selectPage);
    selectPage(0);
    connect(VersionCheck::instance(), &VersionCheck::noNewVersion, this, &SettingsDialog::onNoNewVersion);
    mVersionCheckParent = VersionCheck::instance()->parentWidget();
}

SettingsDialog::~SettingsDialog()
{
    VersionCheck::instance()->setParentWidget(mVersionCheckParent);
    delete ui;
}

void SettingsDialog::initializeGeneralPage(const ScoutConfig *config)
{
    auto cc = config->clusterConfig();
    ui->labelClustername->setText(cc->cluster());
    if (cc->address().isEmpty()) {
        ui->labelInitialAddress0->setVisible(false);
        ui->labelInitialAddress1->setVisible(false);
    }
    else
        ui->labelInitialAddress1->setText(cc->address());
    ui->labelUser->setText(cc->username());
    if (cc->ssl())
        ui->labelPort->setText(QString("%1 (ssl)").arg(cc->port()));
    else
        ui->labelPort->setText(QString("%1").arg(cc->port()));
    ui->checkBoxCheckVersion->setChecked(config->checkVersion());
    if (config->checkVersion())
        ui->checkBoxCheckBetaVersion->setChecked(config->checkBetaVersion());
    else
        ui->checkBoxCheckBetaVersion->setChecked(false);

    ui->checkBoxCache->setChecked(config->cacheEnabled());
    on_checkBoxCache_toggled(config->cacheEnabled());
    ui->lineEditCacheSize->setValidator(new SizeValidator(this));
    ui->lineEditCacheSize->setText(formatSize(config->cacheSize()));
    ui->lineEditCacheFilesizeLimit->setValidator(new SizeValidator(this));
    ui->lineEditCacheFilesizeLimit->setText(formatSize(config->cacheFileLimit()));
}

void SettingsDialog::initializeLoggingPage()
{
    ui->tableView->setModel(LogsModel::instance());
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
}

void SettingsDialog::on_buttonStartWizard_clicked()
{
    close();
    emit showWizard();
}

void SettingsDialog::selectPage(int index)
{
    ui->listWidget->setCurrentRow(index);
    ui->stackedWidget->setCurrentIndex(index);
}

void SettingsDialog::on_buttonBox_accepted()
{
    if (mConfig->checkVersion() != ui->checkBoxCheckVersion->isChecked() ||
            mConfig->checkBetaVersion() != ui->checkBoxCheckBetaVersion->isChecked()) {
        mConfig->setCheckVersion(ui->checkBoxCheckVersion->isChecked(), ui->checkBoxCheckBetaVersion->isChecked());
        VersionCheck::instance()->setEnabled(ui->checkBoxCheckVersion->isChecked());
        VersionCheck::instance()->setCheckingBeta(ui->checkBoxCheckBetaVersion->isChecked());
    }
    SizeValidator sv;
    qint64 cacheSize = sv.parseSize(ui->lineEditCacheSize->text());
    qint64 cacheFileSize = sv.parseSize(ui->lineEditCacheFilesizeLimit->text());
    mConfig->setCacheEnabled(ui->checkBoxCache->isChecked());
    mConfig->setCacheSize(cacheFileSize, cacheSize);
    accept();
}

void SettingsDialog::on_buttonBox_rejected()
{
    close();
}

void SettingsDialog::on_checkBoxCheckVersion_toggled(bool checked)
{
    ui->checkBoxCheckBetaVersion->setEnabled(checked);
    if (!checked)
        ui->checkBoxCheckBetaVersion->setChecked(false);
}

void SettingsDialog::on_buttonCheckNow_clicked()
{
    VersionCheck::instance()->resetVersionCheck();
    VersionCheck::instance()->setParentWidget(this);
    VersionCheck::instance()->checkVersionShowResult(ui->checkBoxCheckBetaVersion->isChecked());
}

void SettingsDialog::onNoNewVersion(bool showResult)
{
    if (!showResult)
        return;
    QMessageBox::information(this, tr("%1 Update").arg(QApplication::applicationName()), tr("No new version available"));
    VersionCheck::instance()->setCheckingBeta(mConfig->checkBetaVersion());
}

void SettingsDialog::onVersionCheckError(bool showResult, const QString &errorMsg)
{
    if (!showResult)
        return;
    QMessageBox::warning(this, tr("%1 Update").arg(QApplication::applicationName()), tr("Checking for new version failed\n")+errorMsg);
    VersionCheck::instance()->setCheckingBeta(mConfig->checkBetaVersion());
}

void SettingsDialog::on_checkBoxCache_toggled(bool checked)
{
    ui->lineEditCacheSize->setEnabled(checked);
    ui->lineEditCacheFilesizeLimit->setEnabled(checked);
}
