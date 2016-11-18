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

#include "sxwizard.h"
#include "wizardstartpage.h"
#include "wizardconnecttopage.h"
#include "wizardsetupcompletepage.h"
#include <QAbstractButton>
#include <QDebug>
#include <QEvent>
#include <QPaintEvent>
#include <QLayout>
#include <QTemporaryDir>
#include <translations.h>
#include <QTimer>

SxWizard::SxWizard(QWidget *parent) :
    QWizard(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
{
    mFirstDraw = true;
    m_configChanged = false;

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    m_wizardStartPage = nullptr;
    m_wizardConnectToPage = nullptr;
    m_wizardCompletePage = nullptr;

    connect(this, &SxWizard::currentIdChanged, this, &SxWizard::on_pageChanged);
    setWizardStyle(QWizard::ClassicStyle);
    setMinimumWidth(600);
    setOptions(NoBackButtonOnStartPage | NoBackButtonOnLastPage | HaveHelpButton | NoCancelButton);
    setButtonText(HelpButton, tr("Cancel"));
    connect(this, &QWizard::helpRequested, this, &QWizard::close);
    connect(this, &QWizard::destroyed, this, &QWizard::deleteLater);
    QTimer::singleShot(0, this, SLOT(finishSetup()));
}

SxWizard::~SxWizard()
{
    foreach(int id, pageIds()) {
        QWizardPage* p = page(id);
        removePage(id);
        p->deleteLater();
    }
}

QByteArray SxWizard::certFingerprint() const
{
    return m_certFingerprint;
}

void SxWizard::setCertFingerprint(QByteArray fingerprint)
{
    m_certFingerprint = fingerprint;
}

QByteArray SxWizard::secondaryCertFingerprint() const
{
    return m_secondaryCertFingerprint;
}

void SxWizard::setSecondaryCertFingerprint(QByteArray fingerprint)
{
    m_secondaryCertFingerprint = fingerprint;
}

void SxWizard::on_pageChanged(const int pageId)
{
    auto cp = page(pageId);
    if (cp==m_wizardCompletePage)
    {
        button(FinishButton)->show();
        button(HelpButton)->hide();
    }
    else
    {
        if (button(FinishButton)->isVisible())
            button(FinishButton)->hide();
        if (!button(HelpButton)->isVisible())
            button(HelpButton)->show();
    }
    if (pageId != -1)
        QTimer::singleShot(1, this, SLOT(slotSetMinimumHeight()));
}

void SxWizard::enableButtons(bool enabled)
{
    button(BackButton)->setEnabled(enabled);
    button(CancelButton)->setEnabled(enabled);
}

void SxWizard::paintEvent(QPaintEvent *event)
{
    if (mFirstDraw)
    {
        mFirstDraw = false;
        QTimer *timer = new QTimer();
        timer->setSingleShot(true);
        auto lambda = [this, timer]() {
            timer->deleteLater();
            adjustSize();
        };
        connect(timer, &QTimer::timeout, lambda);
        timer->start(0);
        if (m_language != "en")
            emit languageChanged(m_language);
    }
    QWizard::paintEvent(event);
}

void SxWizard::finishSetup()
{
    _restoreSettings();
    _addPages();
    restart();
}

void SxWizard::slotSetMinimumHeight()
{
    auto size = sizeHint();
    setMinimumHeight(size.height());
}

void SxWizard::setConfigChanged(bool changed)
{
    m_configChanged = changed;
}

SxWizardPage *SxWizard::pageStart()
{
    if (m_wizardStartPage == nullptr)
        m_wizardStartPage = new WizardStartPage(this);
    return m_wizardStartPage;
}

SxWizardPage *SxWizard::pageConnectTo()
{
    if (m_wizardConnectToPage == nullptr)
        m_wizardConnectToPage = new WizardConnectToPage(this);
    return m_wizardConnectToPage;
}

SxWizardPage *SxWizard::pagetComplete()
{
    if (m_wizardCompletePage == nullptr) {
        m_wizardCompletePage = new WizardSetupCompletePage(this);
        connect(m_wizardConnectToPage, &SxWizardPage::sig_enableButtons, this, &SxWizard::enableButtons);
    }
    return m_wizardCompletePage;
}

void SxWizard::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange)
    {
        setButtonText(NextButton, tr("Next"));
        setButtonText(HelpButton, tr("Cancel"));
    }
    else
        QWidget::changeEvent(e);

}

QSize SxWizard::sizeHint() const
{
    QSize size = QWizard::sizeHint();
    WizardConnectToPage *ctp = qobject_cast<WizardConnectToPage*>(currentPage());
    if (ctp) {
        int padding = height() - ctp->height();
        QSize wizardHint = QWizard::sizeHint();
        QSize pageHint = ctp->sizeHint();
        wizardHint.setHeight(pageHint.height()+padding);
        size= wizardHint;
    }
    return size;
}

bool SxWizard::configChanged() const
{
    return m_configChanged;
}

void SxWizard::saveSettings()
{
    _storeSettings();
}

QString SxWizard::lastAddress() const
{
    return m_lastAddress;
}

void SxWizard::setLastAddress(const QString &lastAddress)
{
    m_lastAddress = lastAddress;
}

QString SxWizard::language() const
{
    return m_language;
}

void SxWizard::setLanguage(QString language)
{
    m_language = language;
    emit languageChanged(language);
}

QString SxWizard::vcluster() const
{
    return m_vcluster;
}

void SxWizard::setVcluster(const QString &vcluster)
{
    m_vcluster = vcluster;
}

QString SxWizard::username() const
{
    return m_username;
}

void SxWizard::setUsername(const QString &username)
{
    m_username = username;
}

QString SxWizard::clusterUuid() const
{
    return m_clusterUuid;
}

QString SxWizard::oldClusterUuid() const
{
    return _oldClusterUUID();
}

void SxWizard::setClusterUuid(const QString &clusterUuid)
{
    m_clusterUuid = clusterUuid;
}

int SxWizard::sslPort() const
{
    return m_sslPort;
}

void SxWizard::setSslPort(int sslPort)
{
    m_sslPort = sslPort;
}

bool SxWizard::useSsl() const
{
    return m_useSsl;
}

void SxWizard::setUseSsl(bool useSsl)
{
    m_useSsl = useSsl;
}

QString SxWizard::sxAddress() const
{
    return m_sxAddress;
}

void SxWizard::setSxAddress(const QString &sxAddress)
{
    m_sxAddress = sxAddress;
}

QString SxWizard::sxAuth() const
{
    return m_sxAuth;
}

void SxWizard::setSxAuth(const QString &sxAuth)
{
    m_sxAuth = sxAuth;
}

QString SxWizard::sxCluster() const
{
    return m_sxCluster;
}

void SxWizard::setSxCluster(const QString &sxCluster)
{
    m_sxCluster = sxCluster;
}
