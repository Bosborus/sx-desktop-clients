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

#include "downloaddialog.h"
#include "ui_downloaddialog.h"

DownloadDialog::DownloadDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::DownloadDialog)
{
    ui->setupUi(this);
    ui->label->setText(tr("%1 is downloading a new version").arg(QApplication::applicationName()));
    setWindowTitle(tr("%1 Updater").arg(QApplication::applicationName()));
}

DownloadDialog::~DownloadDialog()
{
    delete ui;
    connect(this, &DownloadDialog::destroyed, this, &DownloadDialog::deleteLater);
}

void DownloadDialog::setDownloadProgress(quint64 downloaded, quint64 size)
{
    if (size)
    {
        float p = (float)downloaded/(float)size;
        ui->progressBar->setValue(1000*p);
        ui->progressBar->setMaximum(1000);
    }
    else {
        ui->progressBar->setValue(0);
        ui->progressBar->setMaximum(0);
    }
}

void DownloadDialog::on_pushButton_clicked()
{
    close();
}

void DownloadDialog::closeEvent(QCloseEvent *)
{
    deleteLater();
}
