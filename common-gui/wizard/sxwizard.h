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

#ifndef SXWIZARD_H
#define SXWIZARD_H

#include <QWizard>
#include <QTranslator>

class SxWizardPage;
class WizardStartPage;
class WizardConnectToPage;
class WizardSetupCompletePage;

class SxWizard : public QWizard
{
    Q_OBJECT
public:
    explicit SxWizard(QWidget *parent = 0);
    ~SxWizard();
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override;
    bool configChanged() const;
    void saveSettings();

    QByteArray certFingerprint() const;
    void setCertFingerprint(QByteArray fingerprint);
    QByteArray secondaryCertFingerprint() const;
    void setSecondaryCertFingerprint(QByteArray fingerprint);
    QString volume() const;
    void setVolume(const QString &volume);
    QString sxCluster() const;
    void setSxCluster(const QString &sxCluster);
    QString sxAuth() const;
    void setSxAuth(const QString &sxAuth);
    QString sxAddress() const;
    void setSxAddress(const QString &sxAddress);
    bool useSsl() const;
    void setUseSsl(bool useSsl);
    int sslPort() const;
    void setSslPort(int sslPort);
    QString clusterUuid() const;
    QString oldClusterUuid() const;
    void setClusterUuid(const QString &clusterUuid);
    QString username() const;
    void setUsername(const QString &username);
    QString lastAddress() const;
    void setLastAddress(const QString &lastAddress);
    QString language() const;
    void setLanguage(QString language);
    QString vcluster() const;
    void setVcluster(const QString &vcluster);
    virtual QString setupCompleteText() const = 0;
    virtual QString applicationName() const = 0;
    virtual QString wwwDocs() const =0;
    virtual QString hardcodedCluster() const = 0;
    virtual QString hardcodedSxAuthd() const = 0;
    virtual QString hardcodedClusterDomain() const = 0;

private slots:
    void finishSetup();
    void slotSetMinimumHeight();

protected:
    virtual void _addPages() = 0;
    virtual void _restoreSettings() = 0;
    virtual void _storeSettings() = 0;
    virtual QString _oldClusterUUID() const = 0;
    void setConfigChanged(bool changed);

    SxWizardPage *pageStart();
    SxWizardPage *pageConnectTo();
    SxWizardPage *pagetComplete();

signals:
    void languageChanged(QString language);

public slots:
    void enableButtons(bool enabled);

private slots:
    void on_pageChanged(const int pageId);

private:
    WizardStartPage *m_wizardStartPage;
    WizardConnectToPage *m_wizardConnectToPage;
    WizardSetupCompletePage *m_wizardCompletePage;
    QByteArray m_certFingerprint;
    QByteArray m_secondaryCertFingerprint;
    QString m_sxCluster;
    QString m_sxAuth;
    QString m_sxVolume;
    QString m_sxAddress;
    bool m_useSsl;
    int m_sslPort;
    QString m_localPath;
    QString m_clusterUuid;
    QString m_username;
    QString m_lastAddress;
    QString m_language;
    bool mFirstDraw;
    QString m_vcluster;
    bool m_configChanged;

protected:
    void changeEvent(QEvent *e) override;
};



#endif // SXWIZARD_H
