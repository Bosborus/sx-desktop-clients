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

#include "changepassworddialog.h"
#include "util.h"

ChangePasswordDialog::ChangePasswordDialog(QString username, SxCluster *cluster, bool firstPassword, QDialog *parent) : QDialog(parent)
{
    m_username = username;
    mCluster = cluster;
    m_firstCheck = true;

    setWindowTitle(tr("Set user password"));

    m_okButton = new QPushButton(tr("OK"));
    m_okButton->setEnabled(false);
    m_cancelButton = new QPushButton(tr("Cancel"));
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    buttonsLayout->addWidget(m_okButton);
    buttonsLayout->addWidget(m_cancelButton);
    QGridLayout *passwordLayout = new QGridLayout();

    QString message;
    if (firstPassword)
    {
        message = tr("When using a configuration file for the first time you have to set a new password for your account.");
        m_oldPassword = 0;
    }
    else
    {
        message = tr("Set a new password for user <b>%1</b>").arg(username.toStdString().c_str());
        m_oldPassword = new QLineEdit();
        m_oldPassword->setEchoMode(QLineEdit::Password);
        passwordLayout->addWidget(new QLabel(tr("Current password:")),0,0);
        passwordLayout->addWidget(m_oldPassword,0,1);
        connect(m_oldPassword, &QLineEdit::textChanged, this, &ChangePasswordDialog::onPasswordChanged);
    }

    m_password1 = new QLineEdit();
    m_password1->setEchoMode(QLineEdit::Password);
    m_password2 = new QLineEdit();
    m_password2->setEchoMode(QLineEdit::Password);
    errorLabel = new QLabel();
    QLabel *messageLabel = new QLabel(message);
    messageLabel->setTextFormat(Qt::RichText);

    passwordLayout->addWidget(new QLabel(tr("New password:")),1,0);
    passwordLayout->addWidget(m_password1,1,1);
    passwordLayout->addWidget(new QLabel(tr("Re-enter password:")),2,0);
    passwordLayout->addWidget(m_password2, 2,1);
    passwordLayout->addWidget(errorLabel, 3,0, 1, 2);

    QVBoxLayout *layout = new QVBoxLayout();
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    setLayout(layout);
    layout->addWidget(messageLabel);
    layout->addSpacerItem(new QSpacerItem(400,20,QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addLayout(passwordLayout);
    layout->addSpacerItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::MinimumExpanding));
    layout->addLayout(buttonsLayout);

    connect(m_okButton, &QPushButton::clicked, this, &ChangePasswordDialog::changePasswordClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_password1, &QLineEdit::textChanged, this, &ChangePasswordDialog::onPasswordChanged);
    connect(m_password2, &QLineEdit::textChanged, this, &ChangePasswordDialog::onPasswordChanged);
    //*/
}

ChangePasswordDialog::~ChangePasswordDialog()
{
}

void ChangePasswordDialog::onPasswordChanged(){
    if (m_oldPassword && m_oldPassword->text().isEmpty())
    {
        errorLabel->setText("");
        m_okButton->setEnabled(false);
    }
    if (m_password1->text().isEmpty() || m_password2->text().isEmpty())
    {
        errorLabel->setText("");
        m_okButton->setEnabled(false);
    }
    else if (m_password1->text()!=m_password2->text())
    {
        errorLabel->setText("<font color='red'>"+tr("Entered passwords don't match"));
        m_okButton->setEnabled(false);
    }
    else if (m_password1->text().length() < 8)
    {
        errorLabel->setText("<font color='red'>"+tr("Password cannot be shorter than 8 characters"));
        m_okButton->setEnabled(false);
    }
    else if (m_oldPassword && m_oldPassword->text() == m_password1->text())
    {
        errorLabel->setText("<font color='red'>"+tr("New password must be different from the current one"));
        m_okButton->setEnabled(false);
    }
    else
    {
        errorLabel->setText("<font color='green'>"+tr("OK"));
        m_okButton->setEnabled(true);
    }
}

void ChangePasswordDialog::changePasswordStage2()
{
    QString error;
    QString newToken = deriveKey(m_username, m_password1->text(), mCluster->uuid(), 12);
    if (m_oldPassword)
    {
        QString oldToken = deriveKey(m_username, m_oldPassword->text(), mCluster->uuid(), 12);
        if (!mCluster->changePassword(oldToken, newToken))
            error = mCluster->lastError().errorMessageTr();
    }
    else {
        if (!mCluster->changePassword(newToken))
            error = mCluster->lastError().errorMessageTr();
    }

    if (!error.isEmpty()) {
        QMessageBox::critical(this, tr("Error"), error);
        m_token.clear();
        reject();
    }
    else {
        m_token =  newToken;
        accept();
    }
}

QString ChangePasswordDialog::token() const
{
    return m_token;
}

void ChangePasswordDialog::changePasswordClicked()
{
    m_okButton->setEnabled(false);
    m_cancelButton->setEnabled(false);
    if (m_oldPassword)
        m_oldPassword->setEnabled(false);
    m_password1->setEnabled(false);
    m_password2->setEnabled(false);
    QTimer::singleShot(1, this, SLOT(changePasswordStage2()));
}
