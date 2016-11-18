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

#include "revisionsdialog.h"
#include "ui_revisionsdialog.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStandardPaths>
#include "util.h"
#include <QFileDialog>
#include <QMessageBox>

#include "whitelabel.h"

RevisionsDialog::RevisionsDialog(SxConfig *config, QString file, std::function<bool(QSslCertificate &, bool)> checkSslCallback, QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint),
    ui(new Ui::RevisionsDialog)
{
    ui->setupUi(this);
#ifndef Q_OS_MAC
    ui->pushButton->setMaximumWidth(40);
#endif
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Restore"));
    ui->revisions->hideColumn(3);
    ui->revisions->hide();
    m_selectedIndex = 0;
    adjustSize();

    m_config = config;
    QString errorMessage;
    mCluster = SxCluster::initializeCluster(m_config->clusterConfig().sxAuth(), m_config->clusterConfig().uuid(), checkSslCallback, errorMessage);
    if (mCluster == nullptr) {
        QMessageBox::warning(this, __applicationName, errorMessage);
        reject();
        return;
    }
    if (selectFile(file))
        hideBrowseButton();
    connect(ui->revisions, &QTableWidget::cellClicked, [this](int row, int) {
        if (row<0 || row>=ui->revisions->rowCount())
            return;
        auto item = qobject_cast<QRadioButton*>(ui->revisions->cellWidget(row, 0));
        if (item)
            item->click();
    });
}

RevisionsDialog::~RevisionsDialog()
{
    delete ui;
    if (mCluster != nullptr)
        delete mCluster;
}

bool RevisionsDialog::selectFile(QString file)
{
    if (file.isEmpty())
        return false;

    bool failed = true;
    foreach(QString volume, m_config->volumes()) {
        QString localDir = m_config->volume(volume).localPath();
        if (!localDir.endsWith("/"))
            localDir+="/";
        if (file.startsWith(localDir)) {
            m_volume = volume;
            failed = false;
            break;
        }
    }
    if (failed) {
        QMessageBox::warning(this, tr("Warning"), tr("File must be in local sync directory"));
        return false;
    }

    ui->revisions->clearContents();
    ui->revisions->setRowCount(0);
    m_file = file;
    ui->file->setText(file);
    ui->revisions->setVisible(false);
    ui->messageLabel->setVisible(true);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    QTimer::singleShot(0, this, SLOT(getFileRevisions()));
    return true;
}

void RevisionsDialog::setBrowsingEnabled(bool enabled)
{
    ui->pushButton->setEnabled(enabled);
}

QString RevisionsDialog::revision() const
{
    if (!m_selectedIndex)
        return QString();
    auto item = ui->revisions->item(m_selectedIndex, 3);
    if (!item)
        return QString();
    return item->text();
}

QString RevisionsDialog::file() const
{
    return m_file;
}

void RevisionsDialog::accept()
{
    QString file_dir = QFileInfo(m_file).absolutePath();
    QString filename;
    QString rev = m_createdAt;

    filename = m_file.split("/").last();
    int index = filename.lastIndexOf(".");
    if (index==-1)
        filename+="_"+rev;
    else {
        QString ext;
        ext = filename.mid(index);
        filename = filename.left(index)+"_"+rev+ext;
    }

    QFileDialog dialog(this);
    dialog.setDirectory(file_dir);
    dialog.selectFile(filename);
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (!dialog.exec())
        return;

    QString localDir = m_config->volume(m_volume).localPath();
    if (!localDir.endsWith("/"))
        localDir+="/";
    QString file = dialog.selectedFiles().first();
    if (!file.startsWith(localDir)) {
        QMessageBox::warning(this, __applicationName, tr("Unable to restore file outside local sync directory"));
        return;
    }
    QString source = m_file.mid(localDir.size()-1);
    QString target = file.mid(localDir.size()-1);

    QDialog::accept();
    SxVolume *volume = mCluster->getSxVolume(m_volume);
    auto selectedRevision = ui->revisions->item(m_selectedIndex, 3)->text();
    if (!mCluster->restoreFileRevision(volume, source, selectedRevision, target)) {
        QMessageBox::warning(this, __applicationName, mCluster->lastError().errorMessageTr());
    }
}

void RevisionsDialog::hideBrowseButton()
{
    ui->pushButton->hide();
}

void RevisionsDialog::getFileRevisions()
{
    ui->messageLabel->setVisible(true);
    ui->revisions->setVisible(false);
    ui->messageLabel->setText(tr("Waiting for file revisions"));

    if (!mCluster->reloadVolumes()) {
        ui->messageLabel->setText(mCluster->lastError().errorMessageTr());
        return;
    }

    QString localPath = m_config->volume(m_volume).localPath();
    if (!localPath.endsWith("/"))
        localPath+="/";
    QString file = m_file.mid(localPath.size()-1);
    SxVolume *volume = mCluster->getSxVolume(m_volume);
    QList<std::tuple<QString, qint64, quint32>> list;
    if (!mCluster->listFileRevisions(volume, file, list)) {
        ui->messageLabel->setText(mCluster->lastError().errorMessageTr());
        return;
    }
    ui->revisions->clearContents();
    int count = list.count();
    foreach (auto rev, list) {
        QString revStr = std::get<0>(rev);
        QString sizeStr = formatSize(std::get<1>(rev));
        QDateTime date = QDateTime::fromTime_t(std::get<2>(rev));
        QString dateStr = QDateTime::fromTime_t(std::get<2>(rev)).toString("yyyyMMdd-hhmmss");

        ui->revisions->insertRow(0);
        auto rb = new QRadioButton();
        --count;
        if (!count) {
            rb->setChecked(true);
        }
        connect(rb, &QRadioButton::clicked, [this, count, dateStr]() {
            m_selectedIndex = count;
            m_createdAt = dateStr;
            ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(count);
        });

        ui->revisions->setCellWidget(0, 0, rb);
        ui->revisions->setItem(0, 1, new QTableWidgetItem(date.toString(Qt::SystemLocaleLongDate)));
        ui->revisions->setItem(0, 2, new QTableWidgetItem(sizeStr));
        ui->revisions->setItem(0, 3, new QTableWidgetItem(revStr));
    }
    ui->revisions->resizeColumnsToContents();
    ui->messageLabel->setVisible(false);
    ui->revisions->setVisible(true);

    int rowHeight = ui->revisions->rowHeight(0);
    if (rowHeight<=40)
        rowHeight = 40;
    int rowCount = ui->revisions->rowCount()+1;
    if (rowCount > 5)
        rowCount = 5;
    ui->revisions->setMaximumHeight(rowCount*rowHeight);
    adjustSize();
}

int RevisionsDialog::exec()
{
    if (mCluster == nullptr)
        return QDialog::Rejected;
    return QDialog::exec();
}

void RevisionsDialog::on_pushButton_clicked()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    if (dialog.exec()) {
        QString file = dialog.selectedFiles().first();
        selectFile(file);
    }
}
