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

#ifndef SHAREFILEDIALOG_H
#define SHAREFILEDIALOG_H

#include <QDialog>
#include <QList>
#include <QPair>
#include <QNetworkAccessManager>
#include "abstractshareconfig.h"

namespace Ui {
class ShareFileDialog;
}

class ShareFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareFileDialog(AbstractShareConfig* config,const QString& sxwebAddress, const QString& sxshareAddress, const QString& file, QWidget *parent = 0);
    explicit ShareFileDialog(AbstractShareConfig* config,const QString& sxwebAddress, const QString& sxshareAddress, const QString& volume, const QString& file, QWidget *parent = 0);
    ~ShareFileDialog();
    QString errorString() const;
    QString publicLink() const;

private:
    explicit ShareFileDialog(AbstractShareConfig *config, const QString &sxwebAddress, const QString &sxshareAddress, QWidget *parent);
    void disableAll();

public slots:
    void accept() override;

private slots:
    void on_passwordCheckBox_stateChanged(int arg1);
    void on_notifyCheckBox_stateChanged(int arg1);
    void on_browseButton_clicked();
    void validate();
    void onNetworkReply(QNetworkReply* repl);
    void checkSsl(QNetworkReply *reply, const QList<QSslError> &errors);

private:
    Ui::ShareFileDialog *ui;
    const QList< QPair<QString, long> > m_expireList;
    QString m_volume;
    QNetworkAccessManager *m_man;
    QString m_errorString;
    QString m_publink;
    QString m_sxwebAddress;
    QString m_sxshareAddress;
    QString m_file;
    QByteArray m_sxwebCertFingerprint;
    AbstractShareConfig *m_config;
    bool m_directShare;
};

#endif // SHAREFILEDIALOG_H
