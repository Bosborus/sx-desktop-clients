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

#ifndef VERSION_CHECK_H
#define VERSION_CHECK_H

#include <QObject>
#include <QTimer>
#include <QTemporaryFile>
#include <QNetworkRequest>
#include "downloaddialog.h"

class QNetworkAccessManager;
class QNetworkReply;

class VersionCheck: public QObject
{
    Q_OBJECT
    const int timeoutSec = 15;
    const int retrySec = 120;

public:
    static void initializeVersionCheck(const QString &applicationName, const QString &version, const QString &urlRepoRelease, const QString &urlRepoBeta, const QString &urlTemplateCheck, const QString &urlTemplateDownload, const QString &updateScriptName);
    static VersionCheck *instance();
    static const QString updateScriptName();
    void setEnabled(bool enabled);
    void setCheckingBeta(bool enabled);
    bool enabled() const;
    bool betaCheckingEnabled() const;
    void pause();
    void resume();
    bool initialCheck();
    void resetVersionCheck();
    void updateFailed();
    bool updateNow();
    void setParentWidget(QWidget *parent);
    QWidget *parentWidget() const;

    class SxVersion
    {
    public:
        SxVersion(QString version);
        bool isBeta() const;
        QString toString();
        bool operator > (const SxVersion &other);
        bool operator == (const SxVersion &other);
    private:
        QString m_version;
    };

public slots:
    bool checkNow();
    void checkVersionShowResult(bool checkBeta);

private slots:
    void replyFinished(QNetworkReply *reply);
    void downloadProgress(qint64, qint64);
    void onNetworkTimeout();
    void slotUpdateNow();

signals:
    void newVersionAvailable(const QString& version);
    void noNewVersion(bool showResult);
    void versionCheckFailed(bool showDialog, const QString& ver);
    void initialCheckFinished();
    void updateSuccessful();
    void signal_updateFailed();

private:
    VersionCheck();
    void writeVersionFile() const;
    void readVersionFile();
    void downloadUpdate(QString version, bool downloadFromBetaRepo);
    void _sendDownloadQuerry(QNetworkRequest request, DownloadDialog *d, int attempt);

private:
    QNetworkAccessManager *m_netAccMan;
    QList<QNetworkReply*> m_replies;
    bool m_enabled;
    bool m_initlalCheck;
    bool m_checkingBeta;
    bool m_showResult;
    QString m_availableVersion;
    QString m_minVersion;
    QString m_updateVersion;
    QString m_versionFile;
    bool m_downloadFromBetaRepo;
    QTimer m_timer;
    QTemporaryFile *m_tempfile;
    QString mUrlRelease;
    QString mUrlBeta;
    QString mUrlTemplateCheck;
    QString mUrlTemplateDownload;
    QString mApplicationName;
    QString mVersion;
    static QString mUpdateScriptName;
    QWidget *mParentWidget;
};
#endif
