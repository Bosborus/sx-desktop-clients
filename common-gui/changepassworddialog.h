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

#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include "sxcluster.h"

#include "dropframe.h"

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ChangePasswordDialog(QString username, SxCluster* cluster, bool firstPassword, QDialog *parent = 0);
    ~ChangePasswordDialog();
    QString token() const;
signals:
private slots:
    void changePasswordClicked();
    void onPasswordChanged();
    void changePasswordStage2();
private:
    void disableInput();
private:
    QLineEdit *m_password1, *m_password2, *m_oldPassword;
    QPushButton *m_okButton, *m_cancelButton;
    QLabel* errorLabel;
    QString m_username, m_token;
    SxCluster *mCluster;
    bool m_firstCheck;
};

#endif // CHANGEPASSWORDDIALOG_H
