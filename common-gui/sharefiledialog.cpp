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

#include "sharefiledialog.h"
#include "ui_sharefiledialog.h"
#include <QFileDialog>
#include <QDebug>
#include <QCloseEvent>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QStandardPaths>
#include "certdialog.h"
#include "sxlog.h"
#include "util.h"

ShareFileDialog::ShareFileDialog(AbstractShareConfig *config, const QString &sxwebAddress, const QString &sxshareAddress, QWidget *parent) :
    QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint),
    ui(new Ui::ShareFileDialog),
    m_expireList({
        {tr("One day"), 24},
        {tr("One week"), 24*7},
        {tr("Two weeks"), 24*14},
        {tr("One month"), 24*31},
        {tr("One year"), 24*365},
        {tr("Never expire"), 0}
    })
{
    m_config = config;
    m_sxwebAddress = sxwebAddress;
    m_sxshareAddress = sxshareAddress;
    m_sxwebCertFingerprint = m_config->sxwebCertFp();
    qint64 m_lastExpirationTime = m_config->expirationTime();
    m_man = 0;
    ui->setupUi(this);
    ui->progressBar->setVisible(false);
#ifndef Q_OS_MAC
    ui->browseButton->setMaximumWidth(40);
#endif
    foreach (auto pair, m_expireList) {
        ui->expireTimeList->addItem(pair.first);
    }
    ui->expireTimeList->setCurrentText(ShareFileDialog::tr("One month"));
    if (m_lastExpirationTime >= 0) {
        for (int i=0 ;i<m_expireList.count(); i++) {
            auto pair = m_expireList.at(i);
            if (pair.second == m_lastExpirationTime) {
                ui->expireTimeList->setCurrentText(pair.first);
                break;
            }
        }
    }
    if (sxshareAddress.isEmpty()) {
        ui->email->setVisible(false);
        ui->emailLabel->setVisible(false);
        ui->notifyCheckBox->setVisible(false);
    }
    else {
        ui->email->setText(m_config->notifyEmail());
    }
    connect(ui->filename, &QLineEdit::textChanged, this, &ShareFileDialog::validate);
    connect(ui->password1, &QLineEdit::textChanged, this, &ShareFileDialog::validate);
    connect(ui->password2, &QLineEdit::textChanged, this, &ShareFileDialog::validate);
    connect(ui->email, &QLineEdit::textChanged, this, &ShareFileDialog::validate);
}

ShareFileDialog::ShareFileDialog(AbstractShareConfig *config, const QString &sxwebAddress, const QString &sxshareAddress, const QString& file, QWidget *parent)
    : ShareFileDialog(config, sxwebAddress, sxshareAddress, parent)
{
    m_directShare = false;
    if (!file.isEmpty()) {
        ui->browseButton->hide();
        ui->filename->setEnabled(false);
        ui->filename->setText(file);
    }
    validate();
}

ShareFileDialog::ShareFileDialog(AbstractShareConfig *config, const QString &sxwebAddress, const QString &sxshareAddress, const QString &volume, const QString &file, QWidget *parent)
    : ShareFileDialog(config, sxwebAddress, sxshareAddress, parent)
{
    m_directShare = true;
    m_volume = volume;
    m_file = file;
    ui->browseButton->hide();
    ui->filename->setEnabled(false);
    ui->filename->setText(file);
    validate();
}

ShareFileDialog::~ShareFileDialog()
{
    delete ui;
    if (m_man)
        delete m_man;
}

QString ShareFileDialog::errorString() const
{
    return m_errorString;
}

QString ShareFileDialog::publicLink() const
{
    return m_publink;
}

void ShareFileDialog::disableAll()
{
    ui->filename->setEnabled(false);
    ui->browseButton->setEnabled(false);
    ui->expireTimeList->setEnabled(false);
    ui->passwordCheckBox->setEnabled(false);
    ui->password1->setEnabled(false);
    ui->password2->setEnabled(false);
}

void ShareFileDialog::accept()
{
    QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok);
    if (ok)
        ok->setEnabled(false);
    disableAll();
    if (m_man)
    {
        m_man->disconnect();
        m_man->deleteLater();
    }
    m_man = new QNetworkAccessManager();
    connect(m_man, &QNetworkAccessManager::finished, this, &ShareFileDialog::onNetworkReply);
    connect(m_man, &QNetworkAccessManager::sslErrors, this, &ShareFileDialog::checkSsl);
    QString host = (m_sxshareAddress.isEmpty()?m_sxwebAddress:m_sxshareAddress);
    if (host.endsWith("/"))
        host += "api/share";
    else
        host += "/api/share";
    QNetworkRequest req(host);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["access_key"] = m_config->clusterToken();

    QString rootDir = m_config->volumePath(m_volume);
    QString file = m_volume+"/"+m_file.mid(rootDir.length()+1);
    json["path"]=file;

    qint64 expireTime = m_expireList.value(ui->expireTimeList->currentIndex()).second;

    m_config->setExpirationTime(expireTime);
    if (ui->notifyCheckBox->isChecked()) {
        m_config->setNotifyEmail(ui->email->text().trimmed());
    }
    m_config->saveConfig();

    if (expireTime)
        expireTime *= 60*60;
    else
        expireTime = 9999999999;

    json["expire_time"]=expireTime;
    json["password"]=ui->password1->text();
    if (ui->notifyCheckBox->isChecked()) {
        json["notify"] = ui->email->text().trimmed();
    }

    QJsonDocument doc(json);
    auto data = doc.toJson();
    m_man->post(req, data);
    m_errorString.clear();
    m_publink.clear();
    ui->progressBar->setVisible(true);
}

void ShareFileDialog::onNetworkReply(QNetworkReply *repl)
{
    QJsonParseError parseError;
    QJsonDocument doc;
    QByteArray data;
    bool status;

    ui->progressBar->setVisible(false);

    repl->deleteLater();
    if (repl->error()!=QNetworkReply::NoError)
    {
        logWarning("Sharing file failed: "+ repl->errorString());
        logDebug(repl->readAll());
        m_errorString = tr("Network error:")+" "+repl->errorString()+" "+m_errorString;
        goto finish;
    }
    data = repl->readAll();
    doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        m_errorString = tr("Unable to parse reply content. Please check the SXWeb address");
        goto finish;
    }
    if (!doc.isObject() ||
        !doc.object().contains("status"))
    {
        m_errorString = tr("Bad reply content");
        goto finish;
    }
    status = doc.object().value("status").toBool();
    if (!status)
    {
        QString error = doc.object().value("error").toString();
        if (error.isEmpty())
            m_errorString = tr("Bad reply content");
        else
        {
            m_errorString = tr("Server replied: %1").arg(error);
        }
        goto finish;
    }
    m_publink = doc.object().value("publink").toString();
    if (m_publink.isEmpty())
    {
        m_errorString = tr("Reply does not contain public link");
        goto finish;
    }

    finish:
    if (!m_errorString.isEmpty())
    {
        logWarning("Sharing file failed: "+ m_errorString);
        logDebug(data);
    }
    QDialog::accept();
}

void ShareFileDialog::checkSsl(QNetworkReply *reply, const QList<QSslError> &errors)
{
    QSslCertificate peerCert = reply->sslConfiguration().peerCertificate();
    if(peerCert.isNull())
    {
        foreach (QSslCertificate cert, reply->sslConfiguration().peerCertificateChain()) {
            if (!cert.isNull())
            {
                peerCert = cert;
                break;
            }
        }
        if (peerCert.isNull())
            return;
    }
    for(int i=0; i<errors.size(); i++) {
        static const QList<QSslError> untrustedList = {QSslError::HostNameMismatch,
                                                       QSslError::SelfSignedCertificate,
                                                       QSslError::SelfSignedCertificateInChain,
                                                       QSslError::UnableToGetLocalIssuerCertificate,
                                                       QSslError::CertificateUntrusted };
        if(untrustedList.contains(errors.at(i).error())) {
	    QCryptographicHash sha1(QCryptographicHash::Sha1);
	    sha1.addData(peerCert.toDer());
	    QByteArray fprint = sha1.result();
	    if(fprint == m_sxwebCertFingerprint)
		continue;
	    CertDialog certDlg(peerCert, this);
            if(certDlg.exec() == QMessageBox::Yes)
            {
                m_sxwebCertFingerprint = fprint;
                m_config->setSxwebCertFp(m_sxwebCertFingerprint);
            }
            else
                return;
        } else {
            int e = errors.at(i).error();
            Q_UNUSED(e);
            return;
        }
    }
    reply->ignoreSslErrors();
}

void ShareFileDialog::on_passwordCheckBox_stateChanged(int arg1)
{
    if (arg1)
    {
        ui->password1Label->setEnabled(true);
        ui->password1->setEnabled(true);
        ui->password2Label->setEnabled(true);
        ui->password2->setEnabled(true);
    }
    else
    {
        ui->password1Label->setEnabled(false);
        ui->password1->setEnabled(false);
        ui->password1->clear();
        ui->password2Label->setEnabled(false);
        ui->password2->setEnabled(false);
        ui->password2->clear();
    }
    validate();
}

void ShareFileDialog::on_browseButton_clicked()
{
    QFileDialog dlg(this);
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setOption(QFileDialog::DontUseNativeDialog,true);
    dlg.setDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));

    if (!m_sxshareAddress.isEmpty()) {
        connect(&dlg, &QFileDialog::currentChanged, [&dlg](const QString &path)
        {
            QFileInfo finfo(path);
            if (finfo.isDir())
                dlg.setFileMode(QFileDialog::Directory);
            else
                dlg.setFileMode(QFileDialog::ExistingFile);
        });
    }

    if (dlg.exec())
    {
        QString file = dlg.selectedFiles().first();
        ui->filename->setText(file);
    }
}

void ShareFileDialog::validate()
{
    QPushButton *ok = ui->buttonBox->button(QDialogButtonBox::Ok);
    ui->warningLabel->clear();

    if (!m_directShare) {
        m_file.clear();
        if (!ok)
            return;
        ok->setEnabled(true);
        if (ui->filename->text().isEmpty())
        {
            ok->setEnabled(false);
            return;
        }

        QString filename = ui->filename->text();
        QFileInfo fi(filename);
        if (!fi.exists())
        {
            ui->warningLabel->setText(tr("File %1 doesn't exist").arg(fi.absoluteFilePath()));
            ok->setEnabled(false);
            return;
        }
        filename = fi.absoluteFilePath();
        m_volume = "";
        foreach (QString vol, m_config->volumes()) {
            QString localPath = m_config->volumePath(vol)+"/";
            if (filename.startsWith(localPath)) {
                m_volume = vol;
                break;
            }
        }
        if (m_volume.isEmpty())
        {
            ui->warningLabel->setText(tr("File %1 must be in the sync directory").arg(ui->filename->text()));
            ok->setEnabled(false);
            return;
        }
        m_file = filename;
        if (fi.isDir())
            m_file += "/";
    }
    if (ui->passwordCheckBox->isChecked() && ui->password1->text().isEmpty())
    {
        ok->setEnabled(false);
        return;
    }
    if (ui->password1->text() != ui->password2->text())
    {
        if (!ui->password2->text().isEmpty())
            ui->warningLabel->setText(tr("Passwords don't match"));
        ok->setEnabled(false);
        return;
    }
    if (!m_sxshareAddress.isEmpty() && ui->notifyCheckBox->isChecked()) {
        const QRegExp emailRegexp("(?:[a-z0-9!#$%&'*+/=?^_`{|}~-]+(?:\\.[a-z0-9!#$%&'*+/=?^_`{|}~-]+)*|\"(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21\\x23-\\x5b\\x5d-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])*\")@(?:(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)+[a-z0-9](?:[a-z0-9-]*[a-z0-9])?|\\[(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?|[a-z0-9-]*[a-z0-9]:(?:[\\x01-\\x08\\x0b\\x0c\\x0e-\\x1f\\x21-\\x5a\\x53-\\x7f]|\\\\[\\x01-\\x09\\x0b\\x0c\\x0e-\\x7f])+)\\])");
        if (!emailRegexp.exactMatch(ui->email->text().trimmed())) {
            ui->warningLabel->setText(tr("Enter valid email address"));
            ok->setEnabled(false);
            return;
        }
    }
}

void ShareFileDialog::on_notifyCheckBox_stateChanged(int arg1)
{
    ui->email->setEnabled(arg1);
    ui->emailLabel->setEnabled(arg1);
    validate();
}
