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

#include "certdialog.h"
#include "sxwizard.h"
#include "ui_wizardconnecttopage.h"
#include "wizardconnecttopage.h"
#include "changepassworddialog.h"

#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QMessageBox>
#include <QHostInfo>
#include <QNetworkInterface>
#include <QUrlQuery>
#include <QFileDialog>


#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent>

WizardConnectToPage::WizardConnectToPage(SxWizard *wizard) :
    SxWizardPage(wizard),
    ui(new Ui::WizardConnectToPage)
{
    ui->setupUi(this);
    ui->frame->setVisible(false);
    ui->advancedSettingsPane->hide();

    connect(ui->advancedSettings, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
    connect(ui->advancedSettings2, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
    connect(ui->advancedSettings3, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);

    if (isRetina()) {
        ui->logo->setPixmap(QPixmap(":/icons/systemicon@2x.png"));
        ui->label_4->setPixmap(QPixmap(":/icons/progress1of3@2x.png"));
    }
    initializeSignals();
    m_validDropDownConfig = false;

    if (!sxWizard()->hardcodedCluster().isEmpty() || !sxWizard()->hardcodedSxAuthd().isEmpty())
    {
        if (sxWizard()->hardcodedSxAuthd().isEmpty()) {
            ui->advancedSettings->setVisible(false);
            ui->sxCluster->setVisible(false);
            ui->label_sx_cluster->setVisible(false);
            ui->sxCluster->setText(sxWizard()->hardcodedCluster());
        }
        else if (sxWizard()->hardcodedCluster().isEmpty()) {
            ui->advancedSettings->setVisible(false);
            ui->enterpriseServer->setVisible(false);
            ui->label_3->setVisible(false);
            ui->enterpriseServer->setText(sxWizard()->hardcodedSxAuthd());
        }
        else {
            ui->sxCluster->setVisible(false);
            ui->label_sx_cluster->setVisible(false);
            ui->sxCluster->setText(sxWizard()->hardcodedCluster());
            ui->enterpriseServer->setVisible(false);
            ui->label_3->setVisible(false);
            ui->enterpriseServer->setText(sxWizard()->hardcodedSxAuthd());
        }
    }
    else if (!sxWizard()->hardcodedClusterDomain().isEmpty())
    {
        ui->advancedSettings->setVisible(false);
    }
    m_validated = false;
}

WizardConnectToPage::~WizardConnectToPage()
{
    delete ui;
}

bool WizardConnectToPage::validatePage()
{
    if (m_validated)
        return true;
    bool result = false;
    sxWizard()->button(QWizard::NextButton)->setEnabled(false);
    sxWizard()->button(QWizard::HelpButton)->setEnabled(false);
    sxWizard()->button(QWizard::BackButton)->setEnabled(false);
    sxWizard()->button(QWizard::CancelButton)->setEnabled(false);
    switch (ui->stackedWidget->currentIndex())
    {
    case 0:
        result = validateLoginSx();
        break;
    case 1:
        result = validateLoginEnterprise();
        break;
    case 2:
        result = validateLoginConfFile();
        break;
    }
    sxWizard()->button(QWizard::NextButton)->setEnabled(true);
    sxWizard()->button(QWizard::HelpButton)->setEnabled(true);
    sxWizard()->button(QWizard::BackButton)->setEnabled(true);
    sxWizard()->button(QWizard::CancelButton)->setEnabled(true);
    return result;

}

bool WizardConnectToPage::isComplete() const
{
    switch (ui->stackedWidget->currentIndex()) {
    case 0: {
        return(!ui->sxCluster->text().isEmpty()
               && !ui->sxUser->text().isEmpty()
               && !ui->sxPassword->text().isEmpty() );
    }
    case 1: {
        return(!ui->enterpriseServer->text().isEmpty()
               && !ui->enterpriseUser->text().isEmpty()
               && !ui->enterprisePassword->text().isEmpty()
               && !ui->enterpriseDevice->text().isEmpty());
    }
    case 2: {
        return m_validDropDownConfig;
    }

    }
    return false;
}

void WizardConnectToPage::onUrlDropped(SxUrl url)
{
    if (url.isValid()) {
        m_validDropDownConfig = true;
        ui->label_Cluster->setText(url.getClusterName());
        ui->label_Username->setText(url.getUsername());
        ui->label_Volume->setText(url.getVolume());
        setSxCluster(url.getClusterName());
        setCertFingerprint(url.getCertFingerprint());
        setSecondaryCertFingerprint(QByteArray{});
        setUsername(url.getUsername());
        setSxAddress(url.getNodeAddess());
        setSxAuth(url.getAuth());
        setUseSsl(url.getSslFlag());
        setSslPort(url.getPort());
    }
    else {
        clearForms(true, true, true);
        QMessageBox::warning(this, tr("Warning"), tr("The file doesn't contain a valid %1 configuration").arg(sxWizard()->applicationName()));
    }
    emit completeChanged();
}

void WizardConnectToPage::initializePage()
{
#ifndef NO_WHITELABEL
    #ifdef WHITELABEL_SPEICHERBOX
        ui->label_55->setText(tr("Connect to %1").arg("SpeicherBox.ch"));
    #else
        ui->label_55->setText(tr("Connect to Server"));
    #endif
#endif
    if (sxWizard()->hardcodedCluster().isEmpty() && sxWizard()->hardcodedSxAuthd().isEmpty()) {
        if (sxWizard()->hardcodedClusterDomain().isEmpty())
            ui->sxCluster->setText(sxCluster());
        else {
            QString cluster = sxCluster();
            if (cluster.endsWith("."+sxWizard()->hardcodedClusterDomain()))
            {
                int index = cluster.length() - sxWizard()->hardcodedClusterDomain().length() - 1;
                cluster = cluster.left(index);
                ui->sxCluster->setText(cluster);
            }
        }
    }
    ui->sxUser->setText(username());
    ui->sxAddress->setText(sxAddress());
    ui->encryptTraffic->setChecked(useSsl());
    ui->sxPort->setValue(sslPort());

    bool showAdvanced = false;
    if (sxWizard()->hardcodedClusterDomain().isEmpty() && sxWizard()->hardcodedSxAuthd().isEmpty()) {
        ui->buttonSxAuth->setChecked(true);
        on_buttonSxAuth_clicked(true);
        showAdvanced = !sxAddress().isEmpty() || sslPort() != 443 || !useSsl();
    }
    else {
        if (!sxWizard()->hardcodedSxAuthd().isEmpty())
            showAdvanced = true;
        ui->buttonEnterpriseAuth->setChecked(true);
        on_buttonEnterpriseAuth_clicked(true);
    }

    ui->advancedSettings->setChecked(showAdvanced);
    m_validated = false;
    QTimer::singleShot(1, this, SLOT(slotAdjustSize()));
}

void WizardConnectToPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
    {
        ui->retranslateUi(this);
    }
    else
        QWizardPage::changeEvent(event);
}

bool WizardConnectToPage::validateLoginSx()
{
    QString clusterName;
    if (sxWizard()->hardcodedCluster().isEmpty()) {
        if (sxWizard()->hardcodedClusterDomain().isEmpty())
            clusterName = ui->sxCluster->text();
        else
            clusterName = ui->sxCluster->text()+"."+sxWizard()->hardcodedClusterDomain();
        if (clusterName.startsWith("sx://"))
            clusterName = clusterName.mid(5);
    }
    else
        clusterName = sxWizard()->hardcodedCluster();
    QString initialAddress = ui->sxAddress->text();
    int port = ui->sxPort->value();
    bool ssl = ui->encryptTraffic->isChecked();
    QString uuid;
    QString errorMessage;
    if (!SxCluster::getClusterUUID(clusterName, initialAddress, ssl, port, uuid, errorMessage)) {
        if (errorMessage == "Unable to locate cluster nodes") {
            errorMessage = tr("%3 was unable to connect to '%1': %2").arg(clusterName).arg(QCoreApplication::translate("SxErrorMessage", errorMessage.toUtf8().constData())).arg(sxWizard()->applicationName());
        }
        QMessageBox::warning(this, sxWizard()->applicationName(), errorMessage);
        return false;
    }
    QString user = ui->sxUser->text();
    QString password = ui->sxPassword->text();
    QString token = deriveKey(user, password, uuid, 12);
    SxAuth auth(clusterName, initialAddress, ssl, port, token);

    auto checkCert = [this](QSslCertificate &cert, bool secondaryCert) -> bool{
        CertDialog dialog(cert, this);
        if (dialog.exec()) {
            if (secondaryCert)
                setSecondaryCertFingerprint(dialog.certFps());
            else
                setCertFingerprint(dialog.certFps());
            return true;
        }
        return false;
    };

    SxCluster *cluster = SxCluster::initializeCluster(auth, uuid.toUtf8(), checkCert, errorMessage);
    if (!cluster) {
        QMessageBox::warning(this, sxWizard()->applicationName(), tr("Unable to initialize cluster: %1").arg(QCoreApplication::translate("SxErrorMessage", errorMessage.toUtf8().constData())));
        return false;
    }
    setVcluster(cluster->userInfo().vcluster());
    delete cluster;
    setUsername(user);
    setSxAuth(token);
    setSxCluster(clusterName);
    setClusterUuid(uuid);
    setSxAddress(initialAddress);
    setUseSsl(ssl);
    setSslPort(port);
    return true;
}

bool WizardConnectToPage::validateLoginEnterprise()
{
    QString server = ui->enterpriseServer->text();
    if (!sxWizard()->hardcodedClusterDomain().isEmpty())
        server += "."+sxWizard()->hardcodedClusterDomain();
    QString user = ui->enterpriseUser->text();
    QString password = ui->enterprisePassword->text();
    QString device = ui->enterpriseDevice->text();

    auto checkCert = [this](const QSslCertificate &cert, bool secondaryCert) -> bool {
        CertDialog dialog(cert);
        if (dialog.exec() == QMessageBox::Yes) {
            if (secondaryCert)
                this->setSecondaryCertFingerprint(dialog.certFps());
            else
                this->setCertFingerprint(dialog.certFps());
            return true;
        }
        return false;
    };

    SxUrl sxUrl;
    QString errorMessage;
    if (!SxCluster::getEnterpriseAuth(server, user, password, device, checkCert, sxUrl, errorMessage)) {
        QMessageBox::warning(this, sxWizard()->applicationName(), errorMessage);
        return false;
    }

    QString clusterName = sxUrl.getClusterName();
    QString initialAddress = sxUrl.getNodeAddess();
    if (!initialAddress.isEmpty()) {
        QHostInfo info = QHostInfo::fromName(initialAddress);
        if (info.error() == QHostInfo::NoError) {
            foreach (auto a, info.addresses()) {
                if (a.toString()==initialAddress) {
                    initialAddress.clear();
                    break;
                }
            }
        }
    }
    setCertFingerprint(sxUrl.getCertFingerprint());
    setSecondaryCertFingerprint(QByteArray{});
    QString token = sxUrl.getAuth();
    int port = sxUrl.getPort();
    bool ssl = sxUrl.getSslFlag();
    QString uuid;

    if (!SxCluster::getClusterUUID(clusterName, initialAddress, ssl, port, uuid, errorMessage)) {
        QMessageBox::warning(this, sxWizard()->applicationName(), errorMessage);
        return false;
    }
    SxAuth auth(clusterName, initialAddress, ssl, port, token);
    SxCluster *cluster = SxCluster::initializeCluster(auth, uuid.toUtf8(), checkCert, errorMessage);
    if (!cluster) {
        QMessageBox::warning(this, sxWizard()->applicationName(), tr("Unable to initialize cluster: %1").arg(QCoreApplication::translate("SxErrorMessage", errorMessage.toUtf8().constData())));
        return false;
    }
    setVcluster(cluster->userInfo().vcluster());
    delete cluster;
    setUsername(user);
    setSxAuth(token);
    setSxCluster(clusterName);
    setClusterUuid(uuid);
    setSxAddress(initialAddress);
    setUseSsl(ssl);
    setSslPort(port);
    return true;
}

bool WizardConnectToPage::validateLoginConfFile()
{
    auto checkCert = [this](const QSslCertificate &cert, bool secondaryCert) -> bool {
        CertDialog dialog(cert);
        if (dialog.exec() == QMessageBox::Yes) {
            if (secondaryCert)
                this->setSecondaryCertFingerprint(dialog.certFps());
            else
                this->setCertFingerprint(dialog.certFps());
            return true;
        }
        return false;
    };
    QString clusterName = sxCluster();
    QString initialAddress = sxAddress();
    QString token = sxAuth();
    int port = sslPort();
    bool ssl = useSsl();
    QString uuid;
    QString errorMessage;

    if (!SxCluster::getClusterUUID(clusterName, initialAddress, ssl, port, uuid, errorMessage)) {
        QMessageBox::warning(this, sxWizard()->applicationName(), errorMessage);
        return false;
    }
    SxAuth auth(clusterName, initialAddress, ssl, port, token);
    SxCluster *cluster = SxCluster::initializeCluster(auth, uuid.toUtf8(), checkCert, errorMessage);
    if (!cluster) {
        if (errorMessage == "Invalid credentials" && !username().isEmpty()) {
            bool firstTime = true;
            while (true) {
                QString message = firstTime ? tr("This account has already been activated.") : tr("The provided password was incorrect.");
                message += "<br>" + tr("Please enter the password for user <b>%1</b>").arg(username());

                QLineEdit *passwordField = new QLineEdit();
                passwordField->setEchoMode(QLineEdit::Password);

                QPushButton *okButton = new QPushButton(tr("OK"));
                QPushButton *cancelButton = new QPushButton(tr("Cancel"));
                QHBoxLayout *buttonsLayout = new QHBoxLayout();
                buttonsLayout->addWidget(okButton);
                buttonsLayout->addWidget(cancelButton);

                QLabel *messageLabel = new QLabel(message);
                messageLabel->setTextFormat(Qt::RichText);

                QGridLayout *layout = new QGridLayout();
                layout->addWidget(messageLabel, 0,0,1,2);
                layout->addItem(new QSpacerItem(400, 10, QSizePolicy::Fixed, QSizePolicy::Fixed), 1, 0, 1, 2);
                layout->addWidget(new QLabel(tr("Password:")), 2,0);
                layout->addWidget(passwordField, 2, 1);
                layout->addItem(new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Expanding), 3, 0, 1, 2);
                layout->addLayout(buttonsLayout, 4, 0, 1, 2);

                QDialog d(wizard());
                d.setWindowTitle(tr("Enter password"));
                d.setLayout(layout);

                connect(okButton, &QPushButton::clicked, &d, &QDialog::accept);
                connect(cancelButton, &QPushButton::clicked, &d, &QDialog::reject);

                if (d.exec())
                {
                    token = deriveKey(username(), passwordField->text(), uuid);
                    SxAuth auth(clusterName, initialAddress, ssl, port, token);
                    SxCluster *cluster = SxCluster::initializeCluster(auth, uuid.toUtf8(), checkCert, errorMessage);
                    if (!cluster) {
                        if (errorMessage == "Invalid credentials")
                            firstTime = false;
                        else {
                            QMessageBox::warning(this, sxWizard()->applicationName(), tr("Unable to initialize cluster: %1").arg(QCoreApplication::translate("SxErrorMessage", errorMessage.toUtf8().constData())));
                            return false;
                        }
                    }
                    else
                        break;
                }
                else {
                    return false;
                }
            }
        }
        else {
            QMessageBox::warning(this, sxWizard()->applicationName(), tr("Unable to initialize cluster: %1").arg(QCoreApplication::translate("SxErrorMessage", errorMessage.toUtf8().constData())));
            return false;
        }
    }
    else if (!username().isEmpty()) {
        ChangePasswordDialog d(username(), cluster, true, wizard());
        if (d.exec() == QDialog::Accepted) {
            token = d.token();
        }
        else
            return false;
    }
    setVcluster(cluster->userInfo().vcluster());
    delete cluster;
    setSxAuth(token);
    setSxCluster(clusterName);
    setClusterUuid(uuid);
    setSxAddress(initialAddress);
    setUseSsl(ssl);
    setSslPort(port);
    return true;
}

void WizardConnectToPage::initializeSignals()
{
    connect(ui->sxCluster, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
    connect(ui->sxUser, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
    connect(ui->sxPassword, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);

    connect(ui->enterpriseServer, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
    connect(ui->enterpriseUser, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
    connect(ui->enterprisePassword, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);
    connect(ui->enterpriseDevice, &QLineEdit::textChanged, this, &QWizardPage::completeChanged);

    connect(ui->frame, &DropFrame::urlDropped, this, &WizardConnectToPage::onUrlDropped);
}

void WizardConnectToPage::clearForms(bool clearSxAuth, bool clearEnterprise, bool clearDropDown)
{
    if (clearSxAuth)
    {
        ui->sxCluster->clear();
        ui->sxUser->clear();
        ui->sxPassword->clear();
        ui->encryptTraffic->setChecked(true);
        ui->sxPort->setValue(443);
    }
    if (clearEnterprise)
    {
        ui->enterpriseServer->clear();
        ui->enterpriseUser->clear();
        ui->enterprisePassword->clear();
        ui->enterpriseDevice->clear();
    }
    if (clearDropDown)
    {
        m_validDropDownConfig = false;
        ui->label_Cluster->setText(tr("n/a"));
        ui->label_Username->setText(tr("n/a"));
        ui->label_Volume->setText(tr("n/a"));
    }
}

void WizardConnectToPage::on_encryptTraffic_stateChanged(int arg1)
{
        ui->sxPort->setValue(arg1 ? 443 : 80);
}

void WizardConnectToPage::on_buttonSxAuth_clicked(bool checked)
{
    if (checked) {
        ui->stackedWidget->setCurrentIndex(0);
        ui->stackedWidget_2->setCurrentIndex(0);
        ui->frame->setVisible(false);
        clearForms(false, true, true);
        if (!sxWizard()->hardcodedCluster().isEmpty())
            ui->sxCluster->setText(sxWizard()->hardcodedCluster());
        emit completeChanged();
        wizard()->adjustSize();
    }
    QTimer::singleShot(1, this, SLOT(slotAdjustSize()));
}

void WizardConnectToPage::on_buttonEnterpriseAuth_clicked(bool checked)
{
    if (checked) {
        ui->stackedWidget->setCurrentIndex(1);
        ui->stackedWidget_2->setCurrentIndex(1);
        ui->frame->setVisible(false);

        if (ui->enterpriseDevice->text().trimmed().isEmpty()) {
            QString host = QHostInfo::localHostName();
            const char *user = getenv("USER"); // hopefully all unices
            if(!user)
                user = getenv("USERNAME"); // windoze
            if(user && !host.isEmpty())
                ui->enterpriseDevice->setText(QString(user) + " on " + host);
        }

        clearForms(true, false, true);
        if (!sxWizard()->hardcodedSxAuthd().isEmpty())
            ui->enterpriseServer->setText(sxWizard()->hardcodedSxAuthd());
        emit completeChanged();
        wizard()->adjustSize();
        QTimer::singleShot(10, this, SLOT(slotAdjustSize()));
    }
    QTimer::singleShot(1, this, SLOT(slotAdjustSize()));
}

void WizardConnectToPage::on_buttonConfigFile_clicked(bool checked)
{
    if (checked) {
        ui->stackedWidget->setCurrentIndex(2);
        ui->stackedWidget_2->setCurrentIndex(2);
        ui->frame->setVisible(true);
        emit completeChanged();
        clearForms(true, true, false);
    }
    QTimer::singleShot(1, this, SLOT(slotAdjustSize()));
}

void WizardConnectToPage::on_pushButton_clicked()
{
    QFileDialog dlg;
    dlg.setDirectory(QDir::home());
    if (dlg.exec()) {
        QFile f(dlg.selectedFiles().first());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream fs(&f);
            QString line;
            while (line.trimmed().isEmpty()) {
                line = fs.readLine();
                if (line.isNull())
                    break;
            }
            f.close();
            SxUrl sxurl = SxUrl(line);
            onUrlDropped(sxurl);
        }
        else {
            QMessageBox::warning(this, tr("Warning"), tr("Unable to open the file"));
        }
    }
}

void WizardConnectToPage::advanedCheckboxClicked(bool checked)
{
    disconnect(ui->advancedSettings, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
    disconnect(ui->advancedSettings2, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
    disconnect(ui->advancedSettings3, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);

    ui->advancedSettings->setChecked(checked);
    ui->advancedSettings2->setChecked(checked);
    ui->advancedSettings3->setChecked(checked);

    ui->advancedSettingsPane->setVisible(checked);
    QTimer::singleShot(1, this, SLOT(slotAdjustSize()));

    connect(ui->advancedSettings, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
    connect(ui->advancedSettings2, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
    connect(ui->advancedSettings3, &QCheckBox::toggled, this, &WizardConnectToPage::advanedCheckboxClicked);
}

void WizardConnectToPage::slotAdjustSize()
{
    static int sIndex = -1;

    int index = ui->stackedWidget->currentIndex();
    if (index < 2)
    {
        if (sIndex == -1) {
            sIndex = index;
            index = sIndex == 0 ? 1 : 0;
            ui->stackedWidget->setCurrentIndex(index);
            ui->stackedWidget_2->setCurrentIndex(index);
            QTimer::singleShot(1, this, SLOT(slotAdjustSize()));
        }
        else
        {
            ui->stackedWidget->setCurrentIndex(sIndex);
            ui->stackedWidget_2->setCurrentIndex(sIndex);
            sIndex = -1;
        }
    }
    else
        sIndex = -1;
    wizard()->adjustSize();
}

QSize WizardConnectToPage::sizeHint() const
{
    int padding = height() - ui->scrollArea->height();
    QSize hint = QWizardPage::sizeHint();
    hint.setHeight(padding+ui->scrollAreaWidgetContents->sizeHint().height());
    return hint;
}
