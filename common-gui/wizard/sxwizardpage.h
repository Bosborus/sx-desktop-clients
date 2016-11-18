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

#ifndef SXWIZARDPAGE_H
#define SXWIZARDPAGE_H

#include <QWizardPage>
#include "sxwizard.h"
#include "sxcluster.h"
#include <QTimer>
#include "util.h"

class SxWizardPage : public QWizardPage
{
    Q_OBJECT
public:
    explicit SxWizardPage(SxWizard *wizard);
    virtual ~SxWizardPage();
    //bool validateConnection();
    //void setIsValid(bool isValid);
    //virtual void on_result(SXQuery *result) = 0;

protected slots:
    //void sxQueryResult(SXQuery *result);
    //void onTimeout();

protected:
    SxWizard *sxWizard() const;
    //bool testingConnection() const;

    QByteArray certFingerprint() const;
    void setCertFingerprint(QByteArray fingerprint);
    QByteArray secondaryCertFingerprint() const;
    void setSecondaryCertFingerprint(QByteArray fingerprint);
    QString volume() const;
    void setVolume(const QString &volume);
    QString localPath() const;
    void setLocalPath(const QString &localPath);
    QString sxCluster() const;
    void setSxCluster(const QString &sxCluster);
    QString sxAuth() const;
    void setSxAuth(const QString &sxAuth);
    QString sxVolume() const;
    void setSxVolume(const QString &sxVolume);
    QString sxAddress() const;
    void setSxAddress(const QString &sxAddress);
    bool useSsl() const;
    void setUseSsl(bool useSsl);
    int sslPort() const;
    void setSslPort(int sslPort);
    QString clusterUuid() const;
    QString oldClusterUuid() const;
    void setClusterUuid(const QString &uuid);
    QString username() const;
    void setUsername(const QString &username);
    QString lastAddress() const;
    void setLastAddress(const QString &lastAddress);
    QString vcluster() const;
    void setVcluster(const QString &vcluster);
    QHash<QString, QString> selectedVolumes() const;
    void setSelectedVolumes(const QHash<QString, QString> &selectedVolumes);

signals:
    void sig_enableButtons(bool enabled);

private:
    SxWizard *mWizard;
};

#endif // SXWIZARDPAGE_H
