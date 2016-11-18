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

#include "whatsnew.h"
#include "ui_whatsnew.h"
#include <QNetworkRequest>
#include <QDebug>
#include "sxversion.h"

#include "whitelabel.h"

static const QString version = QString(SXVERSION);
const QString WhatsNew::url = QString(version.contains(".beta.")?"http://beta.s3.indian.skylable.com/":"http://cdn.skylable.com/") + "check/sxdrive-"+version+".changelog";

WhatsNew::WhatsNew(QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
    ui(new Ui::WhatsNew)
{
    ui->setupUi(this);
    m_finished = false;
    connect(ui->pushButton, &QPushButton::clicked, this, &WhatsNew::close);
    connect(this, &WhatsNew::destroyed, this, &WhatsNew::deleteLater);
    connect(&m_networkAccessManager, &QNetworkAccessManager::finished, this, &WhatsNew::onNetworkReply);
}

WhatsNew::~WhatsNew()
{
    delete ui;
}

void WhatsNew::showWhatsNew()
{
    if (m_finished) {
        show();
        raise();
        activateWindow();
    }
    else
    {
        QNetworkReply* reply = m_networkAccessManager.get(QNetworkRequest(QUrl(url)));
        if (!reply)
        {
            qWarning() << Q_FUNC_INFO << "create network request failed";
        }
        mBytesReceived = 0;
        connect(reply, &QNetworkReply::downloadProgress, [this, reply](qint64 received, qint64 total) {
            if (total > sMessageSizeLimit) {
                reply->abort();
                return;
            }
            mBytesReceived += received;
            if (mBytesReceived > sMessageSizeLimit)
                reply->abort();
        });
    }
}

void WhatsNew::onNetworkReply(QNetworkReply *reply)
{
    reply->deleteLater();
    m_finished=true;
    if (reply->error()==QNetworkReply::NetworkError::NoError)
    {
        auto message = reply->readAll();
#ifndef NO_WHITELABEL
        message.replace("SXDrive", __applicationName.toUtf8());
#endif
        ui->textEdit->setText(message);
        show();
    }
    else
    {
        qWarning() << "Unable to load whats new." << reply->errorString();
    }
}
