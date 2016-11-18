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

#include "detailsdialog.h"
#include "ui_detailsdialog.h"
#include <QFileDialog>
#include <QRadioButton>
#include <QStandardPaths>
#include <QTimer>
#include "util.h"
#include "mainwindow.h"
#include "scoutmodel.h"

DetailsDialog::DetailsDialog(ScoutModel *model, const QStringList &files, QWidget *parent)
    : QDialog (parent),
      ui(new Ui::DetailsDialog)
{
    ui->setupUi(this);
    mModel = model;
    mVolume = mModel->currentVolume();
    mDir = mModel->currentPath();
    mFiles = files;
    mSelectedRevision = nullptr;

    ui->labelName->setText(files.join(","));
    if (files.count() == 1) {
        if (files.first().endsWith("/")) {
            ui->labelType->setText(tr("directory"));
            ui->tabRevisions->setEnabled(false);
            ui->labelImage->setPixmap(QPixmap(":/remoteBrowser/directory.png"));
        }
        else {
            QString extension;
            int index = files.first().lastIndexOf('.');
            if (index >= 0) {
                extension = files.first().mid(index+1);
                if (extension.contains("/"))
                    extension.clear();
            }
            QString mimeType = ScoutModel::getMimeType(extension);
            ui->labelType->setText(mimeType);
            ui->buttonDownloadRevision->setEnabled(false);
            QPixmap mimeIcon = MainWindow::iconForMimeType(mimeType, false);
            ui->labelImage->setPixmap(mimeIcon);
        }
    }
    else {
        ui->labelType->setText(tr("multiple objects"));
        ui->tabRevisions->setEnabled(false);
        ui->labelImage->setPixmap(QPixmap(":/remoteBrowser/files.png"));
    }
    ui->labelContent->setText(tr("counting objects..."));
    ui->labelVolume->setText(mVolume);
    ui->labelPath->setText(mDir);
    ui->revisionsTableWidget->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    connect(ui->revisionsTableWidget, &QTableWidget::clicked, this, &DetailsDialog::tableWidgetClicked);
    connect(ui->buttonDownloadRevision, &QPushButton::clicked, this, &DetailsDialog::downloadClicked);
    QTimer::singleShot(0, this, SLOT(refresh()));
}

DetailsDialog::~DetailsDialog()
{
    delete ui;
}

void DetailsDialog::refresh()
{
    auto count = mModel->countFiles(mVolume, mFiles);
    QString content = tr("%n object(s) of size %1", "", count.first).arg(formatSize(count.second));
    ui->labelContent->setText(content);
    if (!ui->tabRevisions->isEnabled())
        return;
    mSelectedRevision = nullptr;
    ui->buttonDownloadRevision->setEnabled(false);
    auto revisions = mModel->getRevisions(mVolume, mFiles.first());
    auto table = ui->revisionsTableWidget;
    table->setRowCount(0);
    table->setRowCount(revisions.count());

    for (int i=0; i<revisions.count(); i++) {

        auto revRef = revisions.at(revisions.count()-1-i);

        auto revStr = revRef.first;
        auto size = revRef.second;
        auto sizeStr = formatSize(size);

        QRadioButton *button = new QRadioButton();
        button->setProperty("index", i);
        button->setProperty("rev", revStr);
        button->setProperty("fileSize", size);
        table->setCellWidget(i, 0, button);
        table->setItem(i, 1, new QTableWidgetItem(revStr));
        table->setItem(i, 2, new QTableWidgetItem(sizeStr));
        connect(button, &QRadioButton::clicked, [this, button](){
            int index = button->property("index").toInt();
            ui->revisionsTableWidget->selectRow(index);
            if (mSelectedRevision != nullptr)
                mSelectedRevision->setChecked(false);
            mSelectedRevision = button;
            ui->buttonDownloadRevision->setEnabled(true);
        });
    }
    if (revisions.count()>0) {
        table->resizeColumnToContents(0);
        table->resizeColumnToContents(2);
    }
}

void DetailsDialog::tableWidgetClicked(const QModelIndex &index)
{
    int row = index.row();
    QRadioButton *button = qobject_cast<QRadioButton *>(ui->revisionsTableWidget->cellWidget(row, 0));
    if (button == nullptr)
        return;
    if (mSelectedRevision != nullptr)
        mSelectedRevision->setChecked(false);
    mSelectedRevision = button;
    mSelectedRevision->setChecked(true);
    ui->buttonDownloadRevision->setEnabled(true);
}

void DetailsDialog::downloadClicked()
{
    if (mSelectedRevision == nullptr)
        return;
    QString revision = mSelectedRevision->property("rev").toString();
    if (revision.isEmpty())
        return;
    QString remoteFile = mFiles.first();
    int index = remoteFile.lastIndexOf('/');
    if (index < 0)
        return;
    QString filename = remoteFile.mid(index);
    QFileDialog d(this);
    d.selectFile(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)+filename);
    d.setAcceptMode(QFileDialog::AcceptSave);
    d.setFileMode(QFileDialog::AnyFile);
    if (!d.exec())
        return;
    QString localFile = d.selectedFiles().first();
    auto size =  mSelectedRevision->property("fileSize");
    mModel->requestDownload(mVolume, remoteFile, revision, size.toLongLong(), localFile);
    emit openTasksDialog();
    close();
}
