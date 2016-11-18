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

#include "wizardstartpage.h"
#include "ui_wizardstartpage.h"
#include "translations.h"
#include "coloredframe.h"

#define ClassName "WizardStartPage"

WizardStartPage::WizardStartPage(SxWizard *wizard) :
    SxWizardPage(wizard),
    ui(new Ui::WizardStartPage)
{
    ui->setupUi(this);
    ui->versionLabel1->setText(QApplication::applicationVersion());
    ui->versionLabel1->setStyleSheet(QString("color: %1;").arg(ColoredFrame::versionTextColor()));
    ui->versionLabel0->setStyleSheet(QString("color: %1;").arg(ColoredFrame::versionTextColor()));

    if (isRetina())
        ui->logo->setPixmap(QPixmap(":/icons/systemicon@2x.png"));
}

WizardStartPage::~WizardStartPage()
{
    delete ui;
}

bool WizardStartPage::isComplete() const
{
    return true;
}

bool WizardStartPage::validatePage()
{
    return true;
}

void WizardStartPage::onLanguageChange(const QString &language)
{
    QString lang = Translations::instance()->languageCode(language);
    sxWizard()->setLanguage(lang);
}

void WizardStartPage::setupTexts()
{
    static const char* s_title = QT_TR_NOOP("Welcome to the %1 Setup Wizard");
    static const char* s_desc1 = QT_TR_NOOP("This wizard will guide you through the setup of %1.");
    static const char* s_desc2 = QT_TR_NOOP("You should have already received a username and password or a configuration file from your cluster administrator.");
    static const char* s_desc3 = QT_TR_NOOP("You can learn more about %1 at %2");
    static const char* s_language  = QT_TR_NOOP("Language");
    static const char* s_version  = QT_TR_NOOP("Version");

    ui->label_31->setText("<html><head/><body><p>" + tr(s_title).arg(sxWizard()->applicationName()) + "</p></body></html>");
    ui->label_32->setText(QString("<html><head/><body>"
                           "<p>"+tr(s_desc1).arg(sxWizard()->applicationName())+"</p>"
                           "<p>"+tr(s_desc2)+"</p>"
                           "<p>"+tr(s_desc3)+"</p>"
                           "</body></html>")
                          .arg(sxWizard()->applicationName())
                          .arg(QString("<a href=\"%1\"><span style=\" text-decoration: underline; color:#0000ff;\">%1</span></a>")
                               .arg(sxWizard()->wwwDocs())));

    ui->label->setText(tr(s_language));
    ui->versionLabel0->setText(tr(s_version));
}

void WizardStartPage::initializePage()
{
    ui->comboBox->clear();
    ui->comboBox->addItem("English");
    ui->comboBox->addItems(Translations::instance()->availableLanguages());
    ui->comboBox->setCurrentText(Translations::instance()->nativeLanguage(sxWizard()->language()));
    connect(ui->comboBox, &QComboBox::currentTextChanged, this, &WizardStartPage::onLanguageChange, Qt::UniqueConnection);
    setupTexts();
}

void WizardStartPage::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
    {
        setupTexts();
    }
    else
        QWidget::changeEvent(e);
}
